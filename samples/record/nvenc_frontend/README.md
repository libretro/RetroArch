# Native frontend GPU recording

Desktop Windows builds with D3D12, threads, configuration files and built-in
recording components expose `record_driver = "nvenc"`. Use an SDR 8-bit
D3D12 swapchain and viewport recording (Game Only disabled). HDR, streaming and
asynchronous core audio callbacks are not supported. Core audio can be recorded
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
change stops and finalizes recording. Startup waits for the first valid
viewport. Normal quit polls asynchronous encoder/file retirement before
exiting.

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
