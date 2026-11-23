The Vulkan viewer can sample CPU-decoded 8-bit 4:2:0 planes directly from
mapped, device-local linear R8 images. It packs Y, Cb and Cr into an atlas;
the shared Slang compiler performs conversion at each original-frame read.
RGB quantization precedes filtering, and viewer HDR conversion follows it.
Logical texture dimensions and level-zero integer reads remain unchanged.

The four-slot atlas retains no decoder pointers. Host writes require completed
GPU readers, checked without waits; a full ring drops the offered frame.
The first frame initializes the ordinary RGB fallback. Compatible subsequent
frames skip its packing and conversion dispatch. An RGB consumer can later
materialize from the retained atlas. This still copies CPU decoder planes
once; it is not a decoder-to-display zero-copy claim.

Eligibility requires mapped device-local sampled images, clamp-to-edge input,
no input mipmaps and supported sampler operations. Temporal history, feedback,
forward/self pass references and presets requiring a separate LUT upload keep
the existing RGB path. Native decoder imports retain their existing path.
Only the Vulkan renderer opts into the atlas contract; shared compiler output
is exercised for SPIR-V, GLSL/GLES, HLSL and MSL.

Run `python3 samples/gfx/rslc_remap/planar_pixels.py` for real-device pixel
comparisons against quantized RGB input. `--bench` measures repeated sampling
kernels, excluding packing and the RGB conversion dispatch. These timings do
not represent end-to-end playback speedups. Set
`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` to include Vulkan validation.
Ownership, refusal, partial allocation and backpressure checks are in
`python3 samples/gfx/threaded_video/vulkan_media_pool.py`; the actual viewer
callback is checked by `viewer_vulkan_hdr.py` in that directory.
