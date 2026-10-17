# Bounded native MP4 output

`record/record_mux_mp4` builds an initialization segment and one moof/mdat
fragment per encoded access unit. H.264/HEVC Annex B input becomes four-byte
NAL lengths; AV1 uses sized OBUs with temporal delimiters/padding removed.
AVC/HEVC/AV1 configuration comes from the producer's sequence headers. Input
is restricted to matching 4:2:0 8/10-bit configurations (H.264 is 8-bit), one
parameter set per kind, and DTS=PTS without B frames. The optional AC-3 track
uses the existing frame parser and its own sample-rate clock.

All buffers are allocated at open. Each output chunk remains borrowed until
release; BUSY leaves input/timing/sequence state untouched. Failed input does
not advance either track. Timestamps are 64-bit, durations are nonzero 32-bit
track ticks, and overlap is refused per track while gaps are retained.
Acknowledged fragments form a playable prefix. Finish needs no seek-back,
index or trailer, but the caller must still drain/close its asynchronous
output and report file errors. There is no file I/O or pixel readback here.
Frontend scheduling, async file output and live core-audio hookup remain
pending; this does not register a frontend recording mode.

`make check` runs bounds/ownership/error tests and 20 independently decoded
A/V files: H.264, HEVC/AV1 8/10-bit, video-only or native AC-3 at 48 kHz stereo,
44.1 kHz mono and 32 kHz 5.1. FFmpeg/ffprobe are test-only dependencies.
The oracle checks pixels, audio samples, codec/geometry/bit depth, per-track
timestamps, duration, keyframes and recovery of complete-fragment prefixes.
`make check CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine` uses the same oracle.

Format references: [fragmented ISO BMFF](https://www.w3.org/TR/mse-byte-stream-format-isobmff/),
[AV1 binding](https://aomediacodec.github.io/av1-isobmff/), and
[AC-3 dac3](https://www.etsi.org/deliver/etsi_ts/102300_102399/102366/01.04.01_60/ts_102366v010401p.pdf), Annex F.

On Windows the fixture drains each MP4 borrow through the production overlapped
file writer, then flushes/closes before invoking the independent decoder.
