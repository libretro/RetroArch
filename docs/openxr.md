# OpenXR headset output

RetroArch can show content in a virtual reality headset through OpenXR:
each of a core's screens as a screen floating in the room, a stereoscopic
core's left and right images one to each eye, and a head-tracked core's
two eyes as the headset's own view. The menu shows as a screen of its
own, and the headset's controllers work as a gamepad and as a laser
pointer.

It runs on desktop Linux with the `vulkan`, `glcore` and `gl` video
drivers, and on the Meta Quest with `vulkan` and `gl`.

## Building and enabling

- Linux: `./configure --enable-openxr`. OpenXR is off by default. It
  needs Vulkan or OpenGL, and threads. The Khronos loader is built
  in-tree, so nothing has to be installed to build.
- Quest: the `quest` gradle flavour.

At run time an OpenXR runtime has to be active (SteamVR or Monado on
Linux). Then turn on **Settings > Video > Stereo 3D & Screens > Headset
Output (OpenXR)**. The other rows there are Headset Refresh Rate, Screen
Distance, Screen Width, Screen Placement (Fixed in Room, Follow Head,
Lazy Follow), Headset Controllers and Laser Pointer. The Recenter
Headset Screens and Laser Pointer (Toggle) hotkeys are unbound by
default.

Without a runtime or a headset, RetroArch says so once and shows its
window as usual.

## What a core sees

A core describes its frame with `RETRO_ENVIRONMENT_SET_VIDEO_VIEWS`: a
map of rectangles, each a screen or one eye's image of a screen. That
is all a core with two screens or a stereoscopic picture needs, with or
without a headset.

A core that draws from the head's position asks
`RETRO_ENVIRONMENT_GET_VIDEO_VIEWS_STATUS` whether a tracking headset
is there (`RETRO_VIDEO_VIEWS_STATUS_HMD`), takes each frame's eye poses
from `RETRO_ENVIRONMENT_GET_VR_FRAME_STATE`, and marks its map with
`RETRO_VIDEO_VIEWS_FLAG_TRACKED`. Its two eyes then fill the headset's
view. `libretro.h` documents the calls.

A core that follows the hands asks
`RETRO_ENVIRONMENT_GET_VR_CONTROLLER_POSES` for each controller's grip,
aim and palm poses as well as the hands joints.

## How it is built

Everything that does not depend on a graphics API or a platform is
shared. A headset path adds only what differs.

Shared, in `gfx/common/` unless noted:

| Unit | What it does |
| --- | --- |
| `openxr_runtime` | The loader, instance, system and extensions. A platform whose loader has to be started first (Android) passes what it needs through `openxr_runtime_platform_t`. |
| `openxr_session` | The session's state machine and spaces, and what happens when the runtime ends or loses it. |
| `openxr_swapchain` | One set of swapchains for each screen (a slot), with acquire and release. |
| `openxr_frame` | The frame loop: waiting for the headset, pacing the core to it, the refresh rate, where the screens are, recentering, and the layers each frame ends with. It runs on a thread of its own, or inside the video frame for a context that shows only the headset. |
| `openxr_host`, `openxr_tracking` | What a core is told about the head and the eyes, in the space it asked for, and what is kept across a video driver restart. |
| `gfx/video_xr` | The geometry: where each screen goes and how a view maps onto it. |
| `input/common/input_openxr` | The controllers as joypads, their haptics, and the laser pointer. |

A *host* joins those units to one graphics API. There are three:

| Host | Used by | How images reach the headset |
| --- | --- | --- |
| `gfx/common/vulkan_openxr.c` | `vulkan` on Linux, and the Quest's Vulkan context | The video driver draws into the swapchain images. On Linux the frame loop has its own thread; on the Quest it runs inside the video frame. |
| `gfx/common/gl_openxr.c` | `glcore` and `gl` on Linux | The frame loop's thread holds a GL context in the window's share group. The video thread draws into shared textures and the frame loop copies them in. |
| `gfx/drivers_context/gl_android_openxr_ctx.c` | `gl` on the Quest | The context is the headset: `gl` draws its window, and the context shows it and a tracked map's eyes inside the video frame. |

Each host registers a `video_xr_host_t` (`gfx/video_driver.h`) with the
video driver layer. That is how the frontend asks for the headset's
state and answers a core's pose calls without knowing which host is
running.

## Adding a headset

A headset on a platform that already has a host needs no video code,
as long as its runtime offers Vulkan or OpenGL. This covers another PC
headset through SteamVR or Monado, and another Android standalone
headset. It may need:

- A controller profile: one entry in `input_openxr_profiles[]`
  (`input/common/input_openxr.c`) that maps its interaction profile's
  paths. Without one the runtime remaps a profile that is already there
  (Index, Touch, Vive, Steam Frame, or the simple controller), which
  can lose buttons.
- On Android, its loader and instance set-up through
  `openxr_runtime_platform_t`, and a build flavour like `quest`.

## Adding a graphics API or a platform

A new graphics API or platform, such as Direct3D on Windows, needs a
host. Model it on the three above:

1. A graphics binding: the structure the session is created with, and
   a callback that lists a swapchain's images. `openxr_binding_gl.c`
   and `openxr_binding_gles.c` are the small examples.
2. The host. It owns the shared units' state, starts the frame loop on
   a thread (`openxr_frame_start`) or in-line
   (`openxr_frame_start_inline`), and registers a `video_xr_host_t`
   whose calls forward to `openxr_host_*`.
3. In the video driver: draw each screen, or each eye, into its slot's
   image, then publish the quads (`openxr_frame_publish`) or the
   projection (`openxr_frame_publish_projection`). Wait on the headset
   with `openxr_frame_pace_wait` while it paces the core.
4. The driver's name in `video_driver_has_headset_path()`, so the menu
   lists the headset's settings for it.
5. The build: the objects in `Makefile.common` and `griffin/griffin.c`,
   and the platform in `qb/config.libs.sh`, which today allows OpenXR
   on Linux only.

## Testing

- `samples/openxr/core` runs the shared units and the hosts against a
  stand-in runtime, with no headset and no GPU. A new host can be
  tested there against a fake graphics API, as the GL host is.
- `samples/input/openxr_input` does the same for the controllers.
- `samples/openxr/e2e/run.py` runs RetroArch against Monado's simulated
  headset in headless gamescope, through a test API layer
  (`samples/openxr/test_layer`) that records every frame's layers and
  images. It needs a GPU, so it is not run in CI.
