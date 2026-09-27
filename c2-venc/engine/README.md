# Vendored sm8250 V4L2 encoder engine

These files are a copy of

    hardware/qcom-caf/sm8250/media/mm-video-v4l2/vidc/venc/{src,inc}/
    hardware/qcom-caf/sm8250/media/mm-video-v4l2/vidc/common/inc/

taken because the **sm8250** media tree, not the sm8150 one, is the generation
whose V4L2 control vocabulary matches this device's kernel
(`kernel/xiaomi/sm8150/techpack/video`): 97% of the controls it uses exist
there, against 37% for the sm8150 tree. See `changes.md` section 45.

## Why a copy and not a filegroup

Referencing the sm8250 tree across Soong namespaces requires adding
`hardware/qcom-caf/sm8250` to `PRODUCT_SOONG_NAMESPACES`, which makes every
module in that tree eligible for the vendor partition and immediately collides
with the sm8150 audio HAL:

    module "libbatterylistener" found in multiple namespaces
    module "libvolumelistener"  found in multiple namespaces
    module "libqcompostprocbundle" found in multiple namespaces

The sm8250 OMX encoder is not built for this device, so nothing else consumes
this source and a copy costs no divergence in practice.

## If you update it

Re-copy from the sm8250 tree and re-apply the one local change: the
`VENC_HOST_C2` switch in `inc/video_encoder_device_v4l2.h` (host class
selection) and the `#ifndef VENC_HOST_C2` guard around the OMX message-thread
teardown in `venc_close()`. Both are marked with comments in place.
