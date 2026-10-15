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
It reuses the H.264 texture-array, shared-output and fence retirement paths.
Explicit Main10 opens select the Main10 profile and P010 textures instead.
Compressed NAL spans are packed directly into the acquired driver buffer;
picture, optional scaling matrix and short-slice buffers are submitted once.
Busy BeginFrame leaves ownership uncommitted. Driver failures end an opened
frame and stop the session. Close retains unread export handles until their
lease is consumed or abandoned. Close every session before adapter deinit.

The mock tests cover profile/configuration, buffer sizes/padding, optional
matrices, busy retry, driver failures and deferred close. The device oracle
compares imported Main and Main10 pictures with software through shared handles and
fences; it skips if device setup is unavailable. Oracle readback is test-only.
The D3D11 adapter registers Main8 and, with P010 imports, Main10 with rh265. MP4 hardware opt-in
opens it before the first picture and keeps compressed packet storage across
decode/output backpressure. DPB pressure may export an older picture while
retaining the unconsumed packet; the presentation clock advances only for
the exported picture. Sessions request the SPS DPB capacity plus one target,
subject to the device's minimum texture-array capacity. Rewind drops untaken
leases and reopens the decoder. Decoder-open refusal retains software decode.
`rh265_hw_stream_test` covers decoder and MP4 lease transfer, busy retries,
drain, rewind and untouched CPU pixel buffers with portable mocks.

GPU frames carry an explicit image pixel format; zero retains legacy NV12.
D3D11 and Vulkan NV12 producers label their frames explicitly. The surface
player forwards P010 metadata instead of labeling it NV12; unknown formats
are refused. D3D11/D3D12 import NV12 or P010 through shared GPU handles and
convert their planes into RGBA8 without CPU pixel readback. P010 sampling
preserves limited/full-range normalization and 10-bit neutral chroma.
Renderer format support is frozen before publishing the decoder factory;
NV12-only consumers retain Main10 software fallback. Vulkan imports remain
NV12-only. HDR playback remains gated; this is SDR presentation.
`../d3d_p010_import/run.py` extracts the production shaders, D3D11 import and
constant/dispatch functions. Windows checks shared NV12/P010 handles, cache
format/dimension refusal and GPU colors in BT.601/709 limited/full range.
The D3D12 shader is also executed with its constant layout on the D3D11
compute oracle. `test12.c` runs the production D3D12 import functions on a
real queue, using a D3D11 producer on the same adapter. It checks shared
NV12/P010 views, BT.601/709 limited/full-range colors (including low 10-bit
code bits), blocked producer fences, consumer release, cache reuse and
refusal without submission under list/descriptor/fence pressure. The fixture
preallocates the output; initial output allocation is not exercised. No
production ownership callback is invoked by the importer. Actual HEVC
decoder execution and movie-core GPU presentation remain separate gates. Linux and
unsupported Windows devices skip GPU execution. Readback is test-only.

Remaining: main movie-core GPU presentation, Vulkan HEVC and real
inter-picture GPU reference coverage remain. Export performs one GPU
crop/copy into the shared texture pool;
it never reads pixels back to the CPU. Removing that copy requires array-layer
and crop transport plus compatible sharing and retained DPB ownership.

Format references:

- [Microsoft HEVC DXVA specification](https://www.microsoft.com/en-ie/download/details.aspx?id=39947)
- [DXVA_PicParams_HEVC](https://learn.microsoft.com/en-us/windows/win32/medfound/dxva-picparams-hevc)

The device oracle uses 256x256 HEVC I pictures and 12-picture Main/Main10
streams with I/P, B-pyramid reordering, WPP and conformance-window cropping.
Both D3D11 and D3D12 readers compare every decoded sample to rh265.
The portable hevc_device_fixture_test also compares rh265 to per-frame
FFmpeg hashes. Regenerate these fixtures with python3 gen_hevc_data.py
(FFmpeg/libx265 are generation tools, not player dependencies). Tiny 64x64
parser fixtures remain in the mock tests; real drivers may expose no
HEVC decoder configuration at that size.

hevc_color_stream_test covers SPS VUI fallback for missing/unspecified MP4
color descriptions, explicit container/range precedence, full-range NV12
and P010 metadata, and software PQ/HLG scRGB fallback. It also checks SDR
scalar/SIMD conversions against color-matrix equations, including tails
and output padding. gen_hevc_color_data.py regenerates its fixtures.

Long-term hardware metadata now includes SPS table indices, slice-signaled
LSBs, usage flags and cumulative MSB cycles. Owner tests cover retained
references, wrap, missing/ambiguous references and rejected transactions;
DXVA tests check long-term flags and current sets, including later slices.
Software reconstruction supports long-term reference retention, current lists
and reference-class-aware temporal/spatial prediction. The optional
native conformance oracle compares actual long-term hardware output to external FFmpeg NV12 rather than rh265 software reconstruction.
Syntax reference: https://github.com/FFmpeg/FFmpeg/blob/n7.1/libavcodec/hevc/hevcdec.c#L268

To exercise an external Main8 long-term stream (up to 32 displayed pictures),
run `python3 prepare_hevc_conformance.py INPUT.bit OUTPUT_DIR`. It prepares
an hvc1 MP4 with parameter sets in extradata and no edit list, plus an NV12
reference and SHA-256 manifest. Set `HEVC_CONFORMANCE_MP4` and
`HEVC_CONFORMANCE_NV12` to the generated paths, then run the native Windows
`d3d11_video_decode_test.exe`. Both variables are required when either is
set. The oracle requires active long-term entries, checks frame count,
geometry, display order and every sample through D3D11 and D3D12 imports.
Unsupported devices retain the existing skip behavior. Input media is not
bundled or fetched by the script; default tests remain self-contained.

Validated input: the first 24 coded pictures of
`LTRPSPS_A_Qualcomm_1.bit` from the
[FFmpeg conformance suite](https://fate-suite.ffmpeg.org/hevc-conformance/),
416x240 Main8 with an eight-entry SPS long-term table and seven active
long-term entries across the selected access units. All 24 output pictures
match FFmpeg through both native readers. This does not validate Main10
long-term streams or all hardware cycle/wrap cases. Software reconstruction
also matches all 500 pictures of that stream against FFmpeg on one thread,
rotating contexts and four concurrent picture threads.
The manifest identifies the exact source and prepared output bytes.

Matching SPS/PPS repeats in access units are validated against the active
parsed parameter sets without allocation or pixel copies. Changed sets
remain refused without replacing active metadata. Generated `repeat8` and
`repeat10` fixtures cover repeated headers through portable and device
oracles. For an external short-term regression, set
`HEVC_CONFORMANCE_REQUIRE_LT=0`; long-term entries remain required by default.
A four-picture 3840x2160 Main8 preview regression with repeated headers also
matches FFmpeg through the native D3D11 and D3D12 readers.

## Explicit VP9 device decoding

`d3d11_video_decode_vp9_open` offers an explicit profile-0/NV12 or
profile-2/P010 backend for `rvp9_hw_session_t`. The initialized renderer must
advertise the matching import format. The device is queried for the actual
profile, coded size and raw-bitstream configuration. The backend is registered when HAVE_RVP9 is enabled. The WebM animation
stream can select it through image_transfer and the shared video_player
used by thumbnail animation and the primary media_core WebM path.
The movie-core host and optional native GPU oracle live in
`samples/cores/webm_player`; Vulkan VP9 submission remains pending.

The initial path accepts even, fixed-size 4:2:0 frames and BT.601/BT.709 output
matrices (VP9 color spaces 0-3); unsupported color spaces, depth and geometry
require fallback/reopen. DPB storage is aligned to 128 pixels independently of
the coded size. Display export reuses the asynchronous shared-texture/fence
path: no CPU pixel transfer. D3D11 readers use a DPB-to-output copy;
D3D12 opts into shared decoder-array import where allocation permits it.
Dropped display frames can bypass export through the common session API.

`dxva_vp9_test` checks the packed 208-byte parameters, including references,
filter deltas and segmentation, against the Windows SDK on Windows. The D3D11
mock tests both profiles, BUSY without submission, padded packet/slice buffers,
output backpressure, delayed close and buffer/submission failures.

Generate external real-device fixtures with:

```
python3 gen_vp9_device.py /tmp/vp9-device
```

Set `VP9_TEST_IVF` and `VP9_TEST_RAW` to a matching generated pair before running
`d3d11_video_decode_test.exe` on Windows. `VP9_REQUIRE_HW=1` makes an unavailable
VP9 decoder configuration a failure. Each pair is checked through D3D11 and,
when available, D3D12 shared-handle readers. Readbacks/waits are test-only.
The generator requires FFmpeg with libvpx-vp9; production decoding does not.
It packs FFmpeg's decoded planar samples into NV12/P010 directly, avoiding
swscale range conversion in the oracle.

Native RTX 5090 validation: four 12-frame profile/depth/color/range cases,
each through D3D11 and D3D12, match every independent reference sample
(96 decoded-frame comparisons). Eight show-existing replays also match without
another decoder submission. H.264 and HEVC regression device tests pass.
This direct-session test does not establish movie-core playback, HDR/tone mapping,
changing resolution or wider devices. Mock WebM stream tests cover hidden and
superframe retry/ownership sequencing separately.
Vulkan VP9 submission remains separate work.


Set VP9_TEST_WEBM to a generated .webm file and VP9_TEST_RAW to its matching
oracle to test registered-backend selection through rwebm_video_stream. The
native oracle compares all frames through D3D11/D3D12 readers, then rewinds and
reads an exported frame after closing its stream. Four 12-frame cases pass on
the RTX 5090 (96 WebM frame comparisons plus eight rewind/close checks).
These tests exercise real hardware decoding and shared GPU leases, not the
complete File Browser UI, shader chain, audio synchronization or primary core.

The primary movie-core device oracle uses these same external fixtures:

```
cd samples/cores/webm_player
make webm_vp9_device_test.exe CC=x86_64-w64-mingw32-gcc OS=Windows_NT EXE=.exe
```

Run natively on Windows with `VP9_TEST_WEBM` and `VP9_TEST_RAW` set to a
matching generated pair. It drives the real media_core and compares every
NV12/P010 sample through the D3D11 and available D3D12 readers, while checking
that the CPU canvas stays untouched. Readbacks and waits are test-only.
`make check` also runs a portable mock-backend core test for BUSY retries,
partial input, seeks, reset/restore, suspend/restart and software fallback.
Seek catch-up is asynchronous and currently replays from the first keyframe;
skipped frames decode without exporting shared textures. The native oracle
checks a post-seek picture against its oracle samples through both readers.
Long-file seek performance and actual renderer/shader/A/V acceptance remain.


### Direct D3D12 decoder-array import

D3D12 imports the NV12/P010 decoder array with a slice and even crop origin.
Four spare physical slices allow logical decoder slots to be renamed while
readers hold older pictures. Ready/done fences and abandonment retain each
lease through decode, rewind and close. Shared-allocation failure falls back
to private DPB storage and copied output. D3D11 rendering keeps that copied
path: D3D11 does not permit shader views of decoder arrays. The D3D12 YUV-to-RGB
conversion still writes an output texture.

Set `D3D11_TEST_SHARED_DPB=1` for the native device oracle, or the primary
movie-core oracle with its external fixtures, to require shared-array output
without independent output textures. RTX 5090 tests cover H.264, HEVC Main/Main10
and four VP9 color/depth cases. The core oracle holds a duplicate first-picture
lease through subsequent decoding, seek and unload, then compares it again.
The four cases through both readers pass 112 sequential, seek and held-picture
comparisons. This does not establish wider-device or complete UI/A/V acceptance.

The mock checks no-copy export, slot/reference remapping, BUSY retry, delayed
close and allocation fallback. The production D3D12 import shader oracle also
covers NV12/P010 arrays, cropped nonzero slices, slice-cache replacement and
invalid crop/slice rejection; GPU readbacks and blocking waits are test-only.
