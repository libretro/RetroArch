# Native frontend GPU recording

Desktop Windows builds with D3D12, threads, configuration files and built-in
recording components expose `record_driver = "nvenc"`. Use viewport recording
(Game Only disabled). SDR uses 8-bit BT.709 output. Active PQ or scRGB output
can be recorded with HEVC or AV1 as 10-bit, limited-range BT.2020/PQ; H.264
HDR recording is refused. Admission follows the renderer's active output,
including fallback to SDR, rather than the requested HDR setting. Display peak
brightness is not used to invent content-light or mastering metadata.
Asynchronous core audio callbacks are not supported. Core audio can be recorded
as native AC-3 at 32, 44.1 or 48 kHz.
The recording keeps the speaker mask declared when it starts, independently
of the playback device layout. AC-3 layouts up to six channels are accepted:
mono, stereo, front and back centre variants and side-surround pairs, each with an
optional LFE. Rear-pair quad/5.1 and 7.1 are refused; rear speakers are not
relabeled as side speakers. Other sample rates are refused; this driver does
not install a resampler. Other video drivers and unsupported builds reject this selection.

Pass `--record output.mp4 --recordconfig native.cfg` with this native config:

```
audio_enable = false
vcodec = "h264"
format = "mp4"
video_bit_rate = 16000000
```

`vcodec` accepts `h264`, `hevc`, or `av1`, subject to GPU support. Set
`audio_enable = true` to record core audio as AC-3 (192 kbit/s for one or two
channels, 448 kbit/s for wider layouts); `false`
keeps video-only recording. An explicit setting is required. Unknown options
are rejected. Existing output files are preserved.
`video_record_allow_frame_drop` controls whether backpressure drops a frame or
fails the recording; accepted frames retain their core timestamps. Audio and
video begin together when the worker is ready. Audio queue exhaustion or a core
sample-rate change fails recording without waiting or silently losing audio.
Normal stop lets already queued video frames complete before retirement.
Capture uses the rendered viewport without CPU pixel readback. A viewport size
change stops and finalizes recording. An output-encoding change also stops
recording before a frame with the new encoding can enter the old session.
Startup waits for the first valid
viewport. Normal quit polls asynchronous encoder/file retirement before
exiting.

## Native RTMP streaming

Build with `HAVE_MF_AUDIO=1 HAVE_RTMP=1 HAVE_RFLV=1` and cryptography enabled.
Set `video_stream_url` to an `rtmp://` or `rtmps://` publishing URL and
`video_stream_config` to a native config:

```
audio_enable = true
vcodec = "h264"
format = "flv"
video_bit_rate = 8000000
```

Start/stop using the normal streaming action. Optional
`rtmp_legacy_commands = true` supports peers requiring legacy command syntax.
RTMPS also requires the SSL transport. This path accepts SDR H.264 and native
AAC-LC at 44.1/48 kHz, mono/stereo; unsupported HDR/codecs/layouts are refused.
No output file is created. Inputs start together after publish setup completes.
URL setup and final drain run on the I/O worker with 30/10-second deadlines.
Enhanced RTMP HDR and other native platform encoders remain separate work.

`samples/gfx/nvenc_d3d12/stream.py` exercises real GPU capture, native AAC,
RTMP delivery and independent received video/audio decoding. It requires
native NVIDIA hardware and an FFmpeg build with the native RTMP listener;
its output directory retains publisher/receiver logs and reference media.

`make check` tests production frame-rate conversion, PCM admission and queued
video retirement, and builds a deterministic libretro test core. Native
hardware acceptance is opt-in:

```
python run.py --core frontend_core.dll --retroarch /path/to/RetroArch.exe
```

The native test covers all three codecs with direct and threaded video, exact
NTSC timestamps, independent test-only FFmpeg decoding, colors, all three audio
rates, stereo tone identity, exact padded PCM counts and A/V duration
agreement. It also checks normal quit, startup cancellation, unsupported audio-
rate refusal and preservation of existing files.
