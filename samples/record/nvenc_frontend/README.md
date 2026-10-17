# Native frontend GPU recording

Desktop Windows builds with D3D12, threads, configuration files and the built-in
recording components expose `record_driver = "nvenc"`. Use an SDR 8-bit D3D12
swapchain and viewport recording (Game Only disabled). HDR, streaming, core audio
and asynchronous core audio callbacks are not supported by this frontend mode.
The existing dispatcher supports PCM separately; frontend audio routing remains
pending. Other video drivers and unsupported builds reject this selection.

Pass `--record output.mp4 --recordconfig native.cfg` with this native config:

```
audio_enable = false
vcodec = "h264"
format = "mp4"
video_bit_rate = 16000000
```

`vcodec` accepts `h264`, `hevc`, or `av1`, subject to GPU support. Audio must be
explicitly disabled. Unknown options are rejected. Existing output files are
preserved. `video_record_allow_frame_drop` controls whether backpressure drops a
frame or fails the recording; accepted frames retain their core timestamps.
Capture uses the rendered viewport without CPU pixel readback. A viewport size
change stops and finalizes recording. Startup waits for the first valid viewport.
Normal quit polls asynchronous encoder/file retirement before exiting.

`make check` tests production frame-rate conversion and builds a deterministic
libretro test core. Native hardware acceptance is opt-in:

```
python run.py --core frontend_core.dll --retroarch /path/to/RetroArch.exe
```

The native test covers all three codecs with direct and threaded video, exact
NTSC timestamps, independent test-only FFmpeg decoding, colors, normal quit,
startup cancellation, explicit audio refusal and preservation of existing files.
