# NVENC interface header

`nvEncodeAPI.h` is the NVIDIA interface header (line endings and final blank lines normalized) from
[FFmpeg/nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers/blob/eddcea9e27f6b772057c9b3f87de2cc1737faffc/include/ffnvcodec/nvEncodeAPI.h),
commit `eddcea9e27f6b772057c9b3f87de2cc1737faffc`, API 13.1.
Upstream download SHA-256: `8776fddcb8febc6aec4d73989b1f21831eb30306bc583da55b4bf0c14a1dc228`.
The permissive license is retained at the top of the header. No SDK sample
implementation, CUDA toolkit, import library or FFmpeg library is required.

The Windows D3D12 adapter loads the system NVENC runtime on demand and
rejects runtimes below the header API version. Other platforms do not
include this header or link the adapter through the frontend build.
