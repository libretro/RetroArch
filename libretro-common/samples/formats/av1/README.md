# AV1 container and OBU views

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

The helper does not parse sequence/frame/tile payload syntax, validate
configuration agreement with a sequence header, reconstruct frames, or
implement Annex B. Hardware session/reference/film-grain handling and
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
`fixtures.py` generates four 12-packet 8/10-bit AV1 MP4/WebM files with
FFmpeg/libaom and compares configuration bytes, packet hashes, timestamps
and sync flags with ffprobe. FFmpeg is a test tool, not a runtime dependency.

Format references:

- [AOM AV1 ISOBMFF binding, sections 2.2-2.4](https://aomediacodec.github.io/av1-isobmff/)
- [Matroska V_AV1 mapping](https://www.matroska.org/technical/codec_specs.html#v_av1)
