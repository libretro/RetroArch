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
(Game Only disabled). Current admission is SDR H.264 in MP4, with optional
native AC-3 at 32, 44.1 or 48 kHz. Unsupported devices, HDR, asynchronous core
audio and unsupported speaker layouts are refused. Existing files are preserved.
Native Linux GPU acceptance remains separate from a successful Linux build.

Example recording configuration:

```
audio_enable = true
vcodec = "h264"
format = "mp4"
video_qp = 24
```

Run the shared native frontend oracle with `python
../nvenc_frontend/run.py --driver vulkan --codecs h264 --core CORE
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
