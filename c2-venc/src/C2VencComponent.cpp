/*
 * See C2VencComponent.h. The engine (venc_dev) owns all V4L2 behaviour; this
 * file is the translation layer between Codec2's work items and the engine's
 * OMX-shaped buffer calls.
 *
 * Completion is deferred, not serialised: process() submits and returns, and
 * the engine's message thread calls finish() for the matching frameIndex when
 * the encoder is done with it. Blocking in process() would idle the hardware
 * for a full frame each time.
 */

#include "C2VencComponent.h"

#include <chrono>

#include <inttypes.h>
#include <string.h>

#include <sys/mman.h>

#include <libyuv.h>

#include <C2AllocatorGralloc.h>
#include <C2AllocatorIon.h>
#include <C2ComponentFactory.h>
#include <C2Debug.h>
#include <C2PlatformSupport.h>
#include <Codec2Mapper.h>
#include <log/log.h>

#include <media/msm_media_info.h>

#include "video_encoder_device_v4l2.h"
// For struct venc_msg / VEN_MSG_* only; the OMX class it declares is unused.
#include "omx_video_base.h"

#undef LOG_TAG
#define LOG_TAG "C2VencComponent"

namespace android {

namespace {
constexpr char COMPONENT_NAME_AVC[] = "c2.sm8150.avc.encoder";
constexpr char COMPONENT_NAME_HEVC[] = "c2.sm8150.hevc.encoder";
// Spelled out rather than pulled from MediaDefs.h: they are the only symbols
// we wanted from libstagefright_foundation, and a string literal is not worth
// a shared-library dependency.
constexpr char MIME_AVC[] = "video/avc";
constexpr char MIME_HEVC[] = "video/hevc";
constexpr uint32_t kDefaultBitrate = 20000000;
// venc_dev's own default is 29; the driver refuses much larger GOPs.
constexpr uint32_t kMaxPFrames = 255;  // 20 Mbps, matches CamcorderProfile 1080p
}  // namespace

// ---------------------------------------------------------------------------
// Interface
// ---------------------------------------------------------------------------

class C2VencComponent::IntfImpl : public SimpleInterface<void>::BaseParams {
public:
    IntfImpl(const std::shared_ptr<C2ReflectorHelper> &helper, Codec codec)
        : SimpleInterface<void>::BaseParams(helper,
                                            codec == Codec::HEVC ? COMPONENT_NAME_HEVC
                                                                 : COMPONENT_NAME_AVC,
                                            C2Component::KIND_ENCODER,
                                            C2Component::DOMAIN_VIDEO,
                                            codec == Codec::HEVC ? MIME_HEVC : MIME_AVC),
          mCodec(codec) {
        noPrivateBuffers();
        noInputReferences();
        noOutputReferences();
        noTimeStretch();
        setDerivedInstance(this);

        // The engine consumes graphic buffers by fd. CPU_READ is advertised on
        // purpose: QTI gralloc never allocates UBWC for CPU-readable buffers,
        // and the engine is configured for linear NV12, so a camera that would
        // otherwise pick UBWC for its video stream can't hand us compressed
        // frames. It also lets checkYuvLayout() map the first frame.
        addParameter(DefineParam(mUsage, C2_PARAMKEY_INPUT_STREAM_USAGE)
                             .withConstValue(new C2StreamUsageTuning::input(
                                     0u, (uint64_t)C2AndroidMemoryUsage::HW_CODEC_READ |
                                                 C2MemoryUsage::CPU_READ))
                             .build());

        addParameter(DefineParam(mSize, C2_PARAMKEY_PICTURE_SIZE)
                             .withDefault(new C2StreamPictureSizeInfo::input(0u, 176, 144))
                             .withFields({
                                     C2F(mSize, width).inRange(96, 4096, 2),
                                     C2F(mSize, height).inRange(96, 4096, 2),
                             })
                             .withSetter(SizeSetter)
                             .build());

        addParameter(DefineParam(mFrameRate, C2_PARAMKEY_FRAME_RATE)
                             .withDefault(new C2StreamFrameRateInfo::output(0u, 30.))
                             .withFields({C2F(mFrameRate, value).greaterThan(0.)})
                             .withSetter(Setter<decltype(*mFrameRate)>::StrictValueWithNoDeps)
                             .build());

        addParameter(DefineParam(mBitrate, C2_PARAMKEY_BITRATE)
                             .withDefault(new C2StreamBitrateInfo::output(0u, kDefaultBitrate))
                             .withFields({C2F(mBitrate, value).inRange(4096, 160000000)})
                             .withSetter(Setter<decltype(*mBitrate)>::StrictValueWithNoDeps)
                             .build());

        addParameter(DefineParam(mSyncFramePeriod, C2_PARAMKEY_SYNC_FRAME_INTERVAL)
                             .withDefault(new C2StreamSyncFrameIntervalTuning::output(0u, 1000000))
                             .withFields({C2F(mSyncFramePeriod, value).any()})
                             .withSetter(Setter<decltype(*mSyncFramePeriod)>::StrictValueWithNoDeps)
                             .build());

        // Profile/level vocabulary differs per codec; the setter clamps to the
        // codec's own default so a wrong-codec value from a client cannot leak
        // through to venc_set_param().
        if (codec == Codec::HEVC) {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                            .withDefault(new C2StreamProfileLevelInfo::output(
                                    0u, PROFILE_HEVC_MAIN, LEVEL_HEVC_MAIN_4_1))
                            .withFields({
                                    C2F(mProfileLevel, profile).oneOf({PROFILE_HEVC_MAIN}),
                                    C2F(mProfileLevel, level)
                                            .oneOf({LEVEL_HEVC_MAIN_3, LEVEL_HEVC_MAIN_3_1,
                                                    LEVEL_HEVC_MAIN_4, LEVEL_HEVC_MAIN_4_1,
                                                    LEVEL_HEVC_MAIN_5, LEVEL_HEVC_MAIN_5_1}),
                            })
                            .withSetter(HevcProfileLevelSetter, mSize, mFrameRate, mBitrate)
                            .build());
        } else {
            addParameter(
                    DefineParam(mProfileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                            .withDefault(new C2StreamProfileLevelInfo::output(
                                    0u, PROFILE_AVC_CONSTRAINED_BASELINE, LEVEL_AVC_4_1))
                            .withFields({
                                    C2F(mProfileLevel, profile)
                                            .oneOf({PROFILE_AVC_CONSTRAINED_BASELINE,
                                                    PROFILE_AVC_BASELINE, PROFILE_AVC_MAIN,
                                                    PROFILE_AVC_HIGH}),
                                    C2F(mProfileLevel, level)
                                            .oneOf({LEVEL_AVC_3, LEVEL_AVC_3_1, LEVEL_AVC_3_2,
                                                    LEVEL_AVC_4, LEVEL_AVC_4_1, LEVEL_AVC_4_2,
                                                    LEVEL_AVC_5, LEVEL_AVC_5_1}),
                            })
                            .withSetter(ProfileLevelSetter, mSize, mFrameRate, mBitrate)
                            .build());
        }
    }

    static C2R SizeSetter(bool mayBlock, const C2P<C2StreamPictureSizeInfo::input> &oldMe,
                          C2P<C2StreamPictureSizeInfo::input> &me) {
        (void)mayBlock;
        C2R res = C2R::Ok();
        if (!me.F(me.v.width).supportsAtAll(me.v.width)) {
            res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.width)));
            me.set().width = oldMe.v.width;
        }
        if (!me.F(me.v.height).supportsAtAll(me.v.height)) {
            res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.height)));
            me.set().height = oldMe.v.height;
        }
        return res;
    }

    static C2R ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::output> &me,
                                  const C2P<C2StreamPictureSizeInfo::input> &size,
                                  const C2P<C2StreamFrameRateInfo::output> &frameRate,
                                  const C2P<C2StreamBitrateInfo::output> &bitrate) {
        (void)mayBlock;
        (void)size;
        (void)frameRate;
        (void)bitrate;
        if (!me.F(me.v.profile).supportsAtAll(me.v.profile)) {
            me.set().profile = PROFILE_AVC_CONSTRAINED_BASELINE;
        }
        if (!me.F(me.v.level).supportsAtAll(me.v.level)) {
            me.set().level = LEVEL_AVC_4_1;
        }
        return C2R::Ok();
    }

    static C2R HevcProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::output> &me,
                                      const C2P<C2StreamPictureSizeInfo::input> &size,
                                      const C2P<C2StreamFrameRateInfo::output> &frameRate,
                                      const C2P<C2StreamBitrateInfo::output> &bitrate) {
        (void)mayBlock;
        (void)size;
        (void)frameRate;
        (void)bitrate;
        if (!me.F(me.v.profile).supportsAtAll(me.v.profile)) {
            me.set().profile = PROFILE_HEVC_MAIN;
        }
        if (!me.F(me.v.level).supportsAtAll(me.v.level)) {
            me.set().level = LEVEL_HEVC_MAIN_4_1;
        }
        return C2R::Ok();
    }

    Codec getCodec() const { return mCodec; }
    uint32_t getWidth() const { return mSize->width; }
    uint32_t getHeight() const { return mSize->height; }
    uint32_t getBitrate() const { return mBitrate->value; }
    float getFrameRate() const { return mFrameRate->value; }
    uint32_t getSyncFramePeriod() const { return mSyncFramePeriod->value; }
    std::shared_ptr<C2StreamProfileLevelInfo::output> getProfileLevel() const {
        return mProfileLevel;
    }

private:
    const Codec mCodec;
    std::shared_ptr<C2StreamUsageTuning::input> mUsage;
    std::shared_ptr<C2StreamPictureSizeInfo::input> mSize;
    std::shared_ptr<C2StreamFrameRateInfo::output> mFrameRate;
    std::shared_ptr<C2StreamBitrateInfo::output> mBitrate;
    std::shared_ptr<C2StreamSyncFrameIntervalTuning::output> mSyncFramePeriod;
    std::shared_ptr<C2StreamProfileLevelInfo::output> mProfileLevel;
};

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

C2VencComponent::C2VencComponent(const char *name, c2_node_id_t id,
                                 const std::shared_ptr<IntfImpl> &intfImpl)
    : SimpleC2Component(std::make_shared<SimpleInterface<IntfImpl>>(name, id, intfImpl)),
      mIntf(intfImpl),
      mDev(nullptr),
      mEngineOpened(false),
      mStreaming(false),
      mSignalledError(false),
      mAsyncThreadRunning(false),
      mSawInputEos(false),
      mLoggedInputFormat(false),
      mYuvChecked(false), mYuvCopy(false), mYuvNv21(false),
      mConversionNeeded(false),
      mConvSrcFormat(0),
      mStageSize(0),
      mStageYStride(0), mStageYScanlines(0), mStageUvStride(0),
      mConvGw(0), mConvGh(0), mConvFmt(0), mConvStride(0),
      mOutputsSeeded(false),
      mInNext(0),
      mInFlightCount(0),
      mInBufCount(0),
      mInBufSize(0),
      mOutBufCount(0),
      mOutBufSize(0) {}

OMX_VIDEO_CODINGTYPE C2VencComponent::omxCoding() const {
    return mIntf->getCodec() == Codec::HEVC ? OMX_VIDEO_CodingHEVC : OMX_VIDEO_CodingAVC;
}

C2VencComponent::~C2VencComponent() {
    closeEngine();
}

bool C2VencComponent::is_secure_session(void) {
    return false;
}

c2_status_t C2VencComponent::openEngine() {
    if (mEngineOpened) {
        return C2_OK;
    }

    // venc_dev binds to the host base, not the component; the cast has to be
    // explicit because C2VencComponent has two bases.
    mDev = new (std::nothrow) venc_dev(static_cast<C2VencHost *>(this));
    if (mDev == nullptr) {
        ALOGE("failed to allocate venc_dev");
        return C2_NO_MEMORY;
    }

    // The engine takes the OMX coding enum, not a mime. OMX_VIDEO_CodingHEVC
    // selects V4L2_PIX_FMT_HEVC in venc_open(); the HEIC variant is not used.
    if (!mDev->venc_open(omxCoding())) {
        ALOGE("venc_open failed");
        delete mDev;
        mDev = nullptr;
        return C2_CORRUPTED;
    }

    mEngineOpened = true;

    // The engine does not spawn its own V4L2 poll loop -- the host does, and
    // passes itself as the argument. Without this nothing ever dequeues a
    // finished buffer, so the output slots fill up and every subsequent frame
    // stalls. venc_set_message_thread_id() is what lets venc_close() join it.
    pthread_t tid;
    int r = pthread_create(&tid, nullptr, venc_dev::async_venc_message_thread,
                           static_cast<C2VencHost *>(this));
    if (r != 0) {
        ALOGE("failed to create the engine message thread: %d", r);
        mDev->venc_close();
        mEngineOpened = false;
        delete mDev;
        mDev = nullptr;
        return C2_NO_MEMORY;
    }
    // venc_dev reads handle->... off the host inside that thread.
    handle = mDev;
    mDev->venc_set_message_thread_id(tid);
    mAsyncThreadRunning = true;

    return configureEngine();
}

bool C2VencComponent::ensureStaging(uint32_t gw, uint32_t gh, uint32_t gfmt,
                                    uint32_t gstride) {
    (void)gw; (void)gh; (void)gstride;
    switch (gfmt) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
            break;
        default:
            // NV12 (or anything else the ISP gives us) goes straight to the
            // encoder; only RGB needs staging.
            mConversionNeeded = false;
            return true;
    }
    if (mConversionNeeded && mConvSrcFormat == gfmt && !mStageIon.empty()) {
        return true;
    }
    if (!allocStaging()) {
        return false;
    }
    mConvSrcFormat = gfmt;
    ALOGI("libyuv staging enabled: RGBA %ux%u stride %u -> NV12 %ux%u "
          "(ystride %u, yscan %u, uvstride %u), %zu x %u bytes",
          gw, gh, gstride, mIntf->getWidth(), mIntf->getHeight(), mStageYStride,
          mStageYScanlines, mStageUvStride, mStageIon.size(), mStageSize);
    return true;
}

bool C2VencComponent::allocStaging() {
    releaseStaging();

    const uint32_t w = mIntf->getWidth();
    const uint32_t h = mIntf->getHeight();
    // The destination must be laid out exactly as the encoder expects NV12:
    // Venus-aligned stride and scanlines, UV plane after Y_STRIDE*Y_SCANLINES.
    mStageYStride = VENUS_Y_STRIDE(COLOR_FMT_NV12, w);
    mStageYScanlines = VENUS_Y_SCANLINES(COLOR_FMT_NV12, h);
    mStageUvStride = VENUS_UV_STRIDE(COLOR_FMT_NV12, w);
    mStageSize = VENUS_BUFFER_SIZE(COLOR_FMT_NV12, w, h);
    if (mStageSize < mInBufSize) {
        mStageSize = mInBufSize;
    }

    // One staging buffer per output slot, so a conversion never overwrites a
    // frame the encoder is still reading.
    // One staging buffer per input slot: the encoder may hold several inputs.
    const uint32_t slots = mInBufCount ? mInBufCount : 8;
    mStageIon.resize(slots);
    mStageBase.resize(slots, nullptr);
    for (uint32_t i = 0; i < slots; ++i) {
        memset(&mStageIon[i], 0, sizeof(venc_ion));
        // Cached: the CPU writes the whole buffer, then the encoder DMAs it.
        if (!alloc_map_ion_memory((int)mStageSize, &mStageIon[i], ION_FLAG_CACHED)) {
            ALOGE("staging ION alloc %u bytes failed at slot %u", mStageSize, i);
            releaseStaging();
            return false;
        }
        mStageBase[i] = ion_map(mStageIon[i].data_fd, (int)mStageSize);
        if (mStageBase[i] == MAP_FAILED) {
            ALOGE("staging mmap failed at slot %u", i);
            mStageBase[i] = nullptr;
            releaseStaging();
            return false;
        }
    }
    mConversionNeeded = true;
    return true;
}

bool C2VencComponent::checkYuvLayout(const C2ConstGraphicBlock &block) {
    mYuvChecked = true;
    mYuvCopy = false;
    const C2GraphicView view = block.map().get();
    if (view.error() != C2_OK) {
        ALOGW("cannot map the first YUV input (%d); assuming Venus NV12", view.error());
        return true;
    }
    const C2PlanarLayout &layout = view.layout();
    if (layout.type != C2PlanarLayout::TYPE_YUV || layout.numPlanes < 3) {
        ALOGW("first input is not planar YUV (type %d, %u planes); assuming Venus NV12",
              (int)layout.type, layout.numPlanes);
        return true;
    }
    const C2PlaneInfo &yp = layout.planes[C2PlanarLayout::PLANE_Y];
    const C2PlaneInfo &up = layout.planes[C2PlanarLayout::PLANE_U];
    const C2PlaneInfo &vp = layout.planes[C2PlanarLayout::PLANE_V];
    const uint8_t *y = view.data()[C2PlanarLayout::PLANE_Y];
    const uint8_t *u = view.data()[C2PlanarLayout::PLANE_U];
    const uint8_t *v = view.data()[C2PlanarLayout::PLANE_V];
    const ptrdiff_t uOff = u - y, vOff = v - y;
    const bool semiPlanar = up.colInc == 2 && vp.colInc == 2 && up.rowInc == vp.rowInc;
    const bool nv12 = semiPlanar && vOff == uOff + 1;
    const bool nv21 = semiPlanar && uOff == vOff + 1;

    const uint32_t w = mIntf->getWidth(), h = mIntf->getHeight();
    const int32_t vYStride = VENUS_Y_STRIDE(COLOR_FMT_NV12, w);
    const int32_t vYScan = VENUS_Y_SCANLINES(COLOR_FMT_NV12, h);
    const int32_t vUvStride = VENUS_UV_STRIDE(COLOR_FMT_NV12, w);
    const bool venus = nv12 && yp.rowInc == vYStride && up.rowInc == vUvStride &&
                       uOff == (ptrdiff_t)vYStride * vYScan;

    ALOGI("input YUV layout %ux%u: y stride %d, uv stride %d, uv offset %td, %s; "
          "Venus wants y stride %d, uv offset %td -> %s",
          w, h, yp.rowInc, up.rowInc, nv21 ? vOff : uOff,
          nv12 ? "NV12" : nv21 ? "NV21" : "other", vYStride, (ptrdiff_t)vYStride * vYScan,
          venus ? "direct" : (nv12 || nv21) ? "repack" : "unsupported, direct");
    if (venus || !(nv12 || nv21)) {
        return true;
    }
    if (!allocStaging()) {
        return false;
    }
    // mConversionNeeded selects the RGBA path; this is the YUV repack path.
    mConversionNeeded = false;
    mYuvCopy = true;
    mYuvNv21 = nv21;
    return true;
}

void C2VencComponent::releaseStaging() {
    for (size_t i = 0; i < mStageIon.size(); ++i) {
        if (i < mStageBase.size() && mStageBase[i] != nullptr) {
            ion_unmap(mStageIon[i].data_fd, mStageBase[i], (int)mStageSize);
            mStageBase[i] = nullptr;
        }
        free_ion_memory(&mStageIon[i]);
    }
    mStageIon.clear();
    mStageBase.clear();
    mStageSize = 0;
    mConversionNeeded = false;
}

c2_status_t C2VencComponent::configureEngine() {
    const uint32_t width = mIntf->getWidth();
    const uint32_t height = mIntf->getHeight();

    // Port definition drives buffer geometry inside the engine, and
    // m_sInPortDef is one of the members it reads back out of the host.
    m_sInPortDef.format.video.nFrameWidth = width;
    m_sInPortDef.format.video.nFrameHeight = height;
    m_sInPortDef.format.video.nStride = width;
    m_sInPortDef.format.video.nSliceHeight = height;
    m_sInPortDef.format.video.xFramerate = (OMX_U32)(mIntf->getFrameRate() * 65536);
    m_sInPortDef.format.video.eColorFormat =
            (OMX_COLOR_FORMATTYPE)QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m;

    // Buffer requirements must be read BEFORE the port is configured:
    // venc_set_param(OMX_IndexParamPortDefinition) rejects the call outright if
    // nBufferCountActual is below the driver's mincount
    // (video_encoder_device_v4l2.cpp, PORT_INDEX_IN case), and a zero-filled
    // port definition trips exactly that.
    OMX_U32 inMin = 0, outMin = 0;
    if (!mDev->venc_get_buf_req(&inMin, &mInBufCount, &mInBufSize, 0 /*PORT_INDEX_IN*/)) {
        ALOGE("venc_get_buf_req(input) failed");
        return C2_CORRUPTED;
    }
    if (mInBufCount < inMin) {
        mInBufCount = inMin;
    }

    OMX_PARAM_PORTDEFINITIONTYPE portDef;
    memset(&portDef, 0, sizeof(portDef));
    portDef.nSize = sizeof(portDef);
    portDef.nPortIndex = 0;  // PORT_INDEX_IN
    portDef.eDir = OMX_DirInput;
    portDef.eDomain = OMX_PortDomainVideo;
    portDef.nBufferCountMin = inMin;
    portDef.nBufferCountActual = mInBufCount;
    portDef.nBufferSize = mInBufSize;
    portDef.format.video = m_sInPortDef.format.video;
    if (!mDev->venc_set_param(&portDef, OMX_IndexParamPortDefinition)) {
        ALOGE("venc_set_param(input port) failed (count %u min %u size %u)",
              mInBufCount, inMin, mInBufSize);
        return C2_CORRUPTED;
    }

    if (!mDev->venc_get_buf_req(&outMin, &mOutBufCount, &mOutBufSize, 1 /*PORT_INDEX_OUT*/)) {
        ALOGE("venc_get_buf_req(output) failed");
        return C2_CORRUPTED;
    }
    if (mOutBufCount < outMin) {
        mOutBufCount = outMin;
    }

    memset(&portDef, 0, sizeof(portDef));
    portDef.nSize = sizeof(portDef);
    portDef.nPortIndex = 1;  // PORT_INDEX_OUT
    portDef.eDir = OMX_DirOutput;
    portDef.eDomain = OMX_PortDomainVideo;
    portDef.nBufferCountMin = outMin;
    portDef.nBufferCountActual = mOutBufCount;
    portDef.nBufferSize = mOutBufSize;
    portDef.format.video = m_sInPortDef.format.video;
    portDef.format.video.eCompressionFormat = omxCoding();
    portDef.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    portDef.format.video.nBitrate = mIntf->getBitrate();
    if (!mDev->venc_set_param(&portDef, OMX_IndexParamPortDefinition)) {
        ALOGE("venc_set_param(output port) failed (count %u min %u size %u)",
              mOutBufCount, outMin, mOutBufSize);
        return C2_CORRUPTED;
    }

    // Profile and level, on the OUTPUT port. The engine rejects
    // V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES inside venc_start() if the session
    // profile was never established, which surfaces as the misleading
    // "Reconfiguring intra period failed".
    OMX_VIDEO_PARAM_PROFILELEVELTYPE profileLevel;
    memset(&profileLevel, 0, sizeof(profileLevel));
    profileLevel.nSize = sizeof(profileLevel);
    profileLevel.nPortIndex = 1;  // PORT_INDEX_OUT
    if (mIntf->getCodec() == Codec::HEVC) {
        profileLevel.eProfile = OMX_VIDEO_HEVCProfileMain;
        profileLevel.eLevel = OMX_VIDEO_HEVCMainTierLevel41;
    } else {
        profileLevel.eProfile = OMX_VIDEO_AVCProfileHigh;
        profileLevel.eLevel = OMX_VIDEO_AVCLevel41;
    }
    if (!mDev->venc_set_param(&profileLevel, OMX_IndexParamVideoProfileLevelCurrent)) {
        ALOGE("venc_set_param(profile/level) failed");
        return C2_CORRUPTED;
    }

    // Intra period. venc_dev defaults num_pframes to 29, but it only programs
    // the control during venc_start(); setting it explicitly here ties it to
    // the interface's sync-frame interval instead of a hardcoded 1s at 30fps.
    const float fps = mIntf->getFrameRate() > 0.f ? mIntf->getFrameRate() : 30.f;
    const uint32_t syncUs = mIntf->getSyncFramePeriod();
    uint32_t pFrames = 29;
    if (syncUs > 0 && syncUs != ~0u) {
        double frames = (double)syncUs * fps / 1000000.0;
        pFrames = frames > 1.0 ? (uint32_t)(frames - 1.0) : 0;
        // The driver rejects very long GOPs; cap it rather than have the
        // control fail outright and lose the setting entirely.
        if (pFrames > kMaxPFrames) {
            pFrames = kMaxPFrames;
        }
    }
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD intraPeriod;
    memset(&intraPeriod, 0, sizeof(intraPeriod));
    intraPeriod.nSize = sizeof(intraPeriod);
    intraPeriod.nPortIndex = 1;  // PORT_INDEX_OUT
    intraPeriod.nPFrames = pFrames;
    intraPeriod.nIDRPeriod = 1;
    if (!mDev->venc_set_config(&intraPeriod, OMX_IndexConfigVideoAVCIntraPeriod)) {
        // Not fatal. The driver rejects large values (screenrecord asks for a
        // 20s keyframe interval, i.e. 599 P-frames at 30fps, and the control
        // fails), and venc_start() programs its own default of 29 regardless.
        // A suboptimal GOP is much better than refusing to encode.
        ALOGW("venc_set_config(intra period, pframes=%u) rejected; "
              "falling back to the engine default", pFrames);
    }

    OMX_VIDEO_PARAM_BITRATETYPE bitrate;
    memset(&bitrate, 0, sizeof(bitrate));
    bitrate.nSize = sizeof(bitrate);
    bitrate.nPortIndex = 1;
    bitrate.eControlRate = OMX_Video_ControlRateVariable;
    bitrate.nTargetBitrate = mIntf->getBitrate();
    if (!mDev->venc_set_param(&bitrate, OMX_IndexParamVideoBitrate)) {
        ALOGE("venc_set_param(bitrate) failed");
        return C2_CORRUPTED;
    }

    // Re-query now that both ports carry real geometry. The first query (before
    // venc_set_param) could only give the driver's defaults -- it exists to
    // learn mincount -- so the sizes from it are wrong: 98304 bytes for a
    // 1080x2340 NV12 input, where ~3.8 MB is correct.
    OMX_U32 reMin = 0;
    if (mDev->venc_get_buf_req(&reMin, &mInBufCount, &mInBufSize, 0 /*PORT_INDEX_IN*/)) {
        if (mInBufCount < reMin) {
            mInBufCount = reMin;
        }
    } else {
        ALOGW("re-query of input buffer requirements failed; keeping %u x %u",
              mInBufCount, mInBufSize);
    }
    if (mDev->venc_get_buf_req(&reMin, &mOutBufCount, &mOutBufSize, 1 /*PORT_INDEX_OUT*/)) {
        if (mOutBufCount < reMin) {
            mOutBufCount = reMin;
        }
    } else {
        ALOGW("re-query of output buffer requirements failed; keeping %u x %u",
              mOutBufCount, mOutBufSize);
    }

    // venc_empty_buf() does REQBUFS for the OUTPUT_MPLANE queue inline, but
    // venc_fill_buf() does not do it for CAPTURE_MPLANE -- in the OMX component
    // that happens during buffer allocation, which we do not use. Without it
    // the driver has no capture buffer pool and every ftb is rejected with
    // "Failed to qbuf (ftb) to driver".
    // The engine's async thread indexes m_out_mem_ptr / m_pOutput_pmem with the
    // dequeued V4L2 index, so those arrays must exist and must be the very
    // headers we submit -- it hands the resulting pointer back as clientdata.
    if (!allocBufferArrays(mInBufCount, mOutBufCount)) {
        return C2_NO_MEMORY;
    }

    if (!mDev->venc_reconfig_reqbufs()) {
        ALOGE("venc_reconfig_reqbufs failed");
        return C2_CORRUPTED;
    }

    ALOGI("engine configured %ux%u, in %u x %u bytes, out %u x %u bytes", width, height,
          mInBufCount, mInBufSize, mOutBufCount, mOutBufSize);
    return C2_OK;
}

void C2VencComponent::closeEngine() {
    releaseStaging();
    mYuvChecked = false;
    mYuvCopy = false;
    mYuvNv21 = false;
    if (mDev == nullptr) {
        return;
    }
    if (mStreaming) {
        mDev->venc_stop();
        mStreaming = false;
    }
    if (mEngineOpened) {
        // venc_close() sets async_thread_force_stop and joins the thread we
        // registered with venc_set_message_thread_id().
        mDev->venc_close();
        mEngineOpened = false;
        mAsyncThreadRunning = false;
    }
    // Only after venc_close() has joined the async thread: it dereferences
    // these arrays on every dequeue, so freeing them earlier is a use-after-free.
    freeBufferArrays();

    delete mDev;
    mDev = nullptr;
}

c2_status_t C2VencComponent::onInit() {
    mSignalledError = false;
    mSawInputEos = false;
    return openEngine();
}

c2_status_t C2VencComponent::onStop() {
    if (mDev != nullptr && mStreaming) {
        mDev->venc_stop();
        mDev->venc_stop_done();
        mStreaming = false;
    }
    {
        std::lock_guard<std::mutex> lock(mLock);
        mInFlight.clear();
        mInFlightCount = 0;
        mInNext = 0;
        mOutSlots.clear();
        mOutputsSeeded = false;
    }
    return C2_OK;
}

void C2VencComponent::onReset() {
    (void)onStop();
}

void C2VencComponent::onRelease() {
    closeEngine();
}

c2_status_t C2VencComponent::onFlush_sm() {
    if (mDev != nullptr && mStreaming) {
        // 3 == both ports, matching the engine's flush encoding.
        mDev->venc_flush(3);
    }
    {
        std::lock_guard<std::mutex> lock(mLock);
        mInFlight.clear();
        mInFlightCount = 0;
        mOutSlots.clear();
        mOutputsSeeded = false;
    }
    return C2_OK;
}

bool C2VencComponent::seedOutputs(const std::shared_ptr<C2BlockPool> &pool) {
    mPool = pool;
    mOutSlots.assign(mOutBufCount, OutSlot());
    for (uint32_t i = 0; i < mOutBufCount; ++i) {
        if (!requeueOutput(i)) {
            ALOGE("seeding capture slot %u failed", i);
            return false;
        }
    }
    ALOGI("seeded %u capture buffers", mOutBufCount);
    return true;
}

bool C2VencComponent::requeueOutput(uint32_t slot) {
    if (!mPool || slot >= mOutSlots.size() || mDev == nullptr) {
        return false;
    }
    std::shared_ptr<C2LinearBlock> block;
    C2MemoryUsage usage = {C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE};
    c2_status_t err = mPool->fetchLinearBlock(mOutBufSize, usage, &block);
    if (err != C2_OK || !block) {
        ALOGE("fetchLinearBlock(%u) for slot %u failed: %d", mOutBufSize, slot, err);
        return false;
    }
    OMX_BUFFERHEADERTYPE *hdr = &m_out_mem_ptr[slot];
    memset(hdr, 0, sizeof(*hdr));
    hdr->nSize = sizeof(*hdr);
    hdr->nAllocLen = block->size();
    hdr->nOffset = block->offset();
    const int fd = block->handle()->data[0];
    m_pOutput_pmem[slot].fd = fd;
    m_pOutput_pmem[slot].size = block->size();
    m_pOutput_pmem[slot].offset = block->offset();
    if (!mDev->venc_fill_buf(hdr, nullptr, slot, fd)) {
        ALOGE("venc_fill_buf failed for slot %u", slot);
        return false;
    }
    mOutSlots[slot].block = block;
    mOutSlots[slot].fd = fd;
    mOutSlots[slot].queued = true;
    return true;
}

void C2VencComponent::process(const std::unique_ptr<C2Work> &work,
                              const std::shared_ptr<C2BlockPool> &pool) {
    work->result = C2_OK;
    work->workletsProcessed = 0u;
    work->worklets.front()->output.flags = work->input.flags;

    if (mSignalledError) {
        work->result = C2_BAD_VALUE;
        return;
    }
    if (!mEngineOpened) {
        c2_status_t err = openEngine();
        if (err != C2_OK) {
            mSignalledError = true;
            work->result = err;
            return;
        }
    }
    if (!mStreaming) {
        if (mDev->venc_start() != 0) {
            ALOGE("venc_start failed");
            mSignalledError = true;
            work->result = C2_CORRUPTED;
            return;
        }
        mStreaming = true;
    }
    if (!mOutputsSeeded) {
        if (!seedOutputs(pool)) {
            mSignalledError = true;
            work->result = C2_NO_MEMORY;
            return;
        }
        mOutputsSeeded = true;
    }

    const bool eos = (work->input.flags & C2FrameData::FLAG_END_OF_STREAM) != 0;
    if (work->input.buffers.empty()) {
        if (eos) {
            mSawInputEos = true;
            mDev->venc_handle_empty_eos_buffer();
        }
        work->workletsProcessed = 1u;
        return;
    }

    const C2ConstGraphicBlock inBlock = work->input.buffers[0]->data().graphicBlocks().front();
    const C2Handle *inHandle = inBlock.handle();
    if (inHandle == nullptr || inHandle->numFds < 1) {
        ALOGE("input block has no fd");
        mSignalledError = true;
        work->result = C2_CORRUPTED;
        return;
    }
    if (!mLoggedInputFormat) {
        uint32_t gw = 0, gh = 0, gfmt = 0, gstride = 0, ggen = 0, gslot = 0;
        uint64_t gusage = 0, gigbp = 0;
        _UnwrapNativeCodec2GrallocMetadata(inHandle, &gw, &gh, &gfmt, &gusage,
                                           &gstride, &ggen, &gigbp, &gslot);
        ALOGI("input gralloc: %ux%u fmt 0x%x stride %u usage 0x%llx, numFds %d",
              gw, gh, gfmt, gstride, (unsigned long long)gusage, inHandle->numFds);
        mLoggedInputFormat = true;
        mConvGw = gw; mConvGh = gh; mConvFmt = gfmt; mConvStride = gstride;
    }
    if (!ensureStaging(mConvGw, mConvGh, mConvFmt, mConvStride)) {
        mSignalledError = true;
        work->result = C2_CORRUPTED;
        return;
    }
    if (!mConversionNeeded && !mYuvChecked && !checkYuvLayout(inBlock)) {
        mSignalledError = true;
        work->result = C2_CORRUPTED;
        return;
    }

    // Input slot: a ring over the input pool, bounded by what is in flight.
    uint32_t in;
    {
        std::unique_lock<std::mutex> lock(mLock);
        mCv.wait_for(lock, std::chrono::milliseconds(500),
                     [&] { return mInFlightCount < mInBufCount; });
        if (mInFlightCount >= mInBufCount) {
            ALOGE("input pool exhausted (%u in flight)", mInFlightCount);
            work->result = C2_TIMED_OUT;
            return;
        }
        in = mInNext;
        mInNext = (mInNext + 1) % mInBufCount;
        mInFlightCount++;
    }

    const int64_t ts = work->input.ordinal.timestamp.peekll();
    const uint64_t frameIndex = work->input.ordinal.frameIndex.peeku();
    OMX_BUFFERHEADERTYPE *inHdr = &m_inp_mem_ptr[in];
    memset(inHdr, 0, sizeof(*inHdr));
    inHdr->nSize = sizeof(*inHdr);
    inHdr->nTimeStamp = ts;
    if (eos) {
        inHdr->nFlags |= OMX_BUFFERFLAG_EOS;
        mSawInputEos = true;
    }

    int submitFd = inHandle->data[0];
    const uint32_t stageIdx = mStageIon.empty() ? 0 : (in % mStageIon.size());
    if (mConversionNeeded && !mStageIon.empty()) {
        const int srcStrideBytes = (int)(mConvStride * 4);
        const int srcSize = srcStrideBytes * (int)mConvGh;
        void *srcBase = ion_map(submitFd, srcSize);
        if (srcBase == MAP_FAILED) {
            ALOGE("failed to map the %d-byte RGBA input", srcSize);
            std::lock_guard<std::mutex> lock(mLock); mInFlightCount--;
            mSignalledError = true; work->result = C2_CORRUPTED; return;
        }
        uint8_t *dstY = static_cast<uint8_t *>(mStageBase[stageIdx]);
        uint8_t *dstUV = dstY + (size_t)mStageYStride * mStageYScanlines;
        const int rc = libyuv::ABGRToNV12(static_cast<const uint8_t *>(srcBase), srcStrideBytes,
                                          dstY, (int)mStageYStride, dstUV, (int)mStageUvStride,
                                          (int)mIntf->getWidth(), (int)mIntf->getHeight());
        ion_unmap(submitFd, srcBase, srcSize);
        if (rc != 0) {
            ALOGE("libyuv ABGRToNV12 failed: %d", rc);
            std::lock_guard<std::mutex> lock(mLock); mInFlightCount--;
            mSignalledError = true; work->result = C2_CORRUPTED; return;
        }
        do_cache_operations(mStageIon[stageIdx].data_fd);
        submitFd = mStageIon[stageIdx].data_fd;
        inHdr->nFilledLen = mStageSize;
        inHdr->nAllocLen = mStageSize;
    } else if (mYuvCopy && !mStageIon.empty()) {
        const C2GraphicView view = inBlock.map().get();
        if (view.error() != C2_OK) {
            ALOGE("failed to map YUV input: %d", view.error());
            std::lock_guard<std::mutex> lock(mLock); mInFlightCount--;
            mSignalledError = true; work->result = C2_CORRUPTED; return;
        }
        const C2PlanarLayout &layout = view.layout();
        const int w = (int)mIntf->getWidth(), h = (int)mIntf->getHeight();
        uint8_t *dstY = static_cast<uint8_t *>(mStageBase[stageIdx]);
        uint8_t *dstUV = dstY + (size_t)mStageYStride * mStageYScanlines;
        const uint8_t *srcY = view.data()[C2PlanarLayout::PLANE_Y];
        const int srcYStride = layout.planes[C2PlanarLayout::PLANE_Y].rowInc;
        const int srcUvStride = layout.planes[C2PlanarLayout::PLANE_U].rowInc;
        if (mYuvNv21) {
            libyuv::NV21ToNV12(srcY, srcYStride, view.data()[C2PlanarLayout::PLANE_V],
                               srcUvStride, dstY, (int)mStageYStride, dstUV,
                               (int)mStageUvStride, w, h);
        } else {
            libyuv::CopyPlane(srcY, srcYStride, dstY, (int)mStageYStride, w, h);
            libyuv::CopyPlane(view.data()[C2PlanarLayout::PLANE_U], srcUvStride, dstUV,
                              (int)mStageUvStride, w, (h + 1) / 2);
        }
        do_cache_operations(mStageIon[stageIdx].data_fd);
        submitFd = mStageIon[stageIdx].data_fd;
        inHdr->nFilledLen = mStageSize;
        inHdr->nAllocLen = mStageSize;
    } else {
        const uint32_t sz = mInBufSize ? mInBufSize
                : VENUS_BUFFER_SIZE(COLOR_FMT_NV12, mIntf->getWidth(), mIntf->getHeight());
        inHdr->nFilledLen = sz;
        inHdr->nAllocLen = sz;
    }

    {
        std::lock_guard<std::mutex> lock(mLock);
        mInFlight[ts] = frameIndex;
    }
    if (!mDev->venc_empty_buf(inHdr, nullptr, in, submitFd)) {
        ALOGE("venc_empty_buf failed (slot %u ts %lld)", in, (long long)ts);
        std::lock_guard<std::mutex> lock(mLock);
        mInFlight.erase(ts);
        mInFlightCount--;
        mSignalledError = true;
        work->result = C2_CORRUPTED;
        return;
    }
    ALOGV("ETB slot %u ts %lld frame %llu fd %d len %u", in, (long long)ts,
          (unsigned long long)frameIndex, submitFd, (unsigned)inHdr->nFilledLen);
    if (frameIndex < 3) {
        ALOGI("ETB slot %u ts %lld frame %llu fd %d len %u", in, (long long)ts,
              (unsigned long long)frameIndex, submitFd, (unsigned)inHdr->nFilledLen);
    }
    // Completion is deferred to onOutputDone() via finish().
}

c2_status_t C2VencComponent::drain(uint32_t drainMode,
                                   const std::shared_ptr<C2BlockPool> &pool) {
    (void)pool;
    if (drainMode == NO_DRAIN || drainMode == DRAIN_CHAIN) {
        return C2_OMITTED;
    }
    if (mDev != nullptr && mStreaming && !mSawInputEos) {
        mDev->venc_handle_empty_eos_buffer();
        mSawInputEos = true;
    }
    return C2_OK;
}

void C2VencComponent::onInputDone(OMX_BUFFERHEADERTYPE *hdr) {
    (void)hdr;
    std::lock_guard<std::mutex> lock(mLock);
    if (mInFlightCount > 0) {
        mInFlightCount--;
    }
    mCv.notify_all();
}

void C2VencComponent::onOutputDone(OMX_BUFFERHEADERTYPE *hdr, const ::venc_msg &msg) {
    if (m_out_mem_ptr == nullptr || hdr < m_out_mem_ptr ||
        hdr >= m_out_mem_ptr + mOutArrayCount) {
        ALOGE("output completion outside the capture array; ignoring");
        return;
    }
    const uint32_t slot = (uint32_t)(hdr - m_out_mem_ptr);
    std::shared_ptr<C2LinearBlock> block;
    {
        std::lock_guard<std::mutex> lock(mLock);
        if (slot < mOutSlots.size()) {
            block = std::move(mOutSlots[slot].block);
            mOutSlots[slot].queued = false;
        }
    }
    const size_t len = msg.buf.len;
    const size_t off = msg.buf.offset;
    const int64_t ts = msg.buf.timestamp;
    const bool eos = (msg.buf.flags & OMX_BUFFERFLAG_EOS) != 0;
    const bool csd = (msg.buf.flags & OMX_BUFFERFLAG_CODECCONFIG) != 0;
    ALOGI("FBD slot %u len %zu off %zu ts %lld flags 0x%lx%s%s", slot, len, off, (long long)ts,
          msg.buf.flags, csd ? " CSD" : "", eos ? " EOS" : "");

    if (block && csd && len > 0) {
        C2WriteView view = block->map().get();
        if (view.error() == C2_OK) {
            auto c = C2StreamInitDataInfo::output::AllocUnique(len, 0u);
            if (c) {
                memcpy(c->m.value, view.data() + off, len);
                std::lock_guard<std::mutex> lock(mLock);
                mCsd = std::move(c);
            }
        }
    } else if (block && len > 0) {
        uint64_t frameIndex = 0;
        bool found = false;
        std::unique_ptr<C2StreamInitDataInfo::output> csdOut;
        {
            std::lock_guard<std::mutex> lock(mLock);
            auto it = mInFlight.find(ts);
            if (it == mInFlight.end() && !mInFlight.empty()) {
                it = mInFlight.begin();   // oldest; timestamps are monotonic
            }
            if (it != mInFlight.end()) {
                frameIndex = it->second;
                mInFlight.erase(it);
                found = true;
            }
            csdOut = std::move(mCsd);
        }
        if (found) {
            std::shared_ptr<C2Buffer> buffer = createLinearBuffer(block, off, len);
            if (msg.buf.flags & OMX_BUFFERFLAG_SYNCFRAME) {
                buffer->setInfo(std::make_shared<C2StreamPictureTypeMaskInfo::output>(
                        0u, C2Config::SYNC_FRAME));
            }
            finish(frameIndex, [&buffer, &csdOut, eos](const std::unique_ptr<C2Work> &work) {
                if (csdOut) {
                    work->worklets.front()->output.configUpdate.push_back(std::move(csdOut));
                }
                work->worklets.front()->output.flags =
                        eos ? C2FrameData::FLAG_END_OF_STREAM : (C2FrameData::flags_t)0;
                work->worklets.front()->output.buffers.clear();
                work->worklets.front()->output.buffers.push_back(buffer);
                work->worklets.front()->output.ordinal = work->input.ordinal;
                work->workletsProcessed = 1u;
            });
        } else {
            ALOGW("FBD ts %lld matched no in-flight frame; dropped", (long long)ts);
        }
    }

    // Give the slot straight back to the encoder unless we are tearing down.
    if (mStreaming && !mSignalledError) {
        if (!requeueOutput(slot)) {
            ALOGE("failed to re-queue capture slot %u", slot);
        }
    }
}

int C2VencComponent::async_message_process(void *context, void *message) {
    (void)context;
    if (message == nullptr) {
        return -1;
    }
    ::venc_msg *msg = (::venc_msg *)message;
    if (msg->statuscode != VEN_S_SUCCESS) {
        ALOGE("engine reported status %lu for msg %lu", msg->statuscode, msg->msgcode);
        mSignalledError = true;
    }
    switch (msg->msgcode) {
        case VEN_MSG_OUTPUT_BUFFER_DONE:
            if (msg->buf.clientdata) {
                onOutputDone((OMX_BUFFERHEADERTYPE *)msg->buf.clientdata, *msg);
            }
            break;
        case VEN_MSG_INPUT_BUFFER_DONE:
            onInputDone((OMX_BUFFERHEADERTYPE *)msg->buf.clientdata);
            break;
        case VEN_MSG_FLUSH_INPUT_DONE:
        case VEN_MSG_FLUSH_OUPUT_DONE:
            mCv.notify_all();
            break;
        case VEN_MSG_HW_OVERLOAD:
            ALOGE("hardware overload reported");
            mSignalledError = true;
            break;
        default:
            break;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Factory
//
// IntfImpl is a complete type only in this translation unit, so C2VencStore
// cannot build the interface itself -- this is the store's only way in.
// Shape follows C2SoftAvcEncFactory, except the reflector is injected rather
// than taken from GetCodec2PlatformComponentStore(): the store owns the
// reflector the HAL reflects params through, and the platform store is the
// wrong one inside a vendor HAL process.
// ---------------------------------------------------------------------------

class C2VencComponentFactory : public C2ComponentFactory {
public:
    C2VencComponentFactory(std::shared_ptr<C2ReflectorHelper> helper, C2VencComponent::Codec codec)
        : mHelper(std::move(helper)), mCodec(codec) {}
    ~C2VencComponentFactory() override = default;

    c2_status_t createComponent(c2_node_id_t id,
                                std::shared_ptr<C2Component> *const component,
                                ComponentDeleter deleter) override {
        *component = std::shared_ptr<C2Component>(
                new C2VencComponent(name(), id,
                                    std::make_shared<C2VencComponent::IntfImpl>(mHelper, mCodec)),
                deleter);
        return C2_OK;
    }

    c2_status_t createInterface(c2_node_id_t id,
                                std::shared_ptr<C2ComponentInterface> *const interface,
                                InterfaceDeleter deleter) override {
        *interface = std::shared_ptr<C2ComponentInterface>(
                new SimpleInterface<C2VencComponent::IntfImpl>(
                        name(), id,
                        std::make_shared<C2VencComponent::IntfImpl>(mHelper, mCodec)),
                deleter);
        return C2_OK;
    }

private:
    const char *name() const {
        return mCodec == C2VencComponent::Codec::HEVC ? COMPONENT_NAME_HEVC : COMPONENT_NAME_AVC;
    }

    std::shared_ptr<C2ReflectorHelper> mHelper;
    const C2VencComponent::Codec mCodec;
};

std::unique_ptr<C2ComponentFactory> CreateC2VencFactory(
        const std::string &name, std::shared_ptr<C2ReflectorHelper> helper) {
    if (name == COMPONENT_NAME_AVC) {
        return std::make_unique<C2VencComponentFactory>(std::move(helper),
                                                        C2VencComponent::Codec::AVC);
    }
    if (name == COMPONENT_NAME_HEVC) {
        return std::make_unique<C2VencComponentFactory>(std::move(helper),
                                                        C2VencComponent::Codec::HEVC);
    }
    return nullptr;
}

}  // namespace android
