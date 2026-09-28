/*
 * Codec2 video encoder for sm8150, driving the in-tree venc_dev V4L2 engine.
 *
 * The component is both a SimpleC2Component (so it inherits the work queue,
 * threading and start/stop/flush plumbing from AOSP) and a C2VencHost (so
 * venc_dev can call back into it for ION and completions). The engine itself
 * is the shipped sm8150 one, compiled with -DVENC_HOST_C2; nothing in its
 * V4L2 handling is reimplemented here.
 *
 * Buffers cross the boundary as OMX_BUFFERHEADERTYPE because that is what
 * venc_dev's signatures take. They are plain structs -- this fabricates them
 * around C2 blocks rather than owning any OMX machinery.
 */

#ifndef C2_VENC_COMPONENT_H_
#define C2_VENC_COMPONENT_H_

#include <condition_variable>
#include <map>
#include <vector>
#include <memory>
#include <mutex>

#include <SimpleC2Component.h>
#include <SimpleC2Interface.h>

#include "C2VencHost.h"

class venc_dev;
struct venc_msg;

namespace android {

struct C2VencComponent : public SimpleC2Component, public C2VencHost {
    class IntfImpl;

    // One component class, two codecs. venc_dev already handles both; the
    // only differences up here are the name, mime, profile/level vocabulary
    // and the OMX coding enum passed to venc_open().
    enum class Codec { AVC, HEVC };

    C2VencComponent(const char *name, c2_node_id_t id,
                    const std::shared_ptr<IntfImpl> &intfImpl);
    ~C2VencComponent() override;

    // SimpleC2Component
    c2_status_t onInit() override;
    c2_status_t onStop() override;
    void onReset() override;
    void onRelease() override;
    c2_status_t onFlush_sm() override;
    void process(const std::unique_ptr<C2Work> &work,
                 const std::shared_ptr<C2BlockPool> &pool) override;
    c2_status_t drain(uint32_t drainMode,
                      const std::shared_ptr<C2BlockPool> &pool) override;

    // C2VencHost -- venc_dev's async message thread lands here.
    int async_message_process(void *context, void *message) override;
    bool is_secure_session(void) override;

private:
    // Output side, modelled on the OMX component: every capture slot is queued
    // up front and re-queued the moment the encoder returns it. msm_vidc will
    // not start producing until its firmware minimum of capture buffers is
    // queued, so "one output per input frame" leaves the encoder idle forever.
    struct OutSlot {
        std::shared_ptr<C2LinearBlock> block;
        int fd = -1;
        bool queued = false;
    };
    bool seedOutputs(const std::shared_ptr<C2BlockPool> &pool);
    bool requeueOutput(uint32_t slot);
    void onOutputDone(OMX_BUFFERHEADERTYPE *hdr, const ::venc_msg &msg);
    void onInputDone(OMX_BUFFERHEADERTYPE *hdr);

    OMX_VIDEO_CODINGTYPE omxCoding() const;
    c2_status_t openEngine();
    void closeEngine();
    c2_status_t configureEngine();

    std::shared_ptr<IntfImpl> mIntf;
    venc_dev *mDev;

    bool mEngineOpened;
    bool mStreaming;
    bool mSignalledError;
    bool mAsyncThreadRunning;
    bool mSawInputEos;
    bool mLoggedInputFormat;

    // RGBA -> NV12 staging. screenrecord (and anything else that encodes a
    // composited surface) hands us HAL_PIXEL_FORMAT_RGBA_8888; the encoder only
    // takes NV12 and the driver answers raw RGBA with EINVAL on qbuf.
    //
    // Converted on the CPU with libyuv, exactly as AOSP's own C2SoftAvcEnc does
    // for TYPE_RGBA input (C2SoftAvcEnc.cpp:1551). NOT with C2D: the QTI blob's
    // C2D blit fails on this device (changes.md sec 41) and the open-source
    // C2DColorConverter hangs inside convertC2D() (sec 47). Two independent
    // implementations failing is enough to route around that subsystem.
    bool ensureStaging(uint32_t gw, uint32_t gh, uint32_t gfmt, uint32_t gstride);
    bool allocStaging();
    void releaseStaging();
    // YUV input whose layout differs from the Venus NV12 layout the engine is
    // configured for (camera buffers with another stride/scanline alignment,
    // or NV21) is repacked into the staging buffers.
    bool checkYuvLayout(const C2ConstGraphicBlock &block);
    bool mYuvChecked;
    bool mYuvCopy;
    bool mYuvNv21;
    bool mConversionNeeded;
    uint32_t mConvSrcFormat;
    std::vector<venc_ion> mStageIon;
    std::vector<void *> mStageBase;
    uint32_t mStageSize;
    uint32_t mStageYStride, mStageYScanlines, mStageUvStride;
    uint32_t mConvGw, mConvGh, mConvFmt, mConvStride;

    std::mutex mLock;
    std::condition_variable mCv;
    std::shared_ptr<C2BlockPool> mPool;      // saved from process(); needed to re-queue
    std::vector<OutSlot> mOutSlots;
    bool mOutputsSeeded;
    // Input frames in flight: driver-echoed timestamp -> C2 frameIndex.
    std::map<int64_t, uint64_t> mInFlight;
    uint32_t mInNext;          // ring over [0, mInBufCount)
    uint32_t mInFlightCount;

    // SPS/PPS, captured from the CODECCONFIG buffer and attached to the next
    // real frame's worklet.
    std::unique_ptr<C2StreamInitDataInfo::output> mCsd;

    // Buffer requirements the engine reports after configuration.
    OMX_U32 mInBufCount, mInBufSize;
    OMX_U32 mOutBufCount, mOutBufSize;

    C2_DO_NOT_COPY(C2VencComponent);
};

}  // namespace android

#endif  // C2_VENC_COMPONENT_H_
