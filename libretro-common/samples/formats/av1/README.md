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

Frame/tile payload syntax, frame reconstruction and Annex B remain unimplemented. Hardware session/reference/film-grain handling and
GPU imports remain the next AV1 layers. Compressed packet views avoid a
demux/framing copy; a hardware backend may still need a bitstream upload.

```sh
make check SANITIZER=address,undefined
python3 fixtures.py
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

Format references:

- [AOM AV1 ISOBMFF binding, sections 2.2-2.4](https://aomediacodec.github.io/av1-isobmff/)
- [Matroska V_AV1 mapping](https://www.matroska.org/technical/codec_specs.html#v_av1)

- [AOM AV1 specification, sequence header syntax and semantics](https://aomediacodec.github.io/av1-spec/)
