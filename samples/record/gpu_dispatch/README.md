# D3D12 recording dispatcher

A dedicated I/O worker constructs, services and retires the recording session.
The cold controller copies configuration and the UTF-8 output path at creation.
The render and PCM producers return BUSY during startup, consuming nothing.
Once ready they preserve capture and whole-PCM-batch backpressure. The desktop Windows NVENC frontend attaches GPU capture and optional multichannel
core audio; see `../nvenc_frontend/README.md`.

Both producers close from their own threads after their last push returns,
including startup failure and cancellation. Disabled audio starts closed.
Cancellation requests drain and stops admission; it does not close producers
on their behalf. Collection checks thread completion without joining or
waiting. The borrowed constructor refuses active resources; retain capture until
dispatch collection and its GPU retirement succeed. Capture owns submitted source/fence references;
its render owner reaps completed sources before a swapchain resize.
A persistent SDK or owned-handle failure may retain resources for safety.

The worker registers before rechecking for work, then waits for producer
notifications or a borrowed encoder/file completion handle. An idle worker
has no periodic timer. SDK BUSY without completion, or BUSY with an already
signaled completion, schedules a short retry on the I/O worker. Producers
notify with an atomic operation and signal the event only while parked.
There are no added waits, locks or allocation on frame/audio entry points.

`make check` exercises the production dispatcher through deterministic park
races, startup/cancellation boundaries, concurrent producers, completion
waits and SDK retries. Windows uses native kernel events and threads. Linux
uses a test-only pthread Win32 scheduling shim to run that same production
code under ASan/UBSan and TSan; shim locks model the kernel waits only.
The separate threadless executable checks the unavailable backend.

The composed fixture uses the real session, GPU packet pump, queue, AC-3 and
MP4 mux, with simulated capture/encoder and file operations. It checks normal
completion, cancellation, partial startup failure and file failure, including
lease retirement. These tests do not establish native GPU acceptance.

The owned constructor transfers a fresh capture only on successful construction.
Its worker destroys the session, then retries capture destruction until GPU
copies and source references retire. DONE/FAILED therefore also cover capture
retirement. Neither producer may access the transferred capture directly;
the render producer uses video_reap before closure when preparing a resize.
Both producers must finish their last API calls before closing, including on
startup failure and cancellation. Failed construction leaves capture ownership
with the caller. Delayed GPU retirement is retried only on the I/O worker.

Owned-lifetime fixtures hold simulated GPU completion after session destruction,
refuse controller collection, and verify exactly one capture destruction after
completion. They also cover startup/cancellation, disabled audio, producer-close
ordering, construction failures and closed-producer reap refusal. The composed
session/AC-3/MP4 fixture exercises both ownership modes.
