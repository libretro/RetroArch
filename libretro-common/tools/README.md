# libretro-common tools

Programs that exercise the code in this tree against real data, kept
here rather than beside it so a build of libretro-common never picks
them up.

The layout mirrors the tree they test: `tools/chd` for
`formats/chd`, `tools/flac` for `formats/flac`, `tools/encodings` for
`encodings`.

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

## encodings

| | |
|---|---|
' `rzstd_encode_test.c` ' Round-trips through rzstd's encoder and decodes with both rzstd and the reference implementation. '
' `rzstd_frame_test.c` ' Decodes a Zstandard frame through `rzstd` and compares against a reference decode. |
| `rzstd_fse_test.c` | Builds the FSE tables RFC 8878 predefines and checks the spread closes. |
| `huff_test.c` | Decodes every `huff` hunk of an image and compares against the original uncompressed source. |
