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
Native Windows, GDB, and real D3D11/D3D12 hardware decode/import are required.
Unsupported hardware or CPU fallback fails the GPU acceptance assertions.

The shipping core decodes a reordered HEVC/AAC movie. A test-only poke rejects
one movie image, causing CPU fallback; the actual reinit command then replaces
the driver and restores GPU delivery. Twelve pictures, including a repeat of
the previous picture at the injected refusal, are checked against fixed FFmpeg
references. The oracle does not search frame order or orientation. Only flat
5x5 reference patches are sampled to separate colour correctness from chroma
filtering. Null audio still observes and forwards the core PCM callback; this
does not establish real audio-device timing or all HDR/preset behaviour.
