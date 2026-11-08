# XMA / WMA decoding

`include/formats/rwma.h` provides C89 raw XMA-frame and WMA v1/v2/Pro packet
decoders. Compile `rwma_pro.c` and `rwma_standard.c` and link the platform math
library. RetroArch's optional `HAVE_RWMA=1` Makefile setting adds these sources;
it does not enable a frontend player or alter existing playback.

The spectral, channel-transform and overlap algorithms and codebooks are adapted
from FFmpeg's LGPL-2.1-or-later WMA decoders. Their notices and license are retained.
This is a self-contained decoder port, not an FFmpeg API wrapper: it has
no libavcodec/libavformat dependency. The private radix-2 DCT-IV uses reusable
storage, and all Huffman trees are immutable compile-time tables. There is no
runtime global table initialization, shared mutable codec state or locking.
The source reference is `libretro-fork/parallel-xbox360` at `34a3fb2ad`,
`third_party/FFmpeg/libavcodec/{wmaprodec,wmadec,wma}.c` and their codebooks;
the raw-XMA frame interface includes that fork's packet-loss reset behavior.

## Input and output ownership

- Create one context per independent stream with `rwma_create`. All allocation
  happens here. Decode and reset reuse the storage; free releases it.
- XMA input is one complete raw frame including the 15-bit length prefix. Pass
  the actual bit offset and length with `rwma_decode_xma`: no padding byte,
  aligned reconstruction or encoded-frame copy is needed. Mono and stereo
  streams decode independently; the caller handles the Xbox packet transport,
  multistream layout, guest output rings and scheduling.
- WMA input is already demuxed. Supply its format extradata, block alignment,
  sample rate, channel count and (for v1/v2) bitrate at creation. Pass a packet
  window to `rwma_decode_packet`. Advance by `consumed`. Zero consumption means
  the same window must remain lent for the next frame; positive consumption
  releases that prefix. Do not replace an outstanding input window.
- Returned float planes borrow decoder storage until the next decode, reset or
  free. Consume them directly or convert straight into the final destination.
  No separate PCM output buffer or per-frame PCM publication copy exists.
- Raw XMA trim values and WMA v1/v2 priming delay are metadata in the returned
  frame; apply them once in the consumer. WMA Pro follows its packet decode
  semantics: suppress the initial priming frame and apply stream trims.
- After finishing packet input, a zero-byte packet call returns the remaining
  overlap once. Empty streams and repeated drains produce no frame. Reset starts a new stream;
  it is not a savestate restore. Errors clear the output view; reset before
  continuing after malformed compressed data.

Complete length-prefixed WMA Pro frames borrow packet bits. A compressed frame
split across packets needs a bounded retained reservoir copy. WMA v1/v2 retain
their packet tail for the same reason. Overlap history still moves inside
reused storage; this is not an entirely copy-free codec. Nothing here reads a
file, resamples, mixes, paces playback or submits to an audio device.

WMA Voice, WMA Lossless, DRM and ASF/XWMA/WAV demultiplexing are not implemented.
WMA Pro handles up to eight channels, with the inherited limitation on default
decorrelation matrices for groups larger than six; unsupported syntax fails.
Porting the core still needs packet/trim integration, decoder-history savestate
support and full-game audio validation. FFmpeg remains in parallel-xbox360.

## Verification

`samples/formats/wma` builds without FFmpeg. Its procedural fixtures check
196,608 XMA PCM values against an independent cosine-sum/overlap reference,
mono/stereo channels, unaligned input, signs, spectral positions and retained
history. IMDCT lengths 64 through 8192 are checked independently. Allocation
failure, 10,000 malformed frames/packets, reset, WMA packet continuation and EOF
are covered. Allocation hooks assert no decode/reset allocation. A polarity
negative control fails the PCM test. Linux ASan/UBSan and MinGW checks pass.

The optional `make reference` target needs FFmpeg development libraries only
for the test oracle. CI encodes procedural WMA v1/v2 streams, including a
low-bitrate noise-coding case, and compares native float PCM. The oracle's
headers require a C99 compiler mode; the decoder sources remain strict C89.
No music or game samples are checked in or downloaded by CI.

Additional local differentials used 376 game XMA frames (385,024 channel
samples), and the FFmpeg FATE WMA Pro stereo/5.1 samples (44/102 output frames).
Maximum absolute PCM differences in those runs were below 3.6e-7 for XMA and
7.5e-8 for Pro. Both implementations rejected the truncated terminal packets
in the Pro files. These are local coverage, not public CI fixtures or a claim
of exhaustive bitstream compatibility, gameplay accuracy or faster decoding.
