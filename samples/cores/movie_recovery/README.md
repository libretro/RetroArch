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

Add --threaded to exercise the actual video-thread wrapper. The harness asserts
that the requested wrapper state is active before and after driver replacement.
Use a separate output directory for each direct/threaded run.

Generate --depth 10 for Main10, or --range full for explicitly converted
full-range samples (the options can be combined). The generator verifies the
coded profile, bit depth, range and BT.709 metadata with FFprobe before saving
the manifest. These remain SDR transfer-function fixtures, including Main10;
they do not establish PQ/HDR movie output acceptance.
The capture fixture enables vsync: Vulkan synchronous readback can report
no acquired image with Windows mailbox emulation when vsync is disabled.
A failed or unavailable readback fails this numerical fixture; it is not
counted as a successful picture or silently retried with another ordinal.

Because null audio supplies no device throttle, the fixture waits one nominal
core frame interval before each run. This is test-only pacing, preserving the
production rule against adding waits to the frame path. It does not measure
audio-device synchronization.

The independent RGB reference explicitly uses bilinear chroma interpolation,
accurate rounding and full chroma precision. Default bicubic interpolation can
overshoot near saturated chroma boundaries, even where clipped RGB patches
appear flat. Decoded Main10 YUV samples and a double-precision BT.709 matrix
confirmed this distinction; the three-value comparison tolerance is unchanged.

Select `--window-mode resize` to grow the real window to 512x384 on tick two,
then shrink it to 320x240 on tick seven. `--window-mode fullscreen` uses
borderless fullscreen over the same ticks and verifies the actual HWND style
and monitor rectangle, then returns to 320x240. Driver replacement on tick nine
restores the configured 256x192 window. The default `steady` mode preserves the
original fixed window. Each run needs its own fresh output directory.

These modes keep the same twelve fixed picture ordinals and comparison tolerance.
The changing viewport dimensions are asserted and recorded in `result.json`.
Fullscreen requires a display large enough to exceed 512x384, with a captured
viewport no larger than 4096x4096. The sample explicitly enables windowed
fullscreen and does not change global display resolution or Windows HDR state.
Test-only queue drains and cached redraws apply pending resizes before viewport
queries; they do not advance the core or add production frame-path waits.

Each run also saves the exact stereo S16 core callback stream as `audio.s16`.
Per-tick callback counts, runloop flags, menu flags and focus state distinguish
core progress from a cached picture that is still being presented. The null
audio driver does not establish physical audio-device delivery.

For coded HDR input, run `python samples/cores/movie_recovery/mkhdrfixtures.py DIR`.
It generates Main10 BT.2020 PQ and HLG movies in `DIR/pq` and `DIR/hlg`, with
fixed RGB references from FFmpeg-decoded YUV planes and the double-precision
import color oracle. Pass either directory to `run.py --fixtures`. The probe
selects SDR output (`video_hdr_mode = 0`), so this covers HDR input conversion,
hardware import, CPU fallback and recovery, rather than HDR display luminance.
Use `mkhdrfixtures.py DIR --primaries bt709` for explicitly signalled BT.709 HDR,
which must retain BT.709 primaries during both GPU import and CPU fallback.

Use `--codec vp9` for ten-bit VP9/Opus WebM. Add `--unspecified-matrix`
with BT.2020 primaries to change only the container matrix code to 2,
exercising the HDR default without changing coded packets. The manifest names
the movie file, so `run.py` accepts either container. WebM recovery keeps
presenting current CPU pictures while the replacement GPU decoder catches up.
The probe requires the GPU to resume by the final tick, checks every fixed
picture ordinal, and records each intermediate `RECOVERY_CPU` tick. The
existing MP4 replacement hold remains explicitly checked against ordinal 8.
For MP4 content-peak conformance, `--max-cll 4000` adds a container `clli`
box without changing coded packets and generates the matching PQ reference.
HLG keeps its 1000-nit reference curve regardless of that metadata.

The probe temporarily sets the logical input-port count to zero while retaining
device polling and cleanup. Requesting a nonexistent null joypad driver can otherwise
fall back to a connected controller and let live hotkeys change playback or
window geometry. The original port count is restored for driver replacement
and final cleanup; the suppression exists only in the sample process.
