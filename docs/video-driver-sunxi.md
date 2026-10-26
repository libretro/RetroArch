# RetroArch sunxi video driver

The sunxi video driver puts the emulator framebuffer on screen through the display engine of Allwinner SoCs, as the legacy linux-sunxi kernel exposes it: `/dev/disp` for the display engine's layers and `/dev/fb0` for the framebuffer memory. The frame is written into the framebuffer, past the console, and a layer of the display engine scales it to the screen with its hardware scaler as it scans out. Nothing goes through the Mali GPU.

## Reasons to use the driver

On an Allwinner A10, A13, A20 or similar board running the linux-sunxi 3.4 kernel, the driver gets a software-rendered core on screen with no GPU driver at all: the core's pixels are converted to XRGB8888 by a NEON blit (or not at all, see below) and scaled by the display engine for free. It has no shaders and no XMB/Ozone; for those, use a GL driver where the Mali blob is available.

## Requirements

The legacy linux-sunxi kernel's display driver (`/dev/disp`, the `DISP_CMD_LAYER_*` ioctls) and its framebuffer. Mainline kernels drive the same hardware through DRM instead; use the `drm` driver there. The framebuffer must have room past the console for two pages of the frame: the size of `/dev/fb0`'s memory follows the `fb0_framebuffer_num` setting in the `[disp_init]` section of the board's script.bin (FEX); three screens' worth is plenty.

Build RetroArch with `--enable-sunxi`. The driver is ARM-only: its pixel conversion is pixman's NEON assembly.

## Configuration

The video driver name is 'sunxi'. It honors the following video settings:

   - the aspect ratio, from the core or as chosen, `video_force_aspect`
   - `video_scale_integer`, with the scaling axis and overscale settings the frontend has
   - the custom viewport; the part of it that falls off the screen is cut from the source
   - `video_rotation` and the core's rotation (`RETRO_ENVIRONMENT_SET_ROTATION`)
   - `video_vsync`
   - `video_monitor_index`: 2 selects `/dev/fb1` (the second display), anything else `/dev/fb0`
   - `video_font_enable`, `video_font_path` and `video_font_size`, with the message position and colour
   - the on-screen statistics
   - a core's views, composed on the CPU and scaled over the whole screen; under a rotation each view is turned in its own place

The display engine's scaler filters on its own; `video_smooth` applies only to the CPU composition of a core's views.

### Aspect ratio, integer scale and the viewport

The frontend lays the frame out exactly as it does for every other driver, and the driver gives the layer that rectangle as its screen window. The rest of the screen is the blanked console. The window is set only when it moves.

### Rotation

The display engine's layer can neither turn nor mirror, so a rotated frame is turned by the CPU as it goes into the page. An XRGB8888 frame is turned straight into the page; a 16-bit frame is turned aside first and converted by the NEON blit, so a rotation costs one extra pass for 16-bit cores.

### vsync

A thread waits for each vsync and reports the flip that landed. With vsync on, a frame is drawn only into the page not on the screen, so a core running slower than the display never draws into what is shown. With vsync off, a frame that comes while a flip is still in flight is dropped, and nothing waits. A framebuffer that cannot wait for a vsync (`FBIO_WAITFORVSYNC` failing) ends the thread, and frames are flipped at once from then on.

### Messages and statistics

On-screen messages and the statistics are blended into the page after the frame, at the size they show on the screen. Notifications drawn as widgets need a GPU display driver; without one, notifications come as these messages.

### The menu

RGUI is converted to XRGB8888 at the end of the framebuffer, past the pages, and shown in the frame's place - where the frame shows, or over the whole screen when the menu asks for that - until the menu closes. The display engine has one scaled layer for the driver, so the menu is not blended over the frame. It can be shown before any core is loaded.

### Screenshots

Screenshots read what is on the screen - the menu while it is up, else the page last flipped to - and scale it to the viewport's size.

## How a frame reaches the screen

The framebuffer memory past the console holds two pages, each a frame's worth of XRGB8888 rows, as wide as the screen when there is room for that and as narrow as the frame otherwise. A frame too large for two pages in the memory there is is not drawn, with one error in the log.

   - A 16-bit or XRGB8888 frame from the core's own buffer is converted (or copied) into the page by a NEON blit: one pass on the CPU.
   - An XRGB8888 core that asks for a framebuffer to render into (`RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER`) is given the page to be shown next, so its frame is never copied: the layer scans it out where the core drew it. The framebuffer is not read at cached speed, so a core that reads its frame back is not given it; nor is one while a rotation is set or views are drawn. A 16-bit core has to be converted, so it is not given one.

The page handed over for a flip is the one not on the screen, and is written only after the flip before it has landed.

## Issues and TODOs

   - The input overlay is not drawn. The display engine has more layers than the driver uses, but blending one over the scaled layer needs the pipes and priorities the legacy API sets per board.
   - Messages and statistics are not drawn while the menu is up.

The driver is tested against a fake display engine and framebuffer in `samples/gfx/sunxi_display`, Linux only, as the driver is built against `linux/fb.h`.
