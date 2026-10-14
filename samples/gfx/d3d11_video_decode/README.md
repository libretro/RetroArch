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
`rh265_hw_session_submit` dispatches a prepared picture to codec-neutral
backend callbacks. A busy return retains packet/parameter/AU/span identity;
accepted decode commits once and retires those borrowed pointers. Each call
attempts at most one decode. Unaccepted work can be cancelled without
changing the DPB. Busy output keeps the slot held; successful output transfers
the frame lease to the caller, who must import or abandon it. Logical DPB
pressure permits one ordered output bump before submission is retried.
Backend errors stop the session; invalid exported frames are abandoned.
Device setup/teardown and GPU fence/lease retirement remain backend-owned.
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
It now exercises busy decode/output callbacks, direct packing, one-time
commit and caller abandonment through the session dispatcher.

`d3d11_video_decode_hevc_open` supplies Main 8-bit callbacks on the initialized
adapter, selecting the HEVC Main profile and raw-bitstream configuration 1.
It reuses the H.264 NV12 array, shared output textures and fence retirement.
Compressed NAL spans are packed directly into the acquired driver buffer;
picture, optional scaling matrix and short-slice buffers are submitted once.
Busy BeginFrame leaves ownership uncommitted. Driver failures end an opened
frame and stop the session. Close retains unread export handles until their
lease is consumed or abandoned. Close every session before adapter deinit.

The mock tests cover profile/configuration, buffer sizes/padding, optional
matrices, busy retry, driver failures and deferred close. The device oracle
compares an imported Main picture with software through shared handles and
fences; it skips if device setup is unavailable. Oracle readback is test-only.
Main10/P010, player registration, Vulkan HEVC and real inter-picture GPU
reference coverage remain. The main decoder/player does not select this
backend yet. Export performs one GPU crop/copy into the shared texture pool;
it never reads pixels back to the CPU. Removing that copy requires array-layer
and crop transport plus compatible sharing and retained DPB ownership.

Format references:

- [Microsoft HEVC DXVA specification](https://www.microsoft.com/en-ie/download/details.aspx?id=39947)
- [DXVA_PicParams_HEVC](https://learn.microsoft.com/en-us/windows/win32/medfound/dxva-picparams-hevc)
