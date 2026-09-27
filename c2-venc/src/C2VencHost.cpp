/*
 * The 17 members venc_dev needs from its host. See C2VencHost.h for why this
 * exists rather than a rewrite of the engine.
 *
 * The ION paths are lifted from omx_video_base.cpp unchanged in behaviour --
 * same heap ids, same secure alignment, same cached-buffer rule for vanilla
 * NV12. Divergence here would show up as corrupt frames rather than a clean
 * failure, so it is deliberately a transcription, not an improvement.
 */

#include "C2VencHost.h"

#include <new>

#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/dma-buf.h>

#include <log/log.h>

#include <gralloc_priv.h>
#include <qdMetaData.h>

#include "video_encoder_device_v4l2.h"

#undef LOG_TAG
#define LOG_TAG "C2VencHost"

// These mirror omx_video_base.cpp:86-105 exactly. Note that the header's
// MEM_HEAP_ID (ION_IOMMU_HEAP_ID) is a decoy -- the .cpp #undefs it and picks
// the heap based on the content-protection model. We build MASTER_SIDE_CP, the
// same as libOmxVenc, so take that branch. Getting this wrong would put secure
// allocations on the wrong heap.
#ifndef SZ_4K
#define SZ_4K 0x1000
#endif
#ifndef SZ_1M
#define SZ_1M 0x100000
#endif

#ifdef SLAVE_SIDE_CP
#define VENC_MEM_HEAP_ID ION_CP_MM_HEAP_ID
#define VENC_SECURE_ALIGN SZ_1M
#else  // MASTER_SIDE_CP
#define VENC_MEM_HEAP_ID ION_SECURE_HEAP_ID
#define VENC_SECURE_ALIGN SZ_4K
#endif

/*
 * venc_dev calls this free function (declared in omx_video_base.h:147, defined
 * in omx_video_base.cpp:5219) when queueing an input buffer, to spot
 * interlaced UBWC content. It is a leaf helper with no OMX state, so it is
 * transcribed here rather than linking libOmxVenc purely to reach it.
 *
 * Behaviour is identical to the original, including the quirk that a failed
 * getMetaData() is treated as "not interlaced" rather than an error.
 */
bool is_ubwc_interlaced(private_handle_t *handle) {
    int interlace_flag = 0;

    if (getMetaData(const_cast<private_handle_t *>(handle), GET_PP_PARAM_INTERLACED,
                    &interlace_flag)) {
        interlace_flag = 0;
    }
    return (handle->format == HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC) && !!interlace_flag;
}

C2VencHost::C2VencHost()
    : allocate_native_handle(false),
      msg_thread_id(0),
      msg_thread_created(false),
      msg_thread_stop(false),
      m_sExtraData(0),
      m_no_vpss(0),
      m_nOperatingRate(0),
      m_etb_count(0),
      m_etb_timestamp(0),
      m_client_output_extradata_mem_ptr(nullptr),
      handle(nullptr),
      m_inp_mem_ptr(nullptr),
      m_out_mem_ptr(nullptr),
      m_pOutput_pmem(nullptr),
      mUseProxyColorFormat(false),
      mUsesColorConversion(false),
      mInArrayCount(0),
      mOutArrayCount(0),
      secure_session(false) {
    memset(meta_buffer_hdr, 0, sizeof(meta_buffer_hdr));
    memset(m_platform, 0, sizeof(m_platform));

    memset(&m_sOutPortDef, 0, sizeof(m_sOutPortDef));
    m_sOutPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    m_sOutPortDef.nPortIndex = 1;  // PORT_INDEX_OUT
    m_sOutPortDef.eDir = OMX_DirOutput;
    m_sOutPortDef.eDomain = OMX_PortDomainVideo;

    memset(&m_sParamAVC, 0, sizeof(m_sParamAVC));
    m_sParamAVC.nSize = sizeof(OMX_VIDEO_PARAM_AVCTYPE);
    m_sParamAVC.nPortIndex = 1;  // PORT_INDEX_OUT
    memset(&m_sInPortDef, 0, sizeof(m_sInPortDef));
    m_sInPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    m_sInPortDef.nPortIndex = 0;  // PORT_INDEX_IN
    m_sInPortDef.eDir = OMX_DirInput;
    m_sInPortDef.eDomain = OMX_PortDomainVideo;
}

C2VencHost::~C2VencHost() {
    freeBufferArrays();
}

bool C2VencHost::allocBufferArrays(uint32_t inCount, uint32_t outCount) {
    freeBufferArrays();
    if (inCount == 0 || outCount == 0) {
        ALOGE("refusing to allocate buffer arrays for %u/%u", inCount, outCount);
        return false;
    }
    m_inp_mem_ptr = new (std::nothrow) OMX_BUFFERHEADERTYPE[inCount]();
    m_out_mem_ptr = new (std::nothrow) OMX_BUFFERHEADERTYPE[outCount]();
    m_pOutput_pmem = new (std::nothrow) struct pmem[outCount]();
    if (!m_inp_mem_ptr || !m_out_mem_ptr || !m_pOutput_pmem) {
        ALOGE("buffer array allocation failed");
        freeBufferArrays();
        return false;
    }
    for (uint32_t i = 0; i < inCount; ++i) {
        m_inp_mem_ptr[i].nSize = sizeof(OMX_BUFFERHEADERTYPE);
    }
    for (uint32_t i = 0; i < outCount; ++i) {
        m_out_mem_ptr[i].nSize = sizeof(OMX_BUFFERHEADERTYPE);
    }
    mInArrayCount = inCount;
    mOutArrayCount = outCount;
    return true;
}

void C2VencHost::freeBufferArrays() {
    delete[] m_inp_mem_ptr;  m_inp_mem_ptr = nullptr;
    delete[] m_out_mem_ptr;  m_out_mem_ptr = nullptr;
    delete[] m_pOutput_pmem; m_pOutput_pmem = nullptr;
    mInArrayCount = 0;
    mOutArrayCount = 0;
}

bool C2VencHost::is_flip_conv_needed(private_handle_t *handle) {
    // Never. The Codec2 input is a dmabuf handed straight to the encoder; the
    // OMX component's CPU flip path does not apply.
    (void)handle;
    return false;
}

void C2VencHost::initFastCV() {
    // No-op: see is_flip_conv_needed().
}

bool C2VencHost::is_secure_session(void) {
    return secure_session;
}

int C2VencHost::async_message_process(void *context, void *message) {
    // Overridden by the component. venc_dev's message thread can still be
    // draining when the component has detached, so swallow rather than fault.
    (void)context;
    (void)message;
    return 0;
}

void C2VencHost::do_cache_operations(int fd) {
    if (fd < 0) {
        return;
    }

    struct dma_buf_sync dma_buf_sync_data[2];
    dma_buf_sync_data[0].flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
    dma_buf_sync_data[1].flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;

    for (unsigned int i = 0; i < 2; i++) {
        int rc = ioctl(fd, DMA_BUF_IOCTL_SYNC, &dma_buf_sync_data[i]);
        if (rc < 0) {
            ALOGE("Failed DMA_BUF_IOCTL_SYNC %s fd : %d", i == 0 ? "start" : "end", fd);
            return;
        }
    }
}

char *C2VencHost::ion_map(int fd, int len) {
    char *bufaddr = (char *)mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bufaddr != MAP_FAILED) {
        do_cache_operations(fd);
    }
    return bufaddr;
}

OMX_ERRORTYPE C2VencHost::ion_unmap(int fd, void *bufaddr, int len) {
    do_cache_operations(fd);
    if (munmap(bufaddr, len) == -1) {
        ALOGE("munmap failed: %s", strerror(errno));
        return OMX_ErrorInsufficientResources;
    }
    return OMX_ErrorNone;
}

bool C2VencHost::alloc_map_ion_memory(int size, struct venc_ion *ion_info, int flag) {
    if (size <= 0 || !ion_info) {
        ALOGE("Invalid input to alloc_map_ion_memory");
        return false;
    }

    ion_info->data_fd = -1;
    ion_info->dev_fd = ion_open();
    if (ion_info->dev_fd <= 0) {
        ALOGE("ION Device open() failed");
        return false;
    }

    if (secure_session) {
        ion_info->alloc_data.len = (size + (VENC_SECURE_ALIGN - 1)) & ~(VENC_SECURE_ALIGN - 1);
        ion_info->alloc_data.flags = flag;
        ion_info->alloc_data.heap_id_mask = ION_HEAP(VENC_MEM_HEAP_ID);
        if (ion_info->alloc_data.flags & ION_FLAG_CP_BITSTREAM) {
            ion_info->alloc_data.heap_id_mask |= ION_HEAP(ION_SECURE_DISPLAY_HEAP_ID);
        }
    } else {
        ion_info->alloc_data.len = (size + (SZ_4K - 1)) & ~(SZ_4K - 1);
        ion_info->alloc_data.flags = (flag & ION_FLAG_CACHED);

        // Vanilla NV12 needs caching for the colour-alignment path to perform;
        // this mirrors omx_video_base.cpp and is not an optimisation choice.
        if (m_sInPortDef.format.video.eColorFormat == OMX_COLOR_FormatYUV420SemiPlanar) {
            ion_info->alloc_data.flags = ION_FLAG_CACHED;
        }
        ion_info->alloc_data.heap_id_mask =
                (ION_HEAP(VENC_MEM_HEAP_ID) | ION_HEAP(ION_SYSTEM_HEAP_ID));
    }

    int rc = ion_alloc_fd(ion_info->dev_fd, ion_info->alloc_data.len, 0,
                          ion_info->alloc_data.heap_id_mask, ion_info->alloc_data.flags,
                          &ion_info->data_fd);
    if (rc || ion_info->data_fd < 0) {
        ALOGE("ION alloc failed 0x%x", rc);
        ion_close(ion_info->dev_fd);
        ion_info->data_fd = -1;
        ion_info->dev_fd = -1;
        return false;
    }

    return true;
}

void C2VencHost::free_ion_memory(struct venc_ion *buf_ion_info) {
    if (!buf_ion_info) {
        ALOGE("Invalid input to free_ion_memory");
        return;
    }
    if (buf_ion_info->data_fd >= 0) {
        close(buf_ion_info->data_fd);
        buf_ion_info->data_fd = -1;
    }
    if (buf_ion_info->dev_fd >= 0) {
        ion_close(buf_ion_info->dev_fd);
        buf_ion_info->dev_fd = -1;
    }
}
