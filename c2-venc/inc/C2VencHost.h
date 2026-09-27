/*
 * Codec2 host for the sm8150 V4L2 video encoder engine.
 *
 * venc_dev (mm-video-v4l2/vidc/venc) contains ~8000 lines of msm_vidc V4L2
 * handling: the 54 proprietary V4L2_CID_MPEG_VIDC_* controls, buffer
 * negotiation, extradata layout and the async message thread. That code is
 * correct for this SoC and is reused here verbatim rather than rewritten.
 *
 * It reaches back into whatever hosts it for exactly 17 things -- ION
 * allocation, the async message callback, and a little port state. This class
 * provides those 17, so venc_dev can be driven from a Codec2 component instead
 * of an OMX one. venc_dev selects between the two at compile time via
 * VENC_HOST_C2; nothing in the OMX path changes.
 *
 * OMX_* types still appear below. They are plain structs from the AOSP media
 * headers, and venc_dev's signatures are written in terms of them. Keeping
 * them avoids touching 825 references inside the engine for no behavioural
 * gain -- this is a Codec2 component that happens to speak OMX's type
 * vocabulary internally, not an OMX component.
 */

#ifndef C2_VENC_HOST_H_
#define C2_VENC_HOST_H_

#include <pthread.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Video.h>
#include <OMX_QCOMExtns.h>

// Brings in linux/msm_ion.h, ion/ion.h and linux/ion.h, which define the heap
// ids and ion_* helpers the allocation paths below use. Including the engine's
// own common header rather than the kernel ones directly keeps us on exactly
// the same definitions it compiles against.
#include "omx_video_common.h"
#include "extra_data_handler.h"

struct venc_ion;
struct pmem;
struct private_handle_t;
class venc_dev;

// omx_video_base.h:133. Repeated rather than including that header, which
// would pull in the entire OMX component class.
#ifndef MAX_NUM_INPUT_BUFFERS
#define MAX_NUM_INPUT_BUFFERS 64
#endif

class C2VencHost {
public:
    C2VencHost();
    virtual ~C2VencHost();

    // --- ION allocation (6) ---------------------------------------------
    bool alloc_map_ion_memory(int size, struct venc_ion *ion_info, int flag);
    void free_ion_memory(struct venc_ion *buf_ion_info);
    char *ion_map(int fd, int len);
    OMX_ERRORTYPE ion_unmap(int fd, void *bufaddr, int len);
    bool allocate_native_handle;

    // Kept because the ION paths below use it internally, even though the
    // sm8250 engine never calls it directly (the sm8150 one did).
    void do_cache_operations(int fd);

    // --- async message callback (4) -------------------------------------
    // venc_dev's message thread calls this for every EBD/FBD/flush/error.
    // The component installs a sink; until it does, messages are dropped
    // rather than crashing, which matters during teardown.
    virtual int async_message_process(void *context, void *message);

    pthread_t msg_thread_id;
    bool msg_thread_created;
    volatile bool msg_thread_stop;

    // --- state venc_dev reads (4) ---------------------------------------
    virtual bool is_secure_session(void);

    OMX_PARAM_PORTDEFINITIONTYPE m_sInPortDef;
    OMX_U32 m_sExtraData;

    // sm8250 engine additions. m_sOutPortDef and m_sParamAVC are read back by
    // the engine while configuring the output port, so they must track what we
    // pass to venc_set_param(), not be left zeroed.
    OMX_PARAM_PORTDEFINITIONTYPE m_sOutPortDef;
    OMX_VIDEO_PARAM_AVCTYPE m_sParamAVC;
    int32_t m_no_vpss;
    char m_platform[OMX_MAX_STRINGNAME_SIZE];

    // Reached only from video_encoder_device_v4l2_params.cpp. m_etb_count and
    // m_etb_timestamp are the engine's own empty-this-buffer bookkeeping and it
    // both reads and writes them, so they live here rather than in the
    // component.
    OMX_U32 m_nOperatingRate;
    uint64_t m_etb_count;
    OMX_TICKS m_etb_timestamp;

    // FastCV is the CPU-side rotation/flip helper the OMX component uses when
    // the encoder cannot do the transform itself. A Codec2 input frame is
    // already a dmabuf the encoder consumes directly, so there is nothing to
    // convert and no library to bring up.
    bool is_flip_conv_needed(private_handle_t *handle);
    void initFastCV();

    // --- extradata (3) ---------------------------------------------------
    client_extradata_info m_client_in_extradata_info;
    client_extradata_info m_client_out_extradata_info;
    OMX_BUFFERHEADERTYPE *m_client_output_extradata_mem_ptr;

    // --- buffer bookkeeping for the async message thread (7) -------------
    // venc_dev::async_venc_message_thread() maps a dequeued V4L2 buffer index
    // straight back to a header through these arrays. Providing them, rather
    // than reimplementing the 220-line thread for Codec2, is what keeps the
    // engine a single shared source file.
    venc_dev *handle;
    OMX_BUFFERHEADERTYPE *m_inp_mem_ptr;
    OMX_BUFFERHEADERTYPE *m_out_mem_ptr;
    struct pmem *m_pOutput_pmem;
    OMX_BUFFERHEADERTYPE meta_buffer_hdr[MAX_NUM_INPUT_BUFFERS];
    bool mUseProxyColorFormat;
    bool mUsesColorConversion;

    // The async thread indexes these arrays directly with the dequeued V4L2
    // buffer index and hands the resulting header back as venc_msg clientdata,
    // so they must be real storage owned by the host -- not per-frame locals.
    // Allocated once the engine reports its buffer counts.
    bool allocBufferArrays(uint32_t inCount, uint32_t outCount);
    void freeBufferArrays();
    uint32_t mInArrayCount;
    uint32_t mOutArrayCount;

protected:
    bool secure_session;
};

#endif  // C2_VENC_HOST_H_
