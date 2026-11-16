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
