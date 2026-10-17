# GPU video and PCM output pipeline

The production component borrows an existing D3D12 GPU worker and combines
its packets with signed 16-bit interleaved PCM through native AC-3, the bounded
MP4 mux and the overlapped Windows file writer. Construct, service and destroy
on an I/O worker. This does not create a thread, install a scheduler, attach
frontend capture or register a recording driver. Production linkage requires
HAVE_D3D12 and HAVE_RAC3. PCM rates/layouts must be accepted by the AC-3 encoder;
there is no resampling. The caller supplies one ordered PCM producer.

One bounded service call handles file completion/submission, one video worker
step, or one 1536-sample audio block. Audio and video alternate when ready.
Disk backpressure retains the mux borrow, then fills the GPU pool and PCM
ring. Whole PCM batches return BUSY without consuming samples; callers must
retain/retry or explicitly fail recording, never silently drop audio. Close
both producers separately. A partial final audio block is zero-padded, adding
at most 1535 stored samples. No priming silent frame is inserted.

END requires GPU EOS, closed/drained PCM, acknowledged mux output and a
successful worker-only file flush/close. On error or abort, close/quiesce the
PCM producer and retire any file borrow through step before free. The caller
still owns the GPU producer, worker and encoder: stop/drain/free them through
their existing contracts; output destruction does not release GPU resources.
No main/render/audio thread may service the output pump, including in a
threadless task runner.

After BUSY, the worker can borrow `record_gpu_output_d3d12_wait_handle()`:
a pending file write takes precedence over the oldest encoder completion.
The file handle also covers cancellation drain. Re-query after every step;
never close or reset a borrowed handle. The scheduler must also wake on
producer input/close and cancellation, and arrange retries for SDK BUSY when
there is no pending completion. This API does not install that scheduler.
The native GPU A/V harness waits on completion and producer notifications;
its finite watchdog and no-handle retry timeout are test-only.

`record_gpu_session_d3d12` owns encoder startup, the GPU pump and output on
one I/O worker. Both producers close independently; cancellation only requests
drain. Its step returns terminal END/ERROR after owned resources retire,
including partial startup and device-removal cleanup. Capture and source GPU
lifetime remain with the caller. A refused encoder destruction retains leases
for a later service attempt. Error/detail reads are atomic. This lifecycle
component does not install the worker dispatcher or frontend capture.

`make check` also exercises session startup failure, pending-write cancellation,
producer closure, encoder destruction refusal and concurrent PCM retirement,
with both threaded and threadless builds. `make session_test SANITIZER=thread`
runs the session ownership tests under TSan.

`make check` runs fault/ownership tests, a concurrent PCM producer with ring
wrap and close/drain, and 20 independent decoded MP4 A/V cases using the shared
mux oracle. Linux uses a test-only stdio file backend and simulated encoded
packet producer; Windows uses the production overlapped writer. FFmpeg and
ffprobe are test-only. `make thread_test SANITIZER=thread` provides TSan.
The same check builds with mingw-w64 and Wine. The native NVENC device harness
also exercises the complete capture/encode/PCM/mux/file chain with a separate
PCM producer, comparing video to raw encoded output and audio to a separately
encoded AC-3 reference, including padding and exact sample timestamps.
