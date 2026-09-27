/*
 * Codec2 HAL service for the sm8150 V4L2 video encoder.
 *
 * Structure follows external/v4l2_codec2/service/service.cpp, with one
 * deliberate difference: that service registers over AIDL only, and on this
 * device an AIDL-only Codec2 service is invisible.
 *
 * Codec2Client picks exactly one transport for the whole framework, in
 * Codec2Client::CacheServiceNames() (frameworks/av/media/codec2/hal/client/
 * client.cpp:2635), gated on c2_aidl::utils::IsSelected(). That resolves to
 * IsCodec2AidlHalSelected() (frameworks/av/media/codec2/hal/common/
 * HalSelection.cpp:31), which returns false unless ro.vendor.api_level >= 202404
 * *and* the property media.c2.hal.selection is set to "aidl" -- it defaults to
 * "hidl" (HalSelection.cpp:57). raphael ships PRODUCT_SHIPPING_API_LEVEL 30
 * (device/xiaomi/sm8150-common/msmnile.mk:14) and sets no such property, so the
 * framework enumerates HIDL instances and an AIDL registration would never be
 * listed. Hence HIDL @1.2 here.
 *
 * The existing prebuilt vendor.qti.media.c2@1.0-service registers
 * android.hardware.media.c2@1.0::IComponentStore/{default,software}. This
 * service uses the instance name "default1" so the two coexist:
 * the framework compatibility matrix only allows "software",
 * "default[0-9]*" and "vendor[0-9]*_software" for this HAL
 * (compatibility_matrix.8.xml:345-352). The QTI blob claims "default"
 * and "software", so "default1" is the first free permitted name.
 */

#include <signal.h>

#include <string>

#include <C2Component.h>
#include <binder/ProcessState.h>
#include <codec2/hidl/1.2/ComponentStore.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>
#include <csignal>
#include <linux/seccomp.h>
#include <minijail.h>

#undef LOG_TAG
#define LOG_TAG "sm8150-c2-venc-service"

namespace android {
// Defined in C2VencStore.cpp. Declared here rather than in a header because
// this service and the store are a single module and nothing else needs it.
std::shared_ptr<C2ComponentStore> CreateC2VencStore();
}  // namespace android

// Absolute on-device path of the prebuilt_etc module
// "android.hardware.media.c2-default-seccomp_policy"
// (frameworks/av/media/codec2/hal/services/Android.bp:81). This service is
// listed as `required:` on that module so the policy is guaranteed present.
static constexpr char kBaseSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/"
        "android.hardware.media.c2-default-seccomp_policy";

// Optional add-on policy. Does not exist by default; SetUpMinijail tolerates
// its absence. If the msm_vidc ioctls trip seccomp, extra syscalls go here.
static constexpr char kExtSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/"
        "android.hardware.media.c2-extended-seccomp_policy";

// Matches AOSP's vendor service. Extra threads are needed because a stacked
// IPC sequence can alternate binder and hwbinder calls (b/35283480), and
// because a blocking operation on one codec instance must not stall another.
static constexpr int kThreadCount = 8;


// Diagnostic: a seccomp kill arrives as SIGSYS with the offending syscall in
// si_syscall, but init only reports "received SIGSYS" and no tombstone is
// written, so the number is otherwise invisible. Log it, then restore the
// default disposition and re-raise so the process still dies exactly as it
// would have. This does not relax the sandbox.
static void logSeccompKill(int sig, siginfo_t *info, void *) {
    if (info != nullptr && info->si_code == SYS_SECCOMP) {
        ALOGE("SECCOMP blocked syscall %d (arch 0x%x) -- add it to "
              "android.hardware.media.c2-extended-seccomp_policy",
              info->si_syscall, info->si_arch);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static void installSeccompReporter() {
    struct sigaction sa = {};
    sa.sa_sigaction = logSeccompKill;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSYS, &sa, nullptr);
}

int main(int /*argc*/, char ** /*argv*/) {
    installSeccompReporter();

    ALOGD("sm8150 Codec2 encoder service starting...");

    // A dead client must not take the HAL down with it.
    signal(SIGPIPE, SIG_IGN);
    android::SetUpMinijail(kBaseSeccompPolicyPath, kExtSeccompPolicyPath);

    // vndbinder, not the default /dev/binder: this is a vendor process and the
    // buffer pool traffic it does is vendor-to-vendor.
    android::ProcessState::initWithDriver("/dev/vndbinder");
    android::ProcessState::self()->startThreadPool();
    android::hardware::configureRpcThreadpool(kThreadCount, true /*callerWillJoin*/);

    {
        using ::android::hardware::media::c2::V1_2::IComponentStore;
        using ::android::hardware::media::c2::V1_2::utils::ComponentStore;

        // ComponentStore's constructor calls SetPreferredCodec2ComponentStore(),
        // which is what makes the allocator usage mapping in C2VencStore's
        // Interface take effect for this process.
        android::sp<IComponentStore> store = new ComponentStore(android::CreateC2VencStore());

        constexpr char const *kInstanceName = "default1";
        if (store->registerAsService(kInstanceName) != android::OK) {
            ALOGE("cannot register IComponentStore as \"%s\"", kInstanceName);
        } else {
            ALOGD("IComponentStore registered as \"%s\"", kInstanceName);
        }
    }

    android::hardware::joinRpcThreadpool();
    ALOGD("sm8150 Codec2 encoder service shutdown.");
    return 0;
}
