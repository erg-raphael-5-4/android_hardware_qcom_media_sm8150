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

#endif  // __VIDC_V4L2_COMPAT_H__
