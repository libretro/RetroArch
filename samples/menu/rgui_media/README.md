# RGUI media controls

Build the non-Qt frontend before running this harness. It links the shipping
objects with the real RGUI translation unit and replaces only `main()`.

With the default null video driver, the harness requires software playback
and checks both CPU thumbnails:

```sh
python3 samples/menu/rgui_media/run.py --c89 movie.mp4
```

Capable Vulkan and D3D drivers prefer the shared GPU player. Require GPU
presentation and hardware decoding explicitly for a supported movie:

```sh
python3 samples/menu/rgui_media/run.py --video-driver vulkan --gpu-surface --hardware-decode movie.mp4
python3 samples/menu/rgui_media/run.py --video-driver vulkan --gpu-surface --hardware-decode --threaded movie.mp4
```

Use `--objdir` to select the frontend build directory, `--shell` for the build
shell, and `--assets` for the menu assets. On Windows the runner uses GDB.
With `--hardware-decode`, an imported hardware surface must supply the picture;
a software-decoded GPU upload does not pass. Hardware admission failure is a
test failure; it is not a passing device gate.

Surface mode checks rendered forward/backward seeks, replacement requests,
the pointer timeline, pause/Stop, fullscreen/inline transitions, repeated
retirement during pending seeks, and recovery after a missing movie. The
fixture must change between zero and 375 ms and allow a seek to one second.
For a fixture whose first picture has a nonzero timestamp, set
`--first-video-us` to that timestamp in microseconds. This changes capture
comparison targets only; it does not retime playback or remove leading gaps.
Set `MEDIA_TEST_CAPTURE_DIR` to an existing directory to save PPM captures.
Captures 0-3 cover fullscreen seeks, 4 is inline, 5 is reopened fullscreen,
and 6 is the browser after retirement. Readback exists only in the harness.

These semantic-action tests do not establish physical keyboard/gamepad/remote
mapping, playback-context ownership, or physical HDR display conformance.

The tested Windows NVIDIA driver fails the opt-in
[H.264 host-header lifetime probe](../../gfx/vulkan_video_caps/README.md#h264-external-and-host-lifetime-probes).
Default validation handle wrapping can expose the same delayed host-header
read during submission. Keep that failed conformance result separate from
normal playback and color checks; this runner does not disable validation.
