# rwma

C89 float PCM decoder with an exclusively owned context. Construction
allocates transform plans, overlap history and bounded compressed carry.
Decode and reset allocate nothing and perform no I/O, resampling, mixing,
threading or playback pacing.

The implementation provides raw XMA frames, length-prefixed WMA Pro packets,
and WMA1/WMA2 frames with VLC/LSP exponents, noise coding, variable blocks
and bit-reservoir input. This is not yet a complete replacement for FFmpeg
in a core or media player. RetroArch builds include it only with `HAVE_RWMA=1`.

## Ownership

Input is borrowed for the duration of `rwma_decode_xma`. The bit span may
start at any bit offset and requires no padding past its declared bytes.

`rwma_decode_packet` pulls one frame at a time from a WMA Pro block.
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
coverage, decoder state serialization and guest-output differential tests.
The current WMA Pro packet path does not implement frames whose length
exceeds one block, or recovery from a sequence discontinuity.

The requested frontend integration includes separately gated rwma decoding,
ASF/XWMA container input for `audio_transfer`, streaming mixer loaders,
RetroArch's built-in media player, and enabled/disabled build tests. Container
input must use borrowed packet spans or reusable assembly storage; it must
not require loading or decoding the entire file before playback.
