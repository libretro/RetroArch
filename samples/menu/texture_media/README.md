# Fullscreen media rendering checks

The existing `Linux-menu-media` workflow exercises all four menu adapters with
H.264/AAC, VP8/Opus and VP9/Vorbis fixtures, with threads enabled and disabled.
It uses the null video driver for the transport and menu checks.

After building the shipping frontend, select a real renderer to additionally
check the presented movie pixels:

```sh
python3 samples/menu/rgui_media/run.py --driver ozone --video-driver vulkan \
  --assets /path/to/assets /path/to/movie.mp4
```

Repeat with `--driver xmb`, `--driver materialui` and `--driver rgui`, and with
`--threaded` for the threaded video wrapper. RGUI uses its CPU media player;
choose a fixture that its decoder supports. Texture menus retain their actual
GPU texture upload and presentation paths. The native mode requires a working
window system and viewport readback on the selected renderer.

Use a fixture whose pictures at 0 and 375 ms differ. The check pauses playback,
seeks forward and back, and requires the central movie image to return to the
same hash. It also submits repeated alternating seeks and checks that the final
requested picture is presented while paused. The existing mouse timeline,
Stop, Back and reopen checks still run. This exercises menu actions and pointer
callbacks; physical keyboard/gamepad acceptance is a separate gate.

Set `MEDIA_TEST_CAPTURE_DIR` to an existing directory to save PPM captures.
Set `MEDIA_TEST_HDR_AV1=1` only with a 10-bit AV1 fixture carrying PQ transfer
and static HDR metadata (MaxCLL 1000, MaxFALL 400). This additionally requires
P010 hardware imports, matching metadata, and an active player worker. It does
not measure physical HDR output or prove RGUI AV1 support.

Set `MEDIA_TEST_HDR_TIMELINE=1` with a PQ fixture whose CLL changes from
1000/400 to 2000/600 before 375 ms. Native captures also check imported
metadata after forward, backward and burst seeks.

Readbacks and bounded polling belong to this test harness only.
