# Actual builtin movie recovery on native Windows

Configure and build a normal non-Qt frontend with builtin media enabled and
FFmpeg disabled. Keep its object directory separate from strict-C89 builds.
From MSYS2, link the shipping frontend with this test main:

```
./configure --disable-qt --disable-ffmpeg --enable-webmplayer
make -j8 OBJDIR=obj-unix/movie-recovery
MAKE_ARGS=OBJDIR=obj-unix/movie-recovery sh samples/cores/movie_recovery/build.sh
```

Generate fixtures with `python3 samples/cores/movie_recovery/mkfixtures.py DIR`
on a host with FFmpeg/libx265. This dependency generates independent test data;
the frontend has no FFmpeg linkage. Transfer the fixture directory if needed.
Run `python samples/cores/movie_recovery/run.py --fixtures DIR --output RESULTS`.
Use --drivers d3d11,d3d12,vulkan to include Vulkan Video.
Native Windows, GDB, and real hardware decode/import are required.
Unsupported hardware or CPU fallback fails the GPU acceptance assertions.

The shipping core decodes a reordered HEVC/AAC movie. A test-only poke rejects
one movie image, causing CPU fallback; the actual reinit command then replaces
the driver and restores GPU delivery. Twelve pictures, including a repeat of
the previous picture at the injected refusal, are checked against fixed FFmpeg
references. The oracle does not search frame order or orientation. Only flat
5x5 reference patches are sampled to separate colour correctness from chroma
filtering. Null audio still observes and forwards the core PCM callback; this
does not establish real audio-device timing or all HDR/preset behaviour.

A decoder-capacity stall during driver replacement may repeat the last CPU
picture on tick nine. The harness reports that event from callback counts;
the oracle then requires the retained ordinal eight at that tick and ordinal
ten on the following tick. It does not search the reference pictures or allow
an ongoing frame offset. Replacement must keep wall time and audio moving,
resume GPU delivery, and keep the exact total CPU/GPU callback counts for the
observed immediate or one-tick-stalled replacement. A missing cached picture
in the replacement renderer must not turn the retained CPU picture black.
