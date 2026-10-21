# RGUI media controls

Build the non-Qt frontend before running this harness. It links the shipping
objects with the real RGUI translation unit and replaces only `main()`.

The normal mode requires software playback and checks both CPU thumbnails:

```sh
python3 samples/menu/rgui_media/run.py --c89 movie.mp4
```

On a supported GPU video driver, require the surface fallback with a movie
that has no software decoder, such as AV1:

```sh
python3 samples/menu/rgui_media/run.py --video-driver vulkan --gpu-surface movie-av1.mp4
python3 samples/menu/rgui_media/run.py --video-driver vulkan --gpu-surface --threaded movie-av1.mp4
```

Use `--objdir` to select the frontend build directory, `--shell` for the build
shell, and `--assets` for the menu assets. On Windows the runner uses GDB.
Hardware admission failure is a test failure; it is not a passing device gate.

Surface mode checks rendered forward/backward seeks, replacement requests,
the pointer timeline, pause/Stop, fullscreen/inline transitions, repeated
retirement during pending seeks, and recovery after a missing movie. The
fixture must change between zero and 375 ms and allow a seek to one second.
Set `MEDIA_TEST_CAPTURE_DIR` to an existing directory to save PPM captures.
Captures 0-3 cover fullscreen seeks, 4 is inline, 5 is reopened fullscreen,
and 6 is the browser after retirement. Readback exists only in the harness.

These semantic-action tests do not establish physical keyboard/gamepad/remote
mapping, playback-context ownership, or physical HDR display conformance.
