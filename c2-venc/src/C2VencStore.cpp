/*
 * C2ComponentStore for the sm8150 V4L2 video encoder.
 *
 * This store exposes exactly one component, "c2.sm8150.avc.encoder", and is
 * handed to the Codec2 HAL wrapper by service.cpp. It is deliberately
 * self-contained: it does not depend on external/v4l2_codec2's
 * libv4l2_codec2_components, because that library's ComponentStore is bound to
 * v4l2_codec2's VideoCodec enum and V4L2ComponentName validation, neither of
 * which apply here.
 *
 * Why a store at all rather than just a CreateCodec2Factory() entry point:
 * the CreateCodec2Factory()/DestroyCodec2Factory() convention only works for
 * components that a *dlopen-ing* store loads. In this tree the only such store
 * is C2PlatformComponentStore (frameworks/av/media/codec2/vndk/C2Store.cpp),
 * which is the software/platform store -- it has a hardcoded component list
 * and no vendor plugin hook (there is no GetCodec2VendorComponentStore() in
 * this tree). The device's actual vendor store lives inside the prebuilt
 * vendor.qti.media.c2@1.0-service blob, which we cannot add components to.
 * So a vendor component needs its own store plus its own HAL service, exactly
 * as external/v4l2_codec2 does.
 */

#include <cutils/properties.h>
#include <string.h>

#include <memory>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <C2.h>
#include <C2Component.h>
#include <C2ComponentFactory.h>
#include <C2Config.h>
#include <log/log.h>
#include <media/stagefright/foundation/MediaDefs.h>
#include <util/C2InterfaceHelper.h>

#undef LOG_TAG
#define LOG_TAG "C2VencStore"

namespace android {

// Defined in C2VencComponent.cpp.
//
// C2VencComponent::IntfImpl is only forward-declared in C2VencComponent.h and
// defined in C2VencComponent.cpp, so this translation unit cannot construct
// the interface itself. The component's own TU exports the factory instead.
// See INTEGRATION-NOTES.md for the block that has to be appended to
// C2VencComponent.cpp to provide this.
std::unique_ptr<C2ComponentFactory> CreateC2VencFactory(
        const std::string &name, std::shared_ptr<C2ReflectorHelper> reflector);

namespace {

constexpr char kStoreName[] = "default1";
constexpr char kAvcEncoderName[] = "c2.sm8150.avc.encoder";
constexpr char kHevcEncoderName[] = "c2.sm8150.hevc.encoder";

bool isOurs(const C2String &name) {
    return name == kAvcEncoderName || name == kHevcEncoderName;
}

// Lower rank wins in MediaCodecList. C2PlatformComponentStore gives every
// software video component rank 512 (C2Store.cpp:1110), so anything below that
// is preferred over c2.android.avc.encoder. 256 leaves room both above (for a
// future fallback) and below (should a QTI component ever need to outrank us).
// Lower rank wins in Codec2. 256 put us behind c2.qti.avc.encoder, so the
// QTI blob was still chosen for every video/avc encode and this component
// never ran. 0 makes us the preferred AVC encoder; the QTI entry stays in
// media_codecs_c2.xml so it is still reachable by name.
// Lower rank wins in Codec2. While this component is still in bring-up it must
// NOT be the default AVC encoder: at rank 0 it is preferred for everything,
// including the camera, so any defect here breaks video recording device-wide.
//
// Default 256 leaves c2.qti.avc.encoder in charge (the known-good path).
// Set persist.vendor.c2venc.prefer=1 and reboot to put this component in front
// for testing. Flip the default once it is proven.
static uint32_t componentRank() {
    char value[PROPERTY_VALUE_MAX] = {0};
    property_get("persist.vendor.c2venc.prefer", value, "0");
    return (value[0] == '1') ? 0u : 256u;
}

}  // namespace

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

class C2VencStore : public C2ComponentStore {
public:
    C2VencStore()
        : mReflector(std::make_shared<C2ReflectorHelper>()),
          mInterface(mReflector) {}

    ~C2VencStore() override = default;

    C2String getName() const override { return kStoreName; }

    c2_status_t createComponent(C2String name,
                                std::shared_ptr<C2Component> *const component) override {
        ALOGV("%s(%s)", __func__, name.c_str());
        if (!isOurs(name)) {
            ALOGD("%s: unknown component \"%s\"", __func__, name.c_str());
            return C2_NOT_FOUND;
        }

        C2ComponentFactory *factory = getFactory(name);
        if (factory == nullptr) {
            ALOGE("%s: no factory for \"%s\"", __func__, name.c_str());
            return C2_CORRUPTED;
        }

        component->reset();
        c2_status_t err = factory->createComponent(0, component);
        ALOGI("%s(\"%s\") -> %d", __func__, name.c_str(), (int)err);
        return err;
    }

    c2_status_t createInterface(
            C2String name, std::shared_ptr<C2ComponentInterface> *const interface) override {
        ALOGV("%s(%s)", __func__, name.c_str());
        if (!isOurs(name)) {
            ALOGD("%s: unknown component \"%s\"", __func__, name.c_str());
            return C2_NOT_FOUND;
        }

        C2ComponentFactory *factory = getFactory(name);
        if (factory == nullptr) {
            ALOGE("%s: no factory for \"%s\"", __func__, name.c_str());
            return C2_CORRUPTED;
        }

        interface->reset();
        c2_status_t err = factory->createInterface(0, interface);
        ALOGI("%s(\"%s\") -> %d", __func__, name.c_str(), (int)err);
        return err;
    }

    std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() override {
        std::lock_guard<std::mutex> lock(mLock);
        if (mTraits.empty()) {
            for (const auto &entry : {std::make_pair(kAvcEncoderName, MEDIA_MIMETYPE_VIDEO_AVC),
                                      std::make_pair(kHevcEncoderName, MEDIA_MIMETYPE_VIDEO_HEVC)}) {
                auto traits = std::make_shared<C2Component::Traits>();
                traits->name = entry.first;
                traits->domain = C2Component::DOMAIN_VIDEO;
                traits->kind = C2Component::KIND_ENCODER;
                traits->rank = componentRank();
                traits->mediaType = entry.second;
                mTraits.push_back(traits);
                ALOGI("listComponents: advertising %s (rank %u)", entry.first,
                      (unsigned)componentRank());
            }
        }
        return mTraits;
    }

    c2_status_t copyBuffer(std::shared_ptr<C2GraphicBuffer> /*src*/,
                           std::shared_ptr<C2GraphicBuffer> /*dst*/) override {
        // Optional in C2ComponentStore; nothing in the framework requires a
        // vendor store to implement it, and the engine has no blit path.
        return C2_OMITTED;
    }

    // The store-level parameters below are not decoration: C2AllocatorIon and
    // C2DmaBufAllocator query the *preferred* component store for "ion-usage"
    // / "dmabuf-usage" to turn a C2MemoryUsage into a heap. The HIDL
    // ComponentStore wrapper installs us as the preferred store for this
    // process in its constructor, so if these are absent every allocation in
    // the HAL process falls back to whatever the allocator defaults to.
    c2_status_t query_sm(
            const std::vector<C2Param *> &stackParams,
            const std::vector<C2Param::Index> &heapParamIndices,
            std::vector<std::unique_ptr<C2Param>> *const heapParams) const override {
        return mInterface.query(stackParams, heapParamIndices, C2_MAY_BLOCK, heapParams);
    }

    c2_status_t config_sm(
            const std::vector<C2Param *> &params,
            std::vector<std::unique_ptr<C2SettingResult>> *const failures) override {
        return mInterface.config(params, C2_MAY_BLOCK, failures);
    }

    std::shared_ptr<C2ParamReflector> getParamReflector() const override { return mReflector; }

    c2_status_t querySupportedParams_nb(
            std::vector<std::shared_ptr<C2ParamDescriptor>> *const params) const override {
        return mInterface.querySupportedParams(params);
    }

    c2_status_t querySupportedValues_sm(
            std::vector<C2FieldSupportedValuesQuery> &fields) const override {
        return mInterface.querySupportedValues(fields, C2_MAY_BLOCK);
    }

private:
    // Mirrors the store interface in frameworks/av/media/codec2/hal/services/
    // vendor.cpp, which is AOSP's own template for a vendor Codec2 service.
    class Interface : public C2InterfaceHelper {
    public:
        explicit Interface(const std::shared_ptr<C2ReflectorHelper> &helper)
            : C2InterfaceHelper(helper) {
            setDerivedInstance(this);

            addParameter(DefineParam(mIonUsageInfo, "ion-usage")
                                 .withDefault(new C2StoreIonUsageInfo())
                                 .withFields({
                                         C2F(mIonUsageInfo, usage).flags({C2MemoryUsage::CPU_READ |
                                                                          C2MemoryUsage::CPU_WRITE}),
                                         C2F(mIonUsageInfo, capacity)
                                                 .inRange(0, UINT32_MAX, 1024),
                                         C2F(mIonUsageInfo, heapMask).any(),
                                         C2F(mIonUsageInfo, allocFlags).flags({}),
                                         C2F(mIonUsageInfo, minAlignment).equalTo(0),
                                 })
                                 .withSetter(SetIonUsage)
                                 .build());

            addParameter(DefineParam(mDmaBufUsageInfo, "dmabuf-usage")
                                 .withDefault(C2StoreDmaBufUsageInfo::AllocShared(128))
                                 .withFields({
                                         C2F(mDmaBufUsageInfo, m.usage)
                                                 .flags({C2MemoryUsage::CPU_READ |
                                                         C2MemoryUsage::CPU_WRITE}),
                                         C2F(mDmaBufUsageInfo, m.capacity)
                                                 .inRange(0, UINT32_MAX, 1024),
                                         C2F(mDmaBufUsageInfo, m.allocFlags).flags({}),
                                         C2F(mDmaBufUsageInfo, m.heapName).any(),
                                 })
                                 .withSetter(SetDmaBufUsage)
                                 .build());
        }

        ~Interface() override = default;

    private:
        // sm8150 runs a legacy (non-DMA-BUF-heaps) ION on this kernel and
        // venc_dev picks its own heap ids inside alloc_map_ion_memory(), so
        // the store does not need to constrain the mask here.
        static C2R SetIonUsage(bool /*mayBlock*/, C2P<C2StoreIonUsageInfo> &me) {
            me.set().heapMask = ~0;
            me.set().allocFlags = 0;
            me.set().minAlignment = 0;
            return C2R::Ok();
        }

        static C2R SetDmaBufUsage(bool /*mayBlock*/, C2P<C2StoreDmaBufUsageInfo> &me) {
            strncpy(me.set().m.heapName, "system", me.v.flexCount());
            me.set().m.allocFlags = 0;
            return C2R::Ok();
        }

        std::shared_ptr<C2StoreIonUsageInfo> mIonUsageInfo;
        std::shared_ptr<C2StoreDmaBufUsageInfo> mDmaBufUsageInfo;
    };

    // Factories are built lazily and cached, one per component name: cheap to
    // construct, but the reflector they capture must be the store's own, so
    // that params the component describes are reflectable over the HAL.
    C2ComponentFactory *getFactory(const C2String &name) {
        std::lock_guard<std::mutex> lock(mLock);
        auto it = mFactories.find(name);
        if (it == mFactories.end()) {
            std::unique_ptr<C2ComponentFactory> factory = CreateC2VencFactory(name, mReflector);
            if (factory == nullptr) {
                ALOGE("failed to create factory for %s", name.c_str());
                return nullptr;
            }
            it = mFactories.emplace(name, std::move(factory)).first;
        }
        return it->second.get();
    }

    std::shared_ptr<C2ReflectorHelper> mReflector;
    Interface mInterface;

    std::mutex mLock;
    std::map<C2String, std::unique_ptr<C2ComponentFactory>> mFactories;
    std::vector<std::shared_ptr<const C2Component::Traits>> mTraits;
};

// Entry point used by service.cpp. The store is a process singleton -- the
// HIDL wrapper takes a shared_ptr and would otherwise happily be handed two
// different reflectors for the same component.
std::shared_ptr<C2ComponentStore> CreateC2VencStore() {
    static std::mutex sMutex;
    static std::weak_ptr<C2ComponentStore> sStore;

    std::lock_guard<std::mutex> lock(sMutex);
    std::shared_ptr<C2ComponentStore> store = sStore.lock();
    if (store != nullptr) {
        return store;
    }

    store = std::make_shared<C2VencStore>();
    sStore = store;
    return store;
}

}  // namespace android
