// SPDX-License-Identifier: BSD-3-Clause
//
// Compatibility shim for V4L2_MPEG_VIDC_VIDEO_* enums that this build's
// 5.4 kernel UAPI headers don't expose. The values match historical
// Qualcomm-msm-5.4 sources so any downstream consumer that lives in the
// kernel driver and recognises these IDs still sees the expected value.
//
// Symbols already defined by the kernel's msm_vidc_utils.h are guarded by
// #ifndef and become no-ops if the kernel is later patched to provide them.
//
// Status of the enum families:
//   * HEVC profile     : not in kernel  -> defined here
//   * HEVC level       : not in kernel  -> defined here
//   * VP9 profile      : partially missing (P0, P2_10) -> defined here
//   * TME              : QC-specific, no upstream      -> defined here

#ifndef __VIDC_V4L2_COMPAT_H__
#define __VIDC_V4L2_COMPAT_H__

// Pull in the kernel-provided VIDC enums first, so the #ifndef guards below
// suppress redefinitions of anything the kernel already declares. Resolves
// via the -Iout/.../generated_kernel_includes/gen/usr/include/vidc/ path.
#include <media/msm_vidc_utils.h>

// ---------------------------------------------------------------------------
// HEVC profile
// ---------------------------------------------------------------------------
#ifndef V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN
#define V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN            0
#endif
#ifndef V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN10
#define V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN10          1
#endif
#ifndef V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN_STILL_PIC
#define V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN_STILL_PIC  2
#endif

// ---------------------------------------------------------------------------
// HEVC level (tier-level combined encoding from msm-5.4 era)
// ---------------------------------------------------------------------------
#ifndef V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_1
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_1    0
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_2    1
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_2_1  2
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_3    3
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_3_1  4
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_4    5
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_4_1  6
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_5    7
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_5_1  8
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_5_2  9
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_6   10
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_6_1 11
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_6_2 12
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_1   13
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_2   14
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_2_1 15
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_3   16
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_3_1 17
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_4   18
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_4_1 19
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_5   20
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_5_1 21
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_5_2 22
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_6   23
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_6_1 24
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_6_2 25
#endif

// ---------------------------------------------------------------------------
// VP9 profile (LEVEL_* are already in kernel msm_vidc_utils.h)
// ---------------------------------------------------------------------------
#ifndef V4L2_MPEG_VIDC_VIDEO_VP9_PROFILE_P0
#define V4L2_MPEG_VIDC_VIDEO_VP9_PROFILE_P0     1
#endif
#ifndef V4L2_MPEG_VIDC_VIDEO_VP9_PROFILE_P2_10
#define V4L2_MPEG_VIDC_VIDEO_VP9_PROFILE_P2_10  2
#endif

// ---------------------------------------------------------------------------
// TME (Tile Motion Estimation) - QC-specific, no upstream V4L2 equivalent
// ---------------------------------------------------------------------------
#ifndef V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_0
#define V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_0     0
#define V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_1     1
#define V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_2     2
#define V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_3     3
#endif
#ifndef V4L2_MPEG_VIDC_VIDEO_TME_LEVEL_INTEGER
#define V4L2_MPEG_VIDC_VIDEO_TME_LEVEL_INTEGER 0
#endif

// V4L2 pixel format fourcc for TME (QC-specific). msm-5.4 stock value.
#ifndef V4L2_PIX_FMT_TME
#include <linux/videodev2.h>
#define V4L2_PIX_FMT_TME v4l2_fourcc('T', 'M', 'E', '0')
#endif

// ---------------------------------------------------------------------------
// V4L2 control IDs (QC-specific) - msm-5.4 numeric values
// ---------------------------------------------------------------------------
#include <linux/v4l2-controls.h>
#ifndef V4L2_CID_MPEG_MSM_VIDC_BASE
#define V4L2_CID_MPEG_MSM_VIDC_BASE (V4L2_CID_MPEG_BASE + 2000)
#endif

// Defined relative to V4L2_CID_MPEG_MSM_VIDC_BASE so values are unique.
// The kernel driver in this build doesn't recognise these IDs anyway (they
// were dropped from msm-5.4 UAPI); the shims exist purely so userspace
// compiles. Runtime calls using these will return -EINVAL but won't crash.
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE
#define V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE       (V4L2_CID_MPEG_MSM_VIDC_BASE + 1)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL
#define V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL    (V4L2_CID_MPEG_MSM_VIDC_BASE + 2)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_VP9_PROFILE
#define V4L2_CID_MPEG_VIDC_VIDEO_VP9_PROFILE        (V4L2_CID_MPEG_MSM_VIDC_BASE + 3)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD
#define V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD         (V4L2_CID_MPEG_MSM_VIDC_BASE + 10)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES
#define V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES       (V4L2_CID_MPEG_MSM_VIDC_BASE + 11)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES
#define V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES       (V4L2_CID_MPEG_MSM_VIDC_BASE + 12)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER
#define V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER       (V4L2_CID_MPEG_MSM_VIDC_BASE + 20)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_CAPS
#define V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_CAPS      (V4L2_CID_MPEG_MSM_VIDC_BASE + 30)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_BLUR_WIDTH
#define V4L2_CID_MPEG_VIDC_VIDEO_BLUR_WIDTH         (V4L2_CID_MPEG_MSM_VIDC_BASE + 31)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE_CAPS
#define V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE_CAPS   (V4L2_CID_MPEG_MSM_VIDC_BASE + 32)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_TME_PAYLOAD_VERSION
#define V4L2_CID_MPEG_VIDC_VIDEO_TME_PAYLOAD_VERSION (V4L2_CID_MPEG_MSM_VIDC_BASE + 40)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_NONE
#define V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_NONE      0
#endif

// Frame QP controls (encoder)
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP
#define V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP         (V4L2_CID_MPEG_MSM_VIDC_BASE + 50)
#define V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP         (V4L2_CID_MPEG_MSM_VIDC_BASE + 51)
#define V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP         (V4L2_CID_MPEG_MSM_VIDC_BASE + 52)
#define V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP_MIN     (V4L2_CID_MPEG_MSM_VIDC_BASE + 53)
#define V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP_MIN     (V4L2_CID_MPEG_MSM_VIDC_BASE + 54)
#define V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP_MIN     (V4L2_CID_MPEG_MSM_VIDC_BASE + 55)
#define V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP_MAX     (V4L2_CID_MPEG_MSM_VIDC_BASE + 56)
#define V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP_MAX     (V4L2_CID_MPEG_MSM_VIDC_BASE + 57)
#define V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP_MAX     (V4L2_CID_MPEG_MSM_VIDC_BASE + 58)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_LAYER_ID
#define V4L2_CID_MPEG_VIDC_VIDEO_LAYER_ID           (V4L2_CID_MPEG_MSM_VIDC_BASE + 60)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_QP_MASK
#define V4L2_CID_MPEG_VIDC_VIDEO_QP_MASK            (V4L2_CID_MPEG_MSM_VIDC_BASE + 61)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_TME_PROFILE
#define V4L2_CID_MPEG_VIDC_VIDEO_TME_PROFILE        (V4L2_CID_MPEG_MSM_VIDC_BASE + 62)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_MULTI_SLICE_DELIVERY_MODE
#define V4L2_CID_MPEG_VIDEO_MULTI_SLICE_DELIVERY_MODE (V4L2_CID_MPEG_BASE + 1100)
#endif
#ifndef V4L2_MPEG_VIDEO_BITRATE_MODE_RC_OFF
#define V4L2_MPEG_VIDEO_BITRATE_MODE_RC_OFF          2
#endif
#ifndef V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN
#define V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN           0xff
#endif
#ifndef V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_UNKNOWN
#define V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_UNKNOWN      0xff
#endif
// VP9_LEVEL_UNUSED is in kernel; alias kept for symmetry
#ifndef V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_UNUSED
#define V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_UNUSED        0
#endif

// More encoder CIDs
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_TME_LEVEL
#define V4L2_CID_MPEG_VIDC_VIDEO_TME_LEVEL           (V4L2_CID_MPEG_MSM_VIDC_BASE + 63)
#endif
#ifndef V4L2_CID_MPEG_VIDC_IMG_GRID_ENABLE
#define V4L2_CID_MPEG_VIDC_IMG_GRID_ENABLE           (V4L2_CID_MPEG_MSM_VIDC_BASE + 70)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE_CYCLIC
#define V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE_CYCLIC (V4L2_CID_MPEG_MSM_VIDC_BASE + 71)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_FRAME_QUALITY
#define V4L2_CID_MPEG_VIDC_VIDEO_FRAME_QUALITY       (V4L2_CID_MPEG_MSM_VIDC_BASE + 72)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_REQUEST_IFRAME
#define V4L2_CID_MPEG_VIDC_VIDEO_REQUEST_IFRAME      (V4L2_CID_MPEG_MSM_VIDC_BASE + 73)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_VENC_BITRATE_TYPE
#define V4L2_CID_MPEG_VIDC_VIDEO_VENC_BITRATE_TYPE   (V4L2_CID_MPEG_MSM_VIDC_BASE + 74)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_FLIP
#define V4L2_CID_MPEG_VIDC_VIDEO_FLIP                (V4L2_CID_MPEG_MSM_VIDC_BASE + 75)
#endif
#ifndef V4L2_CID_MPEG_VIDC_VENC_PARAM_LAYER_BITRATE
#define V4L2_CID_MPEG_VIDC_VENC_PARAM_LAYER_BITRATE  (V4L2_CID_MPEG_MSM_VIDC_BASE + 80)
#endif

// Search range CIDs (X/Y for I/P/B frames)
#ifndef V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_X_RANGE
#define V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_X_RANGE      (V4L2_CID_MPEG_MSM_VIDC_BASE + 90)
#define V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_Y_RANGE      (V4L2_CID_MPEG_MSM_VIDC_BASE + 91)
#define V4L2_CID_MPEG_VIDC_VIDEO_PFRAME_X_RANGE      (V4L2_CID_MPEG_MSM_VIDC_BASE + 92)
#define V4L2_CID_MPEG_VIDC_VIDEO_PFRAME_Y_RANGE      (V4L2_CID_MPEG_MSM_VIDC_BASE + 93)
#define V4L2_CID_MPEG_VIDC_VIDEO_BFRAME_X_RANGE      (V4L2_CID_MPEG_MSM_VIDC_BASE + 94)
#define V4L2_CID_MPEG_VIDC_VIDEO_BFRAME_Y_RANGE      (V4L2_CID_MPEG_MSM_VIDC_BASE + 95)
#endif

// Output-order CID values (enum values, not CIDs)
#ifndef V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY
#define V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY   0
#endif
#ifndef V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DECODE
#define V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DECODE    1
#endif

// ---------------------------------------------------------------------------
// V4L2 extradata types (QC-specific)
// ---------------------------------------------------------------------------
#ifndef V4L2_MPEG_VIDC_EXTRADATA_ROI_QP
#define V4L2_MPEG_VIDC_EXTRADATA_ROI_QP             0x0007
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_HDR10PLUS_METADATA
#define V4L2_MPEG_VIDC_EXTRADATA_HDR10PLUS_METADATA 0x0020
#endif

// Decoder-side extradata enum values
#ifndef V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO
#define V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO        0x0001
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO
#define V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO     0x0002
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE
#define V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE          0x0003
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB
#define V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB    0x0004
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_FRAME_QP
#define V4L2_MPEG_VIDC_EXTRADATA_FRAME_QP            0x0005
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_LTR
#define V4L2_MPEG_VIDC_EXTRADATA_LTR                 0x0006
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_OUTPUT_CROP
#define V4L2_MPEG_VIDC_EXTRADATA_OUTPUT_CROP         0x0008
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW
#define V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW      0x0009
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI
#define V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI  0x000A
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP
#define V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP       0x000B
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_STREAM_USERDATA
#define V4L2_MPEG_VIDC_EXTRADATA_STREAM_USERDATA     0x000C
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP
#define V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP           0x000D
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING
#define V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING   0x000E
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_VUI_DISPLAY
#define V4L2_MPEG_VIDC_EXTRADATA_VUI_DISPLAY         0x000F
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_VPX_COLORSPACE
#define V4L2_MPEG_VIDC_EXTRADATA_VPX_COLORSPACE      0x0010
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_DISPLAY_COLOUR_SEI
#define V4L2_MPEG_VIDC_EXTRADATA_DISPLAY_COLOUR_SEI  0x0011
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI
#define V4L2_MPEG_VIDC_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI 0x0012
#endif
#ifndef V4L2_MPEG_VIDC_EXTRADATA_UBWC_CR_STATS_INFO
#define V4L2_MPEG_VIDC_EXTRADATA_UBWC_CR_STATS_INFO  0x0013
#endif

// MSM_VIDC_EXTRADATA_METADATA_* (vendor-side equivalents)
#ifndef MSM_VIDC_EXTRADATA_METADATA_LTR
#define MSM_VIDC_EXTRADATA_METADATA_LTR              0x7F100004
#endif
#ifndef MSM_VIDC_EXTRADATA_METADATA_MBI
#define MSM_VIDC_EXTRADATA_METADATA_MBI              0x7F100005
#endif
#ifndef MSM_VIDC_EXTRADATA_UBWC_CR_STAT_INFO
#define MSM_VIDC_EXTRADATA_UBWC_CR_STAT_INFO         0x7F100007
#endif

// MSM_VIDC_FRAMEPACK_* (decoder frame packing arrangement enum)
#ifndef MSM_VIDC_FRAMEPACK_SIDE_BY_SIDE
#define MSM_VIDC_FRAMEPACK_SIDE_BY_SIDE              3
#endif
#ifndef MSM_VIDC_FRAMEPACK_TOP_BOTTOM
#define MSM_VIDC_FRAMEPACK_TOP_BOTTOM                4
#endif

// ---------------------------------------------------------------------------
// V4L2 QCOM buffer flags - bit positions in v4l2_buffer.flags
// ---------------------------------------------------------------------------
#ifndef V4L2_QCOM_BUF_FLAG_CODECCONFIG
#define V4L2_QCOM_BUF_FLAG_CODECCONFIG  0x00020000
#endif
#ifndef V4L2_QCOM_BUF_FLAG_EOS
#define V4L2_QCOM_BUF_FLAG_EOS          0x00040000
#endif
#ifndef V4L2_QCOM_BUF_FLAG_PERF_MODE
#define V4L2_QCOM_BUF_FLAG_PERF_MODE    0x00400000
#endif
#ifndef V4L2_QCOM_BUF_FLAG_READONLY
#define V4L2_QCOM_BUF_FLAG_READONLY     0x00800000
#endif
#ifndef V4L2_QCOM_BUF_INPUT_UNSUPPORTED
#define V4L2_QCOM_BUF_INPUT_UNSUPPORTED 0x01000000
#endif

// ---------------------------------------------------------------------------
// V4L2 QCOM decoder/encoder command codes (extensions of v4l2_decoder_cmd)
// ---------------------------------------------------------------------------
#ifndef V4L2_QCOM_CMD_FLUSH
#define V4L2_QCOM_CMD_FLUSH             0x40
#endif
#ifndef V4L2_QCOM_CMD_FLUSH_OUTPUT
#define V4L2_QCOM_CMD_FLUSH_OUTPUT      0x00000001
#endif
#ifndef V4L2_QCOM_CMD_FLUSH_CAPTURE
#define V4L2_QCOM_CMD_FLUSH_CAPTURE     0x00000002
#endif
#ifndef V4L2_QCOM_CMD_SESSION_CONTINUE
#define V4L2_QCOM_CMD_SESSION_CONTINUE  0x41
#endif

#endif  // __VIDC_V4L2_COMPAT_H__
