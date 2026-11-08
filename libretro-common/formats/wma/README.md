# rwma

C89 float PCM decoder with an exclusively owned context. Construction
allocates transform plans, overlap history and bounded compressed carry.
Decode and reset allocate nothing and perform no I/O, resampling, mixing,
threading or playback pacing.

The implementation provides raw XMA frames, length-prefixed WMA Pro packets,
and WMA1/WMA2 frames with VLC/LSP exponents, noise coding, variable blocks
and bit-reservoir input. This is not yet a complete replacement for FFmpeg
for all FFmpeg use in a core. RetroArch builds include it only with
`HAVE_RWMA=1` (`./configure --enable-rwma`); the default remains disabled.

## Ownership

Input is borrowed for the duration of `rwma_decode_xma`. The bit span may
start at any bit offset and requires no padding past its declared bytes.

`rwma_decode_packet` pulls one frame at a time from a WMA1/WMA2/Pro block.
Keep passing the same block while `consumed` is zero. A successful call
can return zero samples while retaining an incomplete frame or discarding
the initial overlap-priming frame. A nonzero `consumed` releases the block.
Whole frames decode from the caller's bytes; only compressed fragments
crossing a block boundary are copied into the carry.

Returned planes borrow decoder-owned output until the next decode, reset
or free. Conversion, channel mapping and device delivery belong to the
caller. Spectral conversion and the inverse transform reuse the future
output region instead of allocating an intermediate PCM frame.

An invalid frame clears the returned view and poisons the context until
reset. A caller must handle failure rather than replay stale PCM.

## Validation and remaining integration

`make -C libretro-common/samples/formats/wma test` checks raw bit offsets,
a carried frame followed by another frame in the same packet, in-place
transforms against a direct cosine sum, WMA1/WMA2 variable and reservoir
blocks, and deterministic malformed input.
Use `SANITIZER=address,undefined` on supported compilers.

`make -C libretro-common/samples/formats/wma reference` optionally builds a
test-only FFmpeg oracle through pkg-config. Run `rwma_reference input.wma`
to compare packet output counts and every float sample. The explicit
`--truncated-tail` option permits a cut final packet only when both decoders
reject it. No encoded fixtures or FFmpeg libraries are shipped with rwma.

Before replacing the core's decoder, complete variable packet continuation
coverage, versioned checkpoint integration and guest-output differential tests.
The WMA Pro packet path does not yet recover from a sequence discontinuity.

## Playback integration

`rwma_stream` reads WMA1/WMA2/Pro from ASF or RIFF/XWMA with a stable borrowed
input base and a resident-byte bound. Complete media objects are borrowed;
only fragmented objects use the preallocated assembly area. No decoded PCM
copy is held by the stream wrapper. Unknown codecs, encrypted streams and
non-identity ASF audio spreading are refused.

`audio_transfer` exposes this as `AUDIO_TYPE_WMA`; reads pack borrowed planes
directly into caller output. It reports the native sample rate, supports
looping to frame zero and treats a partial-read frontier as starvation.
Arbitrary seeks and caller-demuxed input are not implemented by this arm.
`audio_mixer_load_wma` uses the existing streaming mixer paths. RetroArch's
music loader uses VFS-backed moving windows for WMA, including small files.

The built-in media player accepts `.wma`, `.xwma` and `.xma` when enabled. It keeps
one bounded output interval, prefers float callbacks and falls back to S16
for older frontends. Source-rate intervals use integer remainder accounting,
without a resampler or host clock. Its input is a single-owner data_transfer
window. Audio-only position states and seek controls are not yet exposed.
Existing FFmpeg/MPV player selection takes precedence when built in.

XMA2 WAVE files use up to four interleaved mono/stereo streams. Complete
frames borrow packet storage; only frames crossing packets use bounded
compressed scratch. Startup alignment and PlayBegin/PlayLength are applied
without copying PCM. The final transform overlap is borrowed once through
`rwma_drain_xma`; reset is required before reusing a drained raw decoder.
The raw-frame API retains its original untrimmed 512-sample contract.
Explicit WAVE speaker masks select stereo weights at voice creation, including
FL/FR/LFE files; LFE is omitted from stereo as in the existing mixer policy.
Noncanonical mono/stereo layouts are refused rather than silently remapped.
XMA1/legacy XMA2 headers and embedded loop regions are not implemented.

## Raw XMA checkpoints

`rwma_xma_capture` returns versioned scalar metadata and borrowed 256-sample
transform-overlap tails per channel. It does not allocate or copy PCM. Cold
and failed states have no history payload; drained states cannot emit a tail
again until reset. The caller keeps the decoder quiescent while using a view.

`rwma_xma_restore` validates the entire source before changing the destination,
including configuration, flags, block lengths, finite history and storage aliasing.
It copies history directly into the destination's final overlap storage without
allocation. Its own exact captured view is accepted; cross-lane or partial aliases
into the destination are rejected. A rejected restore leaves the decoder unchanged.

This is a runtime view, not a serialized struct. Save scalar fields and samples in
the consumer's versioned state format, never the pointers or native struct padding.
The next decoded frame remains a borrowed 512-sample view as before.
## WMA Pro frames spanning packets

The packet-size frame-length sentinel selects an open frame walk. Continuation
packets accumulate in reusable storage bounded by sixteen block sizes (at most
512 KiB), rather than a fixed 1 MiB allocation. Literal frame lengths retain the
bounded-frame path. Complete frames still decode from borrowed packet spans.

An incomplete open walk restores only the overlap prefix and previous subframe
lengths before waiting for more input. That undo storage is reserved at creation;
no decode-time allocation or partial PCM publication occurs. Stale data after an
open frame is discarded according to the continuation boundary. The procedural
regression spans five packets and exercises several rollback retries followed by
a normal frame. The Windows-encoded WaxFlow long-frame corpus also matches the
independent FFmpeg reference for all output samples.
`rwma_xma_configure` reuses a raw decoder for the four XMA sample rates and up to
its creation channel count. Create a stereo context to reserve both channel lanes;
it can then switch between mono and stereo without allocation. A changed profile
rebuilds its band mapping and resets overlap; an unchanged profile preserves its
history. Invalid profiles leave the decoder unchanged. FFT/window storage is reused.
Raw XMA performance work uses bounded 32-bit bit-reader lookahead, immutable
8-bit Huffman prefix tables, cached binary32 gains and SSE2 transform/matrix
strides. Small transforms write their FFT permutation directly, and paired
post-rotation stores each output sample once. Other targets retain scalar
implementations; `RWMA_NO_SIMD` tests that path. No decoder allocation or file I/O
is added to playback. The sample checks prefix/tree identity, reader bounds,
gain values and transform accuracy before real-stream differential timing.

On the measured Windows x64 MSVC `/O2` corpus, native raw XMA beats the existing
FFmpeg raw decoder: approximately 6.1 versus 6.4 microseconds per 512-sample
stereo frame across 376 frames. Separate 17-frame stereo/mono streams measure
about 3.5/1.4 versus 4.9/1.7 microseconds. These are alternating-order offline
runs with preallocated decoder/packet storage, not gameplay, all WMA formats or
all platforms. Native/FFmpeg float error stays below 3.6e-7 in the 376-frame
comparison; the existing guest S16 conversion differs by at most one LSB.
