# Vulkan recording frontend contracts

`make check SANITIZER=address,undefined` exercises the shipping recording
controller with injected Vulkan and frontend operations. It checks admission,
configuration rejection, exact frame timestamps, PCM ownership, duplicate
frames, startup cancellation, wrapper-held frame references and asynchronous
retirement, including device loss. These contracts do not establish a hardware
encode or presentation pass.

The desktop driver is selected automatically by configure when its dependencies
are enabled. Use `--disable-record-vulkan` to omit it; makefile integrations can
select `HAVE_RECORD_VULKAN=1` explicitly.
It requires Vulkan, threads, configuration files, RMP4 and RSLC. Select
`video_driver = "vulkan"`, `record_driver = "vulkan"`, and viewport recording
(Game Only disabled). Admission is SDR H.264 or HEVC Main/Main10 in MP4, with optional
native AC-3 at 32, 44.1 or 48 kHz. Active PQ or scRGB output requires
HEVC Main10; capture encodes BT.2020/PQ once. HEVC requires an enabled codec
extension on the reserved encode queue. Unsupported devices, output formats,
asynchronous core audio and unsupported speaker layouts are refused. Existing files are preserved.
Native Linux GPU acceptance remains separate from a successful Linux build.

Example recording configuration:

```
audio_enable = true
vcodec = "h264"
format = "mp4"
video_qp = 24
```

For HEVC, set `vcodec = "hevc"` and optionally `video_bit_depth = 10`
(the default is 8). H.264 accepts only bit depth 8.

Run the shared native frontend oracle with `python
../nvenc_frontend/run.py --driver vulkan --codecs hevc --bit-depth 10 --core CORE
--retroarch RETROARCH --output-dir OUTPUT`. On Windows, supply
`--desktop-runner RUNNER` to use the existing isolated desktop harness.
The shared oracle checks independently decoded pictures, packet timestamps,
audio clocks, channel tones, unsupported configurations and shutdown. Add
`--float --layouts 4 0x603 0x60f` for supported multichannel float PCM.
Use a fresh output directory for each run. Add `--lifecycle resize`,
`--lifecycle reinit` or `--lifecycle stop` to drive only the isolated session and
verify recording drain plus continued frontend frame progress. These native
runs require network commands. Vulkan validation can be enabled with
`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`, the appropriate
`VK_LAYER_PATH`, and `VK_LAYER_VALIDATE_SYNC=1`; validation errors fail the oracle.
Portable contracts cannot replace native acceptance.

The worker bounds consumed PCM to accepted video timestamps. Queued audio past
video EOS is discarded before AC-3 padding, so a resize cannot leave audio ahead
of the final picture. Finalized PCM counts exclude this tail and codec padding.

## Native Windows streaming

With native AAC, RTMP/FLV, networking and cryptography enabled, select the
normal streaming action and configure an `rtmp://` or `rtmps://` URL. Use the
same native profile with `format = "flv"`. Optional `rtmp_legacy_commands = true`
selects legacy command syntax. Audio supports mono/stereo at 44.1/48 kHz;
`audio_enable = false` selects video-only. RTMPS additionally needs SSL.
Other platforms currently refuse this frontend streaming path.

The existing I/O dispatcher owns connection setup, readiness and retirement.
Capture remains on the GPU. Streams never create MP4 files or video-history
entries. Resize/output-mode changes stop capture and drain the existing owner.

Run `../nvenc_frontend/stream.py --driver vulkan` with `--retroarch`, `--core`,
`--desktop-runner` and a fresh `--output-dir`. Add `--codec hevc`, `--no-audio`
or `--lifecycle resize/reinit` for the corresponding cases. The oracle checks
receiver-decoded pictures, timestamps, audio and frame progress after drain.
Use an absolute `VK_LAYER_PATH`; the frontend changes its working directory.
With `VK_LOADER_DEBUG=layer`, the test requires evidence of layer insertion.

For a local TLS receiver, build a test-only frontend entry with
`../nvenc_frontend/build_stream_tls.py --build-root BUILT_TREE --output TEST_EXE`,
then run the oracle with that executable and `--tls 1.2` or `--tls 1.3`.
Only this test entry consumes the generated `RTMPS_CA`; production trust and
host certificate stores stay unchanged. HDR signal checks use
`--codec hevc --bit-depth 10 --hdr-mode 1` (PQ) or `2` (scRGB). They verify
encoded signal values and metadata, not calibrated physical-display luminance.

`../nvenc_frontend/stream_faults.py` exercises native Windows Vulkan connection
refusal, cancellation after a silent peer receives C0/C1, and receiver loss
while publishing. It checks frontend frame progress, automatic stream-state
reset, clean asynchronous retirement and a fresh successful stream in the same
process. Both H.264/HEVC and direct/threaded video are exercised. Supply the same
`--retroarch`, `--core`, `--desktop-runner` and fresh `--output-dir` arguments as
the normal stream oracle. Only the test's owned sockets/receiver processes are
interrupted. The recovered stream must independently decode and retain the
published packet count. These tests require actual native GPU execution and
Vulkan validation; portable units do not substitute for them. They do not
establish sustained network-backpressure or memory-bound acceptance.

The portable `samples/record/rtmp_stream/vulkan_pressure_test` fills the two-slot
video queue and optional PCM FIFO while the RTMP transport holds a borrowed span.
For 10,000 blocked service calls it requires BUSY without producer admission,
packet mutation, encoder release or any allocation. A fixed test-only ledger
instruments allocation calls in every linked shipping translation unit, without
platform linker hooks. Resume must drain the accepted frames/samples; cancellation
must retire all allocations. Both paths run with video alone and with audio.
This is a bounded session/mux/queue contract with simulated encoder and transport;
it does not replace native GPU/network slow-peer or operating-system memory tests.

For native Windows slow-peer recovery, run the stream oracle with
`--driver vulkan --no-audio --stall-seconds 8` and H.264 or HEVC. Build `--core`
from the current `../nvenc_frontend/core.c`; the oracle enables its deterministic
noise mode to fill socket buffers. A test-only relay has at most 64 KiB queued
per direction and pauses reads from the frontend for the requested interval.
The test requires continued core frame progress, no forwarded media during the
pause, at least a half-second recording timestamp gap proving frame pressure,
then resumed traffic, exact finalized/received frame counts and independent decode.
It samples only the frontend child owned by its isolated desktop launcher:
`pressure-memory.json` retains private-byte/working-set traces. Growth must stay
below 64 MiB, and the latter half of the stall must span less than 16 MiB.
These are bounded-interval process measurements, not GPU-memory accounting or a
long-duration leak proof. Use a receiver build with native RTMP listen support;
some external-librtmp FFmpeg builds ignore the listener option. Active audio
exhaustion, shutdown while the peer remains stalled, TLS stalls and broader
platform acceptance remain separate gates.

The failure oracle also accepts `--faults pressure-audio pressure-stop`.
These cases keep the receiver connected but stop consuming data. Audio FIFO
exhaustion must fail without waiting or silently dropping audio. Video-only stop
must retire within the worker drain deadline and report Windows ERROR_TIMEOUT
while core frame queries continue progressing. The receiver timeout is longer
than the worker deadline; no receiver socket is closed to force this result.
Both cases require automatic stream-state reset and a fresh successful stream
in the same frontend process, with exact packet counts and independent decode.
Supply the current noise-capable fixture core. Relay resets are tolerated only
for the deliberately failed connection, and recovery uses the same owned port.

`--faults pressure-quit` quits the frontend while the peer is still connected
and stalled. It requires the worker timeout, zero native GPU errors and an
unforced zero-status frontend exit within 15 seconds. Receiver teardown happens
only after the owned frontend exits; it cannot manufacture a successful drain.
This quit case has no same-process recovery requirement because that process exits.
