# VideoToolbox recording adapter validation

The compression adapter is under implementation. The current callback storage
is not registered as a recording driver and does not establish native hardware
encoding or Metal capture support.

`make check` exercises the production callback storage with counted stand-ins
for Core Video/Core Media objects. Tickets are reserved before calling an encoder,
including one that completes synchronously. Pending and borrowed slots cannot be
reused; input and output references survive callback completion until worker
acknowledgement. Errors and null/dropped samples remain explicit results. Owner
retirement leaves accepted tickets valid. Quiescence is permitted only after the
native session cannot invoke or run callbacks.

`make thread_test SANITIZER=thread && ./thread_test` exercises asynchronous
publication and acknowledgement. The busy loop is test-only. Both tests compile
as C89 with warnings as errors and a 2048-byte stack-frame limit.

`session_test` compiles the shipping session adapter against a fault-injected
API stand-in. It checks hardware-required startup and hardware-use verification,
SDR/Main10/PQ settings, allocation/property failures, FIFO output, synchronous
completion, backpressure, dropped frames and failed drain cleanup.

On Apple, `python3 run.py --native` compiles the actual adapter and requires
hardware encoding for H.264 8-bit and HEVC 8/10-bit SDR/PQ fixtures. Test-only
FFmpeg/ffprobe independently check decoded YUV, codec, range and colour tags.
The native executable also checks source timestamps and durations. Return 77
means at least one hardware case was unavailable; it is not a complete pass.
CPU plane fills belong only to this fixture, not the intended capture path.

`python3 capture_run.py` independently compiles Metal capture under ARC and
manual reference counting, without requiring an encoder or FFmpeg. The fixed
CoreVideo buffer/view pool and compute shader are not selected by a renderer
yet. The oracle checks all cropped luma/chroma codes against double-precision
RGB/PQ/scRGB references, both packed channel orders, left chroma filtering,
P010 packing, pending-slot refusal and explicit encoder/output ownership.
Test-only command-buffer waits and CPU plane reads do not enter capture code.
Native compilation/GPU execution must pass before any acceptance claim.

Next validate the hardware-required VTCompressionSession on Apple, then integrate
fixed Metal-compatible pixel-buffer capture and compressed AVFoundation
mux/audio input. Reuse record_gpu_queue for producer/worker ownership. Encoder
calls, final draining and file work must stay off the render thread. Native
Apple codec, pixel, A/V and lifecycle oracles remain required; mock tests are
not native acceptance.

The queue worker now has fault-injected and 5,000-frame two-thread tests using
the real SPSC queue. GPU-pending input, encoder BUSY and borrowed output retain
their slots. Partial release invalidates a consumed sample borrow and can be
retried; cancellation holds GPU work until completion. A one-frame maximum
encoder delay with at least two slots prevents unbounded native buffering
from exhausting the fixed pool. Unsupported delay settings refuse startup.

`python3 pipeline_run.py` is the native ARC/MRC capture-to-encode gate. It
cycles 24 frames through two fixed buffers and the shipping capture, queue,
worker and compression adapters. Independent decoding checks YUV and colour
tags; the native fixture checks borrowed output and PTS/duration. This still
does not establish AVFoundation mux/audio or renderer/controller integration.
