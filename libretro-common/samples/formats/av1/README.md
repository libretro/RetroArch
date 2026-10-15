# AV1 configuration, sequence headers and OBU views

`rmp4` recognises `av01` sample entries and borrows the `av1C` payload.
`rwebm` recognises `V_AV1` and borrows its CodecPrivate record. Existing
packet pointers, timestamps, sync flags and partial-read retry contracts
are preserved. Codec enum values are appended so existing values stay stable.
These are demuxer capabilities; builtin AV1 decoding/playback is not added.

`formats/rav1.h` provides allocation-free borrowed views of low-overhead
OBUs and the configuration record. It checks header bits, eight-byte bounded
LEB128 sizes, 32-bit payload-size limits, truncation, profile/depth/chroma
combinations, delay fields and sequence-header ordering in configOBUs.
Reserved OBU types are exposed for a decoder to skip. Packet mode permits a
final unsized OBU to consume its bounded sample; configuration mode requires
size fields. Failures zero the output and leave the packet cursor unchanged.
Retain the input owner while any view or packet is in use.

`formats/rav1_sequence.h` parses sequence payloads and checks agreement with
av1C. It covers normal/reduced still headers, timing and decoder models,
32 operating points, dimensions, frame-ID lengths, feature flags, 8/10/12-bit
colour layouts and film-grain presence. Caller-owned output avoids allocation
and large temporary stack frames; failures clear that output.

`formats/rav1_tiles.h` validates a tile group before exposing allocation-free
borrowed tile spans. It checks explicit tile ranges, zero alignment bits,
1-4 byte little-endian sizes, nonempty spans and the FRAME_OBU restriction.
Its iterator preserves the cursor on failure and reports row/column indices.
The caller supplies the parsed frame layout and the tile-group start; the
helper does not locate or parse the preceding uncompressed frame header.
`formats/rav1_tile_info.h` derives that layout from the frame tile-info bits.
It supports uniform and nonuniform partitions, 64/128-pixel superblocks,
context-update tile validation and coded dimensions with partial superblocks.
Caller-owned output stores the MI boundaries. Failures clear the output and
preserve the bit cursor, allowing a caller to retry with more input. Dimensions
are the coded frame size before super-resolution upscaling.
The syntax bounds are 64 columns by 64 rows. Sequence-level and device limits
belong to their respective capability checks.

`formats/rav1_frame.h` parses frame prefixes through reference-order hints
and the explicit/no-reference frame-size/render-size branch. It handles
frame types, hidden/showable flags, show-existing selectors, timing, frame IDs,
refresh flags, layer-filtered operating-point removal times and superres
dimensions. Failed parses clear output and leave the input bit cursor unchanged.
It requires a validated sequence. Show-existing type stays unresolved until
the caller validates and loads that reference.

This is syntax parsing, not reference management: slot validity/showability,
frame-ID matching/windows and reference refresh must be checked and committed
by the eventual decoder after a complete successful frame. Reference/motion header fields, frame_size_with_refs(), film grain, entropy
decoding, reconstruction and
Annex B remain unimplemented. Hardware session/reference/film-grain handling and
GPU imports remain the next AV1 layers. Compressed packet views avoid a
demux/framing copy; a hardware backend may still need a bitstream upload.

```sh
make check SANITIZER=address,undefined
python3 fixtures.py
python3 tile_fixtures.py
python3 frame_fixtures.py
python3 picture_fixtures.py
python3 reference_fixtures.py
python3 grain_fixtures.py
python3 state_fixtures.py
make check CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine
```

The unit cases cover malformed/truncated headers and lengths, explicit and
unsized OBUs, borrowed payload identity, configuration field combinations
and sequence-header ordering. Synthetic MP4/Matroska tests check borrowed
configuration/packet pointers, colour metadata, timestamps, key flags,
missing MP4 configuration, and repeated partial-read retries.
Sequence tests cover 24 normal/reduced profile/depth/chroma combinations,
32 operating points with optional decoder models, truncation at every byte,
malformed timing/UVLC/levels/frame IDs/chroma/trailing bits, duplicate operating
points, single-bit mutations and all eight configuration agreement fields.
`fixtures.py` generates 22 twelve-packet AV1 MP4/WebM files across 8/10/12-bit
4:2:0, 4:2:2, 4:4:4 and monochrome layouts. It compares sequence dimensions,
colour metadata, configuration bytes, packet hashes, timestamps and sync flags
with ffprobe. FFmpeg is a test tool, not a runtime dependency.

Tile tests cover partial groups, implicit and explicit single tiles, all
four size widths, truncation, invalid layout/range/padding/lengths, whole-group
preflight, failure cursor retention and a 4096-tile syntax grid.
`tile_fixtures.py` generates fourteen 8/10-bit streams with uniform and
nonuniform partitions and partial superblocks. It locates tile-info and
tile-group starts using FFmpeg frame-header traces, compares the derived
layouts, MI boundaries, context-update IDs and ending bit cursors, and checks
192 borrowed tile offsets, sizes and hashes. Only the positions and preceding
frame dimensions/superblock flag still come from the test oracle.
Tile-info unit cases cover ns() codes, unaligned input, irregular geometry,
context IDs, mandatory width/area splits, 64x64 grids, truncation and cursor
rollback. Full frame-header parsing and reference state remain separate work.

Frame-prefix tests cover all four coded frame types, hidden/shown output,
reduced headers, show-existing selectors, timing across 32 operating points,
frame IDs, truncation rollback and all superres denominators.
`frame_fixtures.py` compares eight 8/10-bit real streams with FFmpeg traces:
130 prefixes including 32 hidden frames and 20 show-existing replays, plus
explicit geometry and ending bit positions. It includes error-resilient,
reduced-still and all-intra cases. FFmpeg is only a test oracle.

Format references:

- [AOM AV1 ISOBMFF binding, sections 2.2-2.4](https://aomediacodec.github.io/av1-isobmff/)
- [Matroska V_AV1 mapping](https://www.matroska.org/technical/codec_specs.html#v_av1)

- [AOM AV1 specification, sequence header syntax and semantics](https://aomediacodec.github.io/av1-spec/)

`formats/rav1_picture.h` parses base quantization through transform mode:
quantizer deltas/matrices, segmentation, delta-Q/LF, lossless flags, loop
filters, CDEF and restoration. Inherited segment/filter metadata is supplied
by the caller; it is never modified. Output cannot alias that metadata.
For distinct storage, failures clear output and preserve the input bit cursor.
Only metadata is copied; compressed tile and pixel storage stays borrowed.
Reference-slot validation and commit remain decoder responsibilities.

Picture tests cover signed deltas, segmentation clipping/inheritance/reset,
filter updates, all CDEF entries, restoration mappings/sizes, intrabc gates,
coded versus all-lossless flags, alias rejection and byte truncation rollback.
`picture_fixtures.py` checks 26 picture-tool blocks in fourteen 8/10-bit
streams: lossy, adaptive quantization, lossless, monochrome, 4:4:4,
restoration and reduced still pictures. The native sequence/prefix/size/tile
parsers locate these tools without trace-supplied offsets; all decoded fields
and start/end cursors are compared with FFmpeg. Inter-frame inheritance is
covered by synthetic cases, rather than claimed from this all-intra corpus.
Complete reference-state validation/commit and hardware submission remain
separate work.

`formats/rav1_reference.h` continues a coded frame prefix through
`disable_frame_end_update_cdf`: explicit or short-signaled reference selection,
frame-ID deltas, tentative ID/order-hint invalidation, reference-derived sizes,
superres, intrabc and pre-tile motion flags. Caller-owned slot metadata remains
immutable. The returned valid mask is tentative; decoder state and GPU surface
ownership must only be committed after successful decode. Show-existing
validation, frame-ID progression, scaling constraints and sequence transitions
remain caller responsibilities.

Reference tests exercise order-hint wrap/ties/fallback, every found-ref position,
all superres denominators and interpolation filters, frame-ID wrap/window
boundaries, invalid slots, inferred flags and truncation rollback.
`reference_fixtures.py` compares eight reordered, error-resilient, adaptive-
quantization and still-picture streams at 8/10 bits. Native parsing locates
135 coded setups and picture blocks without supplied offsets, including 85
primary-reference inheritances. Reference indices, sizes, motion flags,
segment/filter metadata and bit positions match FFmpeg traces. Short signaling
and reference-derived resizing are covered synthetically in this corpus.
The fixture harness simulates successful decode to advance metadata; it does
not implement a decoder or change GPU ownership.

`formats/rav1_motion.h` parses compound-reference selection, skip-mode pairs,
warped-motion enablement, reduced TX and global model parameters through the
start of film grain. Global predictors come from caller-owned primary metadata;
NONE uses identity. Parameters remain in AV1's 16-bit fractional precision.
The parser does not validate per-block warp applicability or implement pixels.
Failures preserve the cursor and clear output; predictor/output aliases are
rejected without changing metadata. No compressed or pixel buffers are copied.

Motion tests exhaust subexponential symbols for eight alphabet sizes through
65536 and three predictor positions. Synthetic headers cover all four models,
coefficient extrema, inherited predictors, negative precision conversion,
order-hint wrap/ties, skip eligibility, inferred flags and truncation.
The reference fixture chain now also compares motion flags, all identity model
coefficients and end cursors for the same 135 coded frames. Nonidentity models
are covered synthetically; the generated real corpus uses identity models.

`formats/rav1_grain.h` parses film-grain scaling points, signed autoregressive
coefficients, chroma parameters, seed and flags. Inherited parameters must come
from a selected valid reference and retain the new seed. Point counts/order and
4:2:0 chroma pairing are validated. Metadata remains immutable; output aliases
are rejected, and other errors clear output without consuming input. This
extracts hardware submission metadata; it does not synthesize grain pixels.

Grain tests cover all coefficient lags, maximum point counts, monochrome/luma
chroma inference, signed extrema, inheritance, alias/selection errors, point
ordering and byte truncation. `grain_fixtures.py` checks eight libaom grain
vectors, monochrome and four-tile frames at 8/10 bits: 140 coded headers,
10 grain inheritances and 182 borrowed tiles. Metadata and header endpoints
match FFmpeg traces; tile sizes/hashes match independent packet slicing.
The native prefix/setup/tile/picture/motion/grain chain locates tile data
without oracle offsets. The fixture harness still simulates successful decode;
show-existing validation, decoder reference commit, hardware submission and
actual grain output remain separate work.

`formats/rav1_reference_state.h` keeps slot identity/geometry and display
eligibility separate from GPU surfaces and picture/filter/grain metadata.
Prepare a ticket after parsing the prefix, stage validated setup metadata,
then commit only after the whole frame and decode/submission are accepted.
Failed or abandoned work leaves committed state unchanged. Epoch/owner checks
reject duplicate, stale and cross-state tickets. Reset on seek/new sequence;
the helper assumes one validated sequence and an externally serialized decoder.
GPU fences and output leases remain independent; no GPU wait is required.

Show-existing validates the selected slot, showability and display frame ID.
A key replay refreshes every slot and consumes all aliases of that picture.
Coded frames validate previous/current ID progression and reference scaling
bounds. External metadata must follow the same refresh mask and picture ID.
This does not manage entropy/CDF state, temporal units or spatial-layer rules.

State tests cover rollback, ID wrap, scaling boundaries, key aliases, repeated
inter replays, reset, stale/duplicate/cross-state tickets and epoch exhaustion.
`state_fixtures.py` compares 180 real 8/10-bit transactions, including 35
show-existing replays, against an independent slot model. Four mutated replay
selectors are rejected. The native full header/tile chain stages and commits
metadata assuming successful decode; actual hardware submission remains work.

`formats/rav1_hw.h` supplies a caller-owned hardware session for single FRAME
OBUs and show-existing FRAME_HEADER OBUs. It parses complete coded headers and
preflights borrowed tiles before one backend call. BUSY retains immutable input
for retry; cancellation/backend failure cannot commit references. Acceptance
releases CPU input ownership and commits metadata. Slot selection uses nine or
more DPB slots; previous order hints, geometry and types accompany references.
Show-existing uses output only. Drain or discard display before the next frame.

The backend must snapshot accepted input, preserve exported ready/done leases
across DPB reuse, and implement requested grain or decline before acceptance.
The session allocates nothing, waits for nothing and copies no pixel/packet
buffers. It accepts profile 0, 8/10-bit 4:2:0, with a validated fixed sequence;
separate tile-group OBUs and additional temporal/spatial layers are unsupported.
Sequence changes require retiring/reopening the session and backend.

Hardware-session tests cover BUSY/retry identity, cancel, decoder failure,
output backpressure, lease abandonment, P010 validation, DPB slot rotation,
key replay without decode, order-hint snapshots, grain propagation and malformed
input. The eight state fixtures also run through a mock backend: 180 frames,
145 accepted decodes and 35 replays, with submission/output BUSY injected.
These are protocol tests, not evidence of hardware pixels or Vulkan decoding.
`rav1_set_hw_backend` publishes an immutable platform factory. Open returns
operations and a retained factory copy, so existing decoders keep their close
callback after withdrawal. Failed/incomplete opens close allocated decoders and
clear outputs. Registry tests cover withdrawal, unsupported slot counts and
partial-open cleanup, with and without thread support.

RetroArch's HAVE_RAV1 option links the registry and enables a Vulkan Main
8/10-bit backend. See samples/gfx/vulkan_video_caps for native pixel evidence
and device limits. This does not supply a software AV1 pixel decoder.

`formats/rav1_hw_stream.h` owns packet dispatch and backend retirement around
that session. Initialize fresh persistent storage with a validated configuration
sequence, or NULL for an in-band header. `begin` borrows a complete bounded
packet; `step` returns END, FRAME, PROGRESS, BUSY or DISCARDED.
Keep the packet resident and immutable until END, error or close. FRAME transfers
a GPU lease; PROGRESS permits the caller to yield between hidden frames or
metadata, and BUSY retains the cursor without waiting. Discard omits exports
while retaining decoder references. Sequence changes require close/reinit.
Separate FRAME_HEADER/TILE_GROUP coding and extra layers remain unsupported.
Tests cover multi-OBU packets, hidden/replay ordering, decode/export BUSY,
discard, cancellation, repeated/changed sequences and lease release after close.
MP4 hardware candidates open through `rmp4_video_stream_open_hw_avail` and
require `set_hw` before decoding. AV1 uses no CPU canvas. Ordinary software-only
opens continue declining AV1. The adapter requires an av1C sequence header and
fixed track-sized SDR output; unsupported backends/configurations decline.
Partial input and decoder/export capacity preserve the pending packet floor.
`skip` retains the normal renderable-frame contract; `discard` advances without
AV1 export. Reader tests cover starvation, BUSY, timing, rewind, cancellation,
absent backends and leases released after close. The builtin MP4 core now
admits hardware-only AV1 when its hardware sink is installed, without a CPU
framebuffer. Backend allocation waits for hardware startup; unavailable sinks
or backends terminate playback cleanly. The File Browser GPU preview now uses
`image_transfer_anim_stream_new_hw_avail` through `gfx_anim_preview_open_hw`.
Hardware-only candidates require an available backend; ordinary image and
companion CPU preview opens still decline AV1. Interactive UI/A/V acceptance
remains pending. Successful hidden-frame/metadata progress runs within a 4 ms budget;
true BUSY returns immediately. Untaken displayed leases survive later progress
and EOF so the core can present its final frame.

WebM hardware candidates open with `rwebm_video_stream_open_hw_avail` and
require `set_hw` before decoding. CodecPrivate must contain a valid av1C
header; its sequence may be in CodecPrivate or the first video packet.
Sequence/configuration, fixed track geometry and SDR color must agree.
Packets remain borrowed and pinned across input, decode and export retries;
hidden-frame progress has a 4 ms budget. `next` returns a non-pixel token
and `hw_frame` transfers its GPU lease. WebM `skip` discards exports while
retaining decoder references. Rewind/cancel retire decoder state without
invalidating leases already transferred. Ordinary software opens decline AV1.
The explicit GPU preview open also admits these WebM streams; builtin WebM
movie-core routing remains separate work.
