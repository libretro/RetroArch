# SDL3 raw-write contract

Run `make check` with SDL3 development files available through pkg-config.
`SDL_CFLAGS` and `SDL_LIBS` can instead point to an SDK. Requires GCC's
`-fwhole-program` for this focused include harness. `SANITIZER=address,undefined`
is supported where the compiler provides it. No audio device is opened.

The shipping driver feeds real SDL3 unbound streams. Thin API wrappers inject
format/rate/gain failures, initial and later queue failures, and fragmented
queue space. Checks cover input-frame counts, native input bytes queued,
nonblocking short writes, zero-length requests, invalid parameters, cache
recovery and ordinary byte-write/frame-accounting behavior. They do not certify
physical playback or the driver's approximate consumption/queue-depth model.

This optional SDK-dependent sample adds no shared CI job or matrix variant.
