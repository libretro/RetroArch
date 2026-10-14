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
`dxva_hevc_qmatrix_raster` converts effective raster coefficients directly
to the submission buffer. Pack 32x32 matrix IDs 0 and 3 into the two
`list32` entries; retain their DC values separately. The parser adapter
gets effective SPS/PPS lists from `rh265_video_get_params` on the decoder
owner thread. `dxva_hevc_rh265.h` maps those snapshots to validated picture
fields while preserving per-picture POC, slots, RPS and IRAP flags. Its
matrix helper converts raster coefficients to the submission buffer.
Snapshots are owned metadata; neither helper allocates or accesses pixels.
`rh265_video_get_slice` reads headers directly from borrowed escaped NALs,
without unescaping/copying the payload or advancing the DPB. It supplies
resolved short-term RPS deltas and the predictor count/syntax bit length
needed by DXVA. Long-term reference syntax remains unsupported. Headers
are bounded to 64 KiB; only header syntax is validated.
`dxva_hevc_slice_from_rh265` maps used deltas to the caller's live POC/slot
table. Apply it to every slice in order so mixed I/P/B slices cannot mark
an inter picture as intra. Later slices must agree on picture flags and
current RPS. Full POC derivation, DPB marking and slot leases belong to
the decoder owner; missing/ambiguous references leave descriptors unchanged.
`rh265_video_get_access_unit` validates ordered slices for one picture and
returns borrowed VCL spans. Select Annex-B or the container's length size
explicitly; in-band SPS/PPS changes are refused. Retain the immutable packet
across busy retries and use the summary's `intra` flag for mixed slices.
`dxva_hevc_pack_slices` writes these spans directly to the backend's acquired
compressed-input buffer, adding start codes, zero padding and SDK slice
records. It preflights all slices and capacities before changing buffers.
This requires a compressed upload but avoids a separate host staging copy.
`rh265_hw_prepare` derives full POC and a retained-reference/target-slot plan
without mutating owner state. Busy submission retries keep both packet and
plan alive. `rh265_hw_commit` advances ownership only after backend acceptance
and rejects double/stale commits. Display-pending slots remain held until
export succeeds; output is ordered by sequence epoch and POC. IDR/BLA/initial
CRA anchors reset reference history and gate RASL output. Shape/POC-bit-depth
changes require an owner reset. Long-term references remain unsupported.
`dxva_hevc_picture_from_state` maps the plan to the existing DXVA helpers.
These owner masks do not indicate GPU completion: the backend must still
refuse writes to slots with unfinished work or exported frame leases. No
fence waits, frame leases, allocation or pixel copies occur in these helpers.
The slice helper borrows complete VCL NALs with
Annex-B prefixes, preserves emulation-prevention bytes, and accounts for
128-byte zero padding provided by the backend. It writes descriptors,
without copying the coded payload or touching decoded frames.

Run `make check SANITIZER=address,undefined` on Linux or
`make check CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine` for Windows
ABI and existing H.264 mock regressions. The HEVC test checks 8/10-bit
fields, signed QP/POC, full reference capacity, short/long-term sets,
scaling/DC matrices, malformed NALs, padding and output bounds. Windows
compares complete buffers against native SDK structures.
The access-unit pipeline test borrows Main/Main10 fixture NALs, constructs
picture/slice submissions, and compares software output from the original
length-prefixed sample with the packed Annex-B buffer. This proves payload
preservation and parser integration; it does not execute a GPU decoder.

This is the HEVC parameter/access-unit/submission-buffer layer. The decoder
owner still needs callback/retry integration, hardware dispatch,
D3D11 decode/profile setup, GPU slot scheduling,
NV12/P010 export, main-player integration and real GPU reference checks
remain. It does not register or advertise a working HEVC hardware decoder.
Frame ownership should follow the existing H.264 fences/release protocol;
compatible paths must stay GPU-resident through presentation/encoding.

Format references:

- [Microsoft HEVC DXVA specification](https://www.microsoft.com/en-ie/download/details.aspx?id=39947)
- [DXVA_PicParams_HEVC](https://learn.microsoft.com/en-us/windows/win32/medfound/dxva-picparams-hevc)
