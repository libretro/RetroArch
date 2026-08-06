# libretro-common tools

Programs that exercise the code in this tree against real data, kept
here rather than beside it so a build of libretro-common never picks
them up.

The layout mirrors the tree they test: `tools/chd` for
`formats/chd`, `tools/cdfs` for `formats/cdfs`, `tools/flac` for
`formats/flac`, `tools/mpeg1` for `formats/mpeg1`, `tools/encodings`
for `encodings`, `tools/formats` for the rest of `formats`. `tools/cheevos` is the exception and tests
`deps/rcheevos` as this tree feeds it.

None of these are part of any build. Each is a standalone program or
script, compiled or run by hand against the sources it names.

## chd

| | |
|---|---|
| `chd_probe.py` | Regenerates the reference images `formats/chd/FORMAT.md` is derived from and re-checks every claim marked verified in it. Requires `chdman` on PATH. |
| `rchd_crc16_test.c` | Checks the table-driven CRC-16 against the bitwise definition. |
| `chd_map_test.c` | Decodes the hunk map of real images and checks it against the CRC-16 the file carries, then feeds corrupted maps through the same path. |
| `chd_cd_test.py` | Reconstructs CD hunks — sector and subchannel framing, ECC rebuild — and compares them byte for byte against another reader's decode. |
| `rchd_open_test.c` | Opens images through `rchd` and reports the geometry, map and metadata it finds. |
| `rchd_compare_test.c` | Reads every hunk of an image through `rchd` and compares against another reader — including the partly-padded final hunk, which exercises a path no other one does. |
| `rchd_supply_test.c` | Drives an open and a read entirely through the offset-identified supply calls, borrowing rather than copying. |
| `rchd_sector_test.c` | Checks a sector-addressed read against a byte read of the same frames. |
| `rchd_read_test.c` | Reads every hunk of an image through `rchd` and compares against the original uncompressed source. |
| `avhuff_decode.py` | Reference decode of an A/V hunk's video, for checking against fields another implementation extracts. |
| `find_av_chd.py` | Scans a tree of images and reports each one's codec, marking any that use an audio/video codec. |
| `chd_slice.py` | Extracts the header, map, metadata and a few hunks of an image into a small self-contained file, so a format question can be settled without moving the whole thing — a laserdisc image runs to tens of gigabytes. |
| `make_subchannel_cd.py` | Builds a CD image that carries subchannel data, which no commercial image to hand does — every one reports `SUBTYPE:NONE`, leaving half of each frame untested. |

## cdfs

| | |
|---|---|
| `cdfs_backend_compare.c` | Digests a disc image through cdfs, for comparing CHD readers above chd_stream. |
| `cdfs_sector_compare.c` | Compares CHD readers over the sector-seek path RetroAchievements hashes through. |

## cheevos

| | |
|---|---|
| `rc_hash_backend_compare.c` | Compares the RetroAchievements hash of a disc image between CHD readers. |

## flac

| | |
|---|---|
| `make_mka.py` | Builds a Matroska file holding a FLAC track, from a header and raw frames. |
| `make_ogg.py` | The same for an Ogg FLAC stream (RFC 5334). |
| `flacprobe.c` | Wraps a candidate headerless FLAC stream in a STREAMINFO built from the specification and decodes it, which locates FLAC data by content rather than by guessing at a header. |
| `rflac_push_test.c` | Decodes a headerless stream at several input chunk sizes, exercising the path where a frame straddles two spans. |
| `rflac_header_test.c` | The same for a stream carrying its own header. |
| `rflac_mka_test.c` | Drives the Matroska arm and compares against a native-stream reference. |
| `audio_transfer_flac_test.c` | Drives `audio_transfer`'s FLAC arm end to end. |

The two builders exist because nothing in this tree produces a `.mka` or
an Ogg FLAC stream, so the container paths could otherwise only be built
and not run — and a file recorded elsewhere is not reproducible. The
Matroska one found a real defect on first use.

## mpeg1

| | |
|---|---|
| `gen_tables.py` | Generates `formats/mpeg1/rmpeg1_tables.h` from the Annex B text of ITU-T H.262, which carries the same variable length code tables as ISO/IEC 11172-2. Takes `pdftotext -layout` output. |
| `idct_accuracy.c` | Measures the IDCT against a double-precision reference in the style of IEEE 1180-1990 — peak error, mean square error, mean error and worst per-position mean error over several coefficient ranges. |
| `fuzz_demux.c` | Drives the demuxer through truncations, mid-stream entry points, byte corruption and pure random input, checking it never stalls, over-reads or emits an empty packet. |
| `diff_video.c` | Decodes every frame and compares geometry and per-plane pixels against another decoder, with a per-frame breakdown and a dump of the worst macroblock. |
| `bench.c` | Times the demuxer and the video decoder against another implementation on the same stream. |

`gen_tables.py` proves each table prefix-free, checks its Kraft sum and
asserts completeness before emitting anything. That is not decoration: it
caught a regex that could not match a single-character code, and an en-dash
minus sign in the specification text that dropped every negative
`motion_code`. Both produced tables that were still prefix-free and quietly
missing entries — which a decoder fails on in a way that looks like a
bitstream fault rather than a table fault.

Both DCT tables sum to 4095/4096 rather than 1. The twelve-zero prefix is
left unassigned so no code can emulate a start code prefix, so a DCT table
summing to exactly 1 would be wrong.

`diff_video.c` compares every frame rather than the first because a P
picture is built on its predecessor: a prediction fault accumulates down the
GOP, and a frame-zero check passes a decoder whose motion compensation is
subtly wrong. `idct_accuracy.c` is the authority on pixel values, since
agreeing with another decoder says only that the two agree.

`diff_video.c` and `bench.c` need a comparison decoder, which is not a
dependency of this tree; both were written against pl_mpeg (MIT), fetched
into a scratch directory and pointed at with `-I`. The other three are
self-contained.

Reference streams come from ffmpeg — `-target ntsc-vcd` and `-target
pal-vcd` for the two Video CD shapes, and `-c:v mpeg1video -bf 2` for a
stream carrying B pictures, which neither VCD profile produces.

## encodings

| | |
|---|---|
| `rzstd_encode_test.c` | Round-trips through rzstd's encoder and decodes with both rzstd and the reference implementation. |
| `rzstd_frame_test.c` | Decodes a Zstandard frame through `rzstd` and compares against a reference decode. |
| `rzstd_multiblock_test.c` | Round-trips periodic data across block boundaries, where frame-scoped state can drift. |
| `rzstd_bench.c` | Times rzstd against the reference implementation on synthetic inputs. |
| `rzstd_frame_bench.c` | Times rzstd against the reference implementation on frames taken from a real image. |
| `rzstd_fse_test.c` | Builds the FSE tables RFC 8878 predefines and checks the spread closes. |
| `huff_test.c` | Decodes every `huff` hunk of an image and compares against the original uncompressed source. |
| `adler32_test.c` | Checks `encoding_deflate.c`'s adler32 against a textbook reference over block boundaries, unaligned starts, the chunk bound and the worst case for its 32-bit overflow argument. Build it once per SIMD path; all must agree. |

## formats

| | |
|---|---|
| `rxml_treehash.c` | Hashes the tree rxml builds for each document given, so two builds can be diffed over a corpus. Rejected documents print REJECT, so a change in what is accepted shows up as well. |
