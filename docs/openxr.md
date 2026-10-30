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
Output (OpenXR)**. Under it come Edit Screens, Snap to Screens, Snap to
Angles, Hinge Gap, Edit Smoothing and While Editing Screens, described
below, then Headset Refresh Rate, Headset Controllers, Laser Pointer,
Laser Beam, Screen Placement (Fixed in Room, Follow Head, Lazy Follow),
Screen Distance and Screen Width. The Recenter Headset Screens, Laser
Pointer (Toggle) and Edit Headset Screens hotkeys are unbound by
default.

**Laser Beam** draws a thin line from each controller to its laser's
dot, turned to face you. It shows only where the dot can: outside edit
mode, Laser Pointer Off, or its hotkey, hides both; while editing, both
hands always have a laser, and the hotkey only switches Laser Pointer
for afterwards. Edit Mode Only (the default) shows it while editing
screens, Always also for the menu and light guns, Off never. Pointing
at nothing, the line is 1.5 m long and fades out.

Without a runtime or a headset, RetroArch says so once and shows its
window as usual.

## Adjusting floating screens

In **Settings > Video > Stereo 3D & Screens**, use **Screen** to choose
which floating screen to adjust. The number is the core's logical screen
ID plus one, from 1 to 8. Only screens in the current frame's map are
listed; a tracked projection's eyes are excluded. Without a map,
including with no content loaded, Screen 1 is listed: a core without a
view map shows its whole frame as Screen 1, which these rows then move
like any other. Changing the order of views, or temporarily removing a
screen, preserves that screen's saved adjustments.

- **Horizontal Offset**, **Vertical Offset** and **Depth Offset** move
  the selected screen relative to its normal placement, from -10 to +10
  metres. Positive values move it right, up and towards you respectively,
  in the shared placement anchor's coordinates.
- **Yaw**, **Pitch** and **Roll** rotate it from -180 to +180 degrees.
  **Scale** changes its width and height together, from 0.1 to 10 times
  its normal size.
- **Visible** hides or shows it. The menu remains available even when
  every floating screen is hidden or moved out of sight.
- **Reset Screen** restores the selected screen's zero offsets, zero
  rotations, unit scale and visibility. **Reset All Screens** does this
  for all eight screen IDs, including those absent from the current map.
  Reset All Screens remains available when there is no floating screen
  to select.

These adjustments apply to floating screens, including the two eyes of
a stereoscopic screen together. They do not change a tracked core's eye
poses or fields of view. Changes also take effect while content is paused.
The shared Screen Distance, Screen Width, Screen Placement and layout
settings still determine the base placement. **Recenter Headset Screens**
updates the shared anchor for all screens while preserving their relative
offsets, rotations, scales and visibility; it does not reset them.

The menu opens at its own spot, straight ahead of the anchor. With
**Menu in Front of Screen 1** on, it floats 10 cm in front of Screen 1
and faces the way it does, so moving Screen 1 moves the menu with it. It
looks as large as it does at its own spot, and comes no nearer to you
than 40 cm. While Screen 1 is hidden, is a tracked core's projection or
faces away from you, the menu stays at its own spot. A menu placed in
edit mode (below) stays where it was put until it is reset there, or by
Reset All Screens.

Screen adjustments are configuration settings. Save the configuration to
keep them as defaults, or use **Quick Menu > Overrides** to save core,
content-directory or game overrides. Loading an override applies its
screen values; unloading it restores the inherited values. The selector
itself is only a menu choice and is not saved. Configuration keys use
`video_screenN_xr_x`, `_xr_y`, `_xr_z`, `_xr_yaw`, `_xr_pitch`, `_xr_roll`,
`_xr_scale` and `_xr_visible`, with `N` from 1 to 8. Finite values outside
the supported ranges are clamped; non-finite values use the defaults.

With the legacy **gl** driver on desktop or Quest, opening the menu shows
the whole window on an unadjusted floating screen. Per-screen adjustments
resume when the menu closes. This keeps the menu and resets reachable
without applying the primary content screen's hidden state or transform
to the menu. The desktop **glcore** and **vulkan** paths use a separate
menu screen.

## Editing screens in the headset

**Edit Screens** moves the floating screens and the menu with the
controllers instead of the menu's rows. Start it from **Settings > Video >
Stereo 3D & Screens > Edit Screens** (shown with Headset Output on; the
menu closes into it), the **Edit Headset Screens** hotkey, the headset's
own **Edit Screens** action, or the `HEADSET_EDIT_SCREENS` network
command. It needs content running. The same hotkey, action or command
stops it, as do opening the menu, loading or closing content, and the
end of the headset session. The hotkey and the action are unbound by
default; bind the action in the runtime's controller settings, in both
RetroArch's game sets and its Edit Screens set. If the runtime refuses
the Edit Screens set's bindings for a hand's controller, Edit Screens
says it is unavailable. It also stops and says so when the runtime
binds neither that set's Grab nor its RetroArch Menu, as a controller
binding saved in the runtime before RetroArch had them can.

While editing, every item has a faint outline just outside it, a light
line between two dark ones so it shows against any border. It is bright
on what a hand holds, and on what a laser points at unless an item is
chosen (below). Both hands have a laser whatever **Laser Pointer** is
set to, with a beam unless **Laser Beam** is Off. The menu also shows a
handle bar under it in the same light and dark, faint, and bright when
the menu's outline is. While a held screen is hinged (see **Snap to
Screens**), its edge and the edge it hinges on lose their outline, and a
6 mm green line runs along the hinge where the two overlap, midway
between the edges and drawn a centimetre nearer the eye so it shows
over both screens, while a let-go would settle the screen into the
hinge and for 0.15 s after.
Two screens no hand holds that rest exactly where a hinge holds one of
them, so that taking it up would leave it there, show the same: any
screen let go where a hinge holds it, or set out so. Screens only near
a hinge show nothing until hinged once. A screen hinged at rest on two
edges at a corner keeps one of those two rims.
The headset's controllers only edit;
keyboards and other gamepads still reach the content. The desktop
window shows a notice when editing starts, with the controls, for
about 8 seconds, and a short one when it stops. **Select
Next** and **Select Previous** choose an item, below.

| Controller | Grab | Tilt | Distance and size | Turn and tilt (other hand) | Reset | Menu | Select Next / Previous |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Steam Frame | grip | trigger | stick | stick | A; left hand: D-pad down | right stick click | left D-pad right / left |
| Valve Index | grip | trigger | stick | stick | A | left trackpad press | right B / left B |
| Oculus Touch | grip | trigger | stick | stick | X / A | menu button | B / Y |
| HTC Vive | grip | trigger | trackpad | trackpad | trackpad click | left menu button | right menu button / - |
| Simple controller | select | - | - | - | - | left menu button | - |

- A grip on an item grabs it: it keeps its place relative to the hand
  and follows it. Each hand holds one item; the other hand on the same
  item does nothing.
- Items keep the pitch and roll they had when grabbed, and their angle
  to the way the hand points across the floor: they move with the hand
  and turn left and right as it does, but raising, lowering or
  twisting the hand does not tip, roll or swivel them. Pointing nearly
  straight up or down, a twist of the hand turns them instead, and
  tipping it 5 degrees past straight up or down turns them round to
  face the other way. Hold the trigger while grabbing to let them ride
  the hand, pitch and roll too; let go of the trigger to keep them.
- The other hand's stick, while that hand holds nothing, turns and
  tilts the held item about its middle, whether or not the trigger is
  held, faster the further the stick is pushed, with no stop. Right
  turns it about the vertical, keeping its pitch and roll,
  anticlockwise as seen from above, so a screen facing you turns
  towards your right; left the other way. Up tips its top back about
  its own left-right line, as when pushing a monitor's top back; down
  the other way. Within about 25 degrees of left or right the stick
  only turns, and within about 25 degrees of up or down it only tilts;
  pushed clearly diagonally, it does both at once. As the stick turns
  and tilts it, the item clicks into steps of Snap to Angles, counted
  from facing you and from upright: it holds within about 2 degrees of
  each and the holding hand buzzes once; pushing on leaves the step.
  Carrying it or tilting it with the trigger never clicks. Snapping
  still catches it parallel or square, and wins over a step.
- On the holding hand's stick, up pushes the item away along the laser
  and down pulls it closer; right makes it larger and left smaller,
  faster the further the stick is pushed.
- Offsets stop at +-10 m and scale at 0.1 to 10, as in the menu: the
  item waits at the limit and moves on from there once the hand comes
  back.
- Reset on a screen does what Reset Screen does; on the menu it makes
  the menu follow Screen 1 again.
- The menu is grabbed by its handle bar, even where a screen is nearer,
  and within about 3 cm of the bar where the laser meets no other item;
  elsewhere on the menu the grip takes the screen behind. Once a laser
  is on the bar, the bar stays its target while the laser stays within
  those 3 cm, even over a screen; a laser that comes into the 3 cm from
  a screen keeps the screen. Grabbing the menu places it where it
  shows, even without moving it, so it no longer follows Screen 1. The
  legacy **gl** driver draws the menu in its window, so only screens
  can be grabbed there.
- Select Next and Select Previous choose an item: they step from
  Laser through each screen that can be moved, in number order (a 3D
  screen is one), then the menu while it is shown, and back to Laser.
  Laser is the usual way: grip and Reset act on what a laser points at.
  With an item chosen, its outline is bright and pointing lights
  nothing else; either hand's grip takes it from wherever the
  hand is, as if on a stick, and Reset resets it. While one hand holds
  it, the other points, grabs and resets as usual. The choice stays
  after letting go, until stepped back to Laser or edit mode ends; a
  chosen screen that is hidden or goes away goes back to Laser. This
  takes a screen hidden behind another.
- A faint tag just above the hand that last pressed Select Next or
  Select Previous (the left hand at first) names the chosen item:
  Laser, Screen 1, Screen 2 and so on, or Menu.
- Seen from behind, a screen or the menu shows a plain dark back.
  Edit mode grabs and outlines it from there as from the front.
- A head-tracked core's projection cannot be grabbed, nor can the whole
  frame a views core falls back to when its screens cannot be placed. In
  Follow Head a grabbed screen keeps its place in view; Lazy Follow does
  not glide while anything is held. Recenter lets go of what is held
  first. A hand that loses tracking leaves its item still.

**While Editing Screens** chooses whether the content keeps running (the
default) or pauses; leaving edit mode resumes only content it paused.

**Snap to Screens** (on by default) lines a held screen up with the
other screens within about 30 cm of it that no hand holds. Within 5
degrees of facing the same way as one, or of a quarter turn from it
about its vertical, the held screen turns exactly so; of two, it turns
to the one whose edge is nearer. Then, within 3 cm, it moves into the
other's plane, and its left, right, top, bottom or centre lines up with
the other's, level; edge to edge, or at a corner for a screen at a
quarter turn, it keeps the gap described under Hinge Gap. Or, at any
angle between them, an edge of the held screen hinges on an edge of the
other where the two overlap along it, top, bottom or side, within 5
degrees of parallel and 3 cm of its plane, up to 10 cm out from the
edge: it turns exactly parallel to that edge and moves into the other's
plane, keeping its gap from that edge, centres or ends level along it,
keeping its angle, like the two halves of a DS. The green line
shows wherever a let-go would settle the screen into that hinge, and
0.15 s longer once it leaves, so a shaking hand never flickers it; it
stays when the screen is let go there. A screen taken up where it rests
hinged starts hinged, with no buzz, until it is carried out of the
hinge it rests in, another hinge takes hold or the other hand takes
the screen it hinges on. At a fold or a square corner with a gap
only the screen carried in does: the other one's edge is off its
plane, so it is taken up free. Screens that would overlap do not
hinge. While a screen is hinged, the other hand's stick swings it
about the shared edge, the tilt for a top or bottom edge and the
turn for a side one, and it stays where the swing leaves it. The
swing lets go just past square, but that axis keeps pivoting it on the
same edge until the edge is carried out of the hinge's reach or the
other hand takes the screen it hinges on; the stick's other axis turns
or tilts it as before and can pull it off. Past
each 3 cm range and the 1 cm click the pull fades out over as far again,
a hinge fades out from 10 to 13 cm, and from 30 to 60 cm away every pull
fades, so the screen catches up with the hand instead of jumping. A
screen just taken does not move: snapping fades in as it is carried,
whole once it is 5 cm or 10 degrees from where it was taken, but a
hinge it rests in, where holding it moves nothing, holds at once. The
holding hand gets a short buzz as a snap takes hold, and none again from
that snap until the screen has left its pull. A hinge taking hold gets a
longer, stronger buzz instead, and short buzzes due while it plays are
skipped; it comes again only once the hinge has let go, 0.15 s after
the screen leaves where it would settle, or for another edge, so a
swing past square and back, or a wobble where the hinge takes hold,
does not repeat it. The menu neither snaps nor is
snapped to. Let go, it stays where, turned and as large as it was a
moment (0.08 s) before the grip opened, so the hand's twitch as it
opens does not move it. With Edit Smoothing on, which damps that
twitch, it stays where, turned and as large as it shows, unless it
settles into a hinge. That is where it is saved.

**Snap to Angles** (15 degrees by default) is the step the other hand's
stick turns and tilts a held item in: 15, 30, 45 or 90 degrees. A
hinged screen's steps count from flat with the other screen, both ways.
Off turns and tilts it smoothly, with no steps and no buzz.

**Hinge Gap** (2 cm by default) is the gap Snap to Screens leaves
between screens that meet edge to edge, side by side, at a corner or
hinged at an angle: 0, 1, 2, 3, 5 or 10 cm. A screen within 1 cm of it
clicks to it exactly, with a short buzz; let go within 3 cm of it, or
within 6 cm of level along the edge, a hinged screen settles there
exactly; anywhere else from touching to 10 cm the gap stays where the
hand puts it, still hinged, and from 10 to 13 cm the hinge lets go. At
0 the edges touch. It does nothing while Snap to Screens is off;
screens already placed do not move. Pushed past touching, a hinged
screen stops there, so two screens can be pushed together with no gap
and let go touching, with one short buzz per push (below a 3 cm gap,
the click's). From 3 cm past (3.5 cm at a 1 cm gap) the hinge lets go
and the screen slides on into the other, free from 5.5 cm (6 cm);
pulled back, it hinges again 5 mm inside that 3 cm. With Snap to
Screens off nothing stops them, and screens overlap freely.

**Edit Smoothing** (Medium by default) steadies the controllers' aim
while editing, so a screen held far out on the laser does not shake
with the hand: a slow or still hand is smoothed a lot and a fast one
little, so quick moves keep up. Low, Medium and High smooth more in
turn; Off uses the aim as tracked. Everything edit mode does with the
lasers follows the smoothed aim: the dots and beams, what lights, what
a grip takes, the stick and what is held; a screen is saved where it
was shown. The lasers are never smoothed outside edit mode.

Edits change the same settings as the menu's rows and are saved the
same way: on quit while **Save Configuration on Quit** is on (the
default), or by saving the configuration or an override.
The menu's own place is saved as `video_openxr_menu_xr_x`, `_xr_y`,
`_xr_z`, `_xr_yaw`, `_xr_pitch`, `_xr_roll`, `_xr_scale` and
`video_openxr_menu_placed`; while that is false the menu follows Screen 1.

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
- The `edit-*` cases drive edit mode through the test layer's scripted
  aims (`aim ... [<roll>]`) and `edit/*` actions on Vulkan, glcore and
  gl.
