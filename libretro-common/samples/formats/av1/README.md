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

Other frame-header syntax, entropy decoding, frame reconstruction and
Annex B remain unimplemented. Hardware session/reference/film-grain handling and
GPU imports remain the next AV1 layers. Compressed packet views avoid a
demux/framing copy; a hardware backend may still need a bitstream upload.

```sh
make check SANITIZER=address,undefined
python3 fixtures.py
python3 tile_fixtures.py
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

Format references:

- [AOM AV1 ISOBMFF binding, sections 2.2-2.4](https://aomediacodec.github.io/av1-isobmff/)
- [Matroska V_AV1 mapping](https://www.matroska.org/technical/codec_specs.html#v_av1)

- [AOM AV1 specification, sequence header syntax and semantics](https://aomediacodec.github.io/av1-spec/)
