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
