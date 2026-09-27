# c2-venc: Codec2 AVC/HEVC encoders for sm8150

Two Codec2 components, `c2.sm8150.avc.encoder` and `c2.sm8150.hevc.encoder`,
served from their own HIDL @1.2 `IComponentStore` instance (`default1`) by
`sm8150-c2-venc-service`. They drive the msm_vidc V4L2 encoder through the QTI
`venc_dev` engine, compiled with `-DVENC_HOST_C2` so the engine's host is a
Codec2 component instead of an OMX one.

## Why this exists

The prebuilt QTI Codec2 encoders on this device only work for camera (NV12)
input. The AVC one fails on composited RGBA surfaces — screen recording and
any app that encodes a `Surface` — because its C2D blit does not work here;
the HEVC one fails before its first frame. Both are still listed in
`media_codecs_c2.xml` after ours, so they remain reachable by name.

## Layout

| Path | What |
|---|---|
| `engine/` | Vendored copy of the **sm8250** `venc_dev` engine. See `engine/README.md` for why that generation and why a copy. Two local edits, both marked in place: the `VENC_HOST_C2` host-class switch and a guard around the OMX message-thread teardown in `venc_close()`. |
| `inc/C2VencHost.h`, `src/C2VencHost.cpp` | The ~26 members the engine reaches back into its host for: ION allocation (transcribed from `omx_video_base.cpp`), the async message callback, port state, and the buffer-header arrays the engine's message thread indexes by V4L2 buffer index. |
| `inc/C2VencComponent.h`, `src/C2VencComponent.cpp` | `SimpleC2Component` + `C2VencHost`. One class, parameterised by `Codec {AVC, HEVC}`. |
| `src/C2VencStore.cpp` | `C2ComponentStore` advertising both components. |
| `src/service.cpp` | The HAL binary: HIDL `V1_2::utils::ComponentStore`, seccomp, a SIGSYS reporter that logs the blocked syscall number before re-raising. |
| `seccomp_policy/` | Superset of the QTI Codec2 policy and the platform `mediacodec.policy`; the AOSP software-codec policy is not enough for a hardware codec. |

## How frames flow

- **Output side is independent of input**, as in the OMX component: every
  capture buffer is queued to the driver on the first `process()` and
  re-queued the moment the encoder returns it. msm_vidc does not start until
  its firmware minimum of capture buffers is queued, so "one output per input
  frame" stalls forever.
- **Input** is a ring over the input pool bounded by the in-flight count;
  the driver echoes the timestamp, which maps back to the C2 `frameIndex`.
- **RGBA input** (screen recording) is converted to Venus-aligned NV12 on the
  CPU with `libyuv::ABGRToNV12`, exactly as AOSP's `C2SoftAvcEnc` does. Not
  C2D: two independent C2D implementations fail on this device.
- **SPS/PPS** arrive as an `OMX_BUFFERFLAG_CODECCONFIG` buffer and are attached
  to the next frame as `C2StreamInitDataInfo::output`; delivering them as
  frame data produces an undecodable track.

## Design decisions worth knowing

- **Own store + own service, not `CreateCodec2Factory()`.** The only store
  that `dlopen`s component libraries is `C2PlatformComponentStore` inside the
  swcodec APEX, and the device's vendor store is inside the QTI blob. A vendor
  component therefore needs its own store behind its own HAL instance — the
  `external/v4l2_codec2` shape.
- **HIDL @1.2, not AIDL.** `Codec2Client::CacheServiceNames()` enumerates one
  transport globally; AIDL is selected only with `ro.vendor.api_level >= 202404`
  or the aconfig flag, and raphael ships API level 30. Declaring 1.0 in the
  manifest while registering a 1.2 store is rejected by hwservicemanager.
- **XML order beats Codec2 rank.** `createEncoderByType()` takes the first
  `media_codecs_c2.xml` match. `persist.vendor.c2venc.prefer` only changes our
  rank within Codec2 and is a debugging aid, not the selection mechanism.
- **`.rc` runs as `user mediacodec`** because the binary is labelled
  `mediacodec_exec`; `task_profiles ProcessCapacityHigh` replaces the older
  `writepid /dev/cpuset/...`.

## Testing traps

- `stop`/`start` the service by its `.rc` name, `vendor-sm8150-c2-venc-hal`,
  or just kill the pid; init restarts it.
- mediaserver caches `MediaCodecList`: after changing the XML, kill
  mediaserver too.
- `adb remount` overlays (`/mnt/scratch/overlay`) shadow a flashed `/vendor`.

## Not yet exercised

ByteBuffer-input encoders (video calling), 4K, and mid-stream bitrate /
sync-frame requests, which the interface accepts but the component does not
yet act on.
