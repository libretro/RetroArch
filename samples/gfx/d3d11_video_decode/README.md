# HEVC DXVA submission buffers

`dxva_hevc.h` prepares little-endian `DXVA_PicParams_HEVC`, effective
`DXVA_Qmatrix_HEVC` and short slice descriptors without Windows SDK
headers, allocations or pixel reads. Picture geometry is coded geometry;
display cropping belongs to the GPU frame/presentation adapter.

The initial scope is Main/Main10 4:2:0 with no PCM, tiles or dependent
slice segments. Reference-set indices address the supplied reference
list; texture slots and short/long-term flags stay separate. Invalid
ranges, reserved/unsupported flags and conflicting reference sets fail
before output is populated. Picture/scaling failures clear fixed output.
Output buffers must not overlap input descriptors.

Scaling lists must already include SPS/PPS inference and defaults in
HEVC `ScalingList` order, plus DC values. Submit a matrix buffer only
when scaling lists are enabled; disabled scaling lists require no buffer.
The rh265 parser's raster coefficients need explicit conversion in its
future hardware adapter. The slice helper borrows complete VCL NALs with
Annex-B prefixes, preserves emulation-prevention bytes, and accounts for
128-byte zero padding provided by the backend. It writes descriptors,
without copying the coded payload or touching decoded frames.

Run `make check SANITIZER=address,undefined` on Linux or
`make check CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine` for Windows
ABI and existing H.264 mock regressions. The HEVC test checks 8/10-bit
fields, signed QP/POC, full reference capacity, short/long-term sets,
scaling/DC matrices, malformed NALs, padding and output bounds. Windows
compares complete buffers against native SDK structures.

This is the HEVC submission-buffer layer. rh265 parser snapshots and
hardware dispatch, D3D11 decode/profile setup, GPU slot scheduling,
NV12/P010 export, main-player integration and real GPU reference checks
remain. It does not register or advertise a working HEVC hardware decoder.
Frame ownership should follow the existing H.264 fences/release protocol;
compatible paths must stay GPU-resident through presentation/encoding.

Format references:

- [Microsoft HEVC DXVA specification](https://www.microsoft.com/en-ie/download/details.aspx?id=39947)
- [DXVA_PicParams_HEVC](https://learn.microsoft.com/en-us/windows/win32/medfound/dxva-picparams-hevc)
