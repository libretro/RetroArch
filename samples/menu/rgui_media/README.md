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

On a native hardware-decoding device, add `--threaded --gpu-surface
--first-import-refusal` to exercise software recovery before the first picture.
The test links a wrapper around surface submission that preserves the decoder's
real lease but asks the renderer for an unsupported output format. It requires
an asynchronous import and a recovered software surface, then runs the usual
seek, close and reopen checks. This requires an H.264 fixture with software
decoding support; a WARP-only device cannot establish the native-decoder gate.

`--threaded --gpu-surface --hardware-decode --cold-cancel` holds the first native
constructor on its worker, closes the viewer and immediately opens it again.
It requires the replacement to reach native playback. The bounded hold exists
only in the linked test wrapper, with no production scheduling hook.

Use `--cold-reset` instead of `--cold-cancel` to reinitialize only video while
admission is held. The test requires worker completion before device teardown
and native playback after the renderer is recreated. A failing baseline is
drained by the test wrapper after recording the unsafe ordering, so cleanup
does not deliberately free a device still used by its constructor.

The tested Windows NVIDIA driver fails the opt-in
[H.264 host-header lifetime probe](../../gfx/vulkan_video_caps/README.md#h264-external-and-host-lifetime-probes).
Default validation handle wrapping can expose the same delayed host-header
read during submission. Keep that failed conformance result separate from
normal playback and color checks; this runner does not disable validation.

`--desktop-refresh` exercises measured fullscreen cadence, folder transitions,
settings entry and exit, and unchanged core timing. `--refresh-pixels` also
requires three changing movie-region captures, spaced 250 ms apart, on a
native video driver. A static fixture must fail this pixel check. Captures
3000-3002 exclude controls and clocks. These options do not prove a physical
refresh change when the current mode already matches or the display refuses it.

For a native Windows display-switch gate, set `MEDIA_TEST_REFRESH_RATE` to
the expected advertised integer rate. The fixture checks it after cadence is
published and when the next episode opens. Record the original named output
mode before running and verify restoration afterward. Do not count an OS mode
refusal as a physical-switch pass. Fractional X11 timing has separate live
RandR tests; this Windows assertion uses the nominal EnumDisplaySettings rate.
