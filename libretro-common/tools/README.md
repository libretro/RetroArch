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
| `chd_map_test.c` | Decodes the hunk map of real images and checks it against the CRC-16 the file carries, then feeds corrupted maps through the same path. |
| `chd_cd_test.py` | Reconstructs CD hunks — sector and subchannel framing, ECC rebuild — and compares them byte for byte against another reader's decode. |
| `rchd_open_test.c` | Opens images through `rchd` and reports the geometry, map and metadata it finds. |

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
| `huff_test.c` | Decodes every `huff` hunk of an image and compares against the original uncompressed source. |
