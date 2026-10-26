# RetroArch dispmanx video driver

The dispmanx video driver puts the emulator framebuffer on screen through DispmanX, the display API of the Raspberry Pi's VideoCore firmware. Each frame is handed to the firmware as an image resource, and the HVS (the Pi's hardware video scaler) scales it to the screen as it scans the display out. Scaling, flipping and blending cost the ARM nothing, and nothing goes through the GPU's 3D pipeline.

## Reasons to use the driver

On a Pi Zero, 1, 2 or 3, or a Compute Module of those generations, the driver gets a software-rendered core on screen with the least CPU and memory bandwidth there is: the frame goes from the core's own buffer to the VideoCore, and everything after that is done by the display hardware. It has no shaders and no XMB/Ozone; for those, use the gl driver.

## Requirements

DispmanX belongs to the legacy firmware graphics stack. It needs the VideoCore userland (`bcm_host.h`, `libbcm_host`, `libvchiq_arm`, `libvcos`), traditionally installed in `/opt/vc`, and a system that has not handed the display to the kernel: no `dtoverlay=vc4-kms-v3d` in `config.txt`. Under full KMS the firmware no longer owns the display; use the `drm` driver there. Current Raspberry Pi OS releases ship without `/opt/vc` and drive the display through KMS, so the driver is for the older stack.

Build RetroArch with `--enable-dispmanx`. Configure turns the driver off with a notice when the VideoCore headers are not found.

If the console framebuffer has overscan set, put `overscan_scale=1` in `config.txt`, so the console and DispmanX see the same screen size.

## Configuration

The video driver name is 'dispmanx'. It honors the following video settings:

   - the aspect ratio, from the core or as chosen, `video_force_aspect`
   - `video_scale_integer`, with the scaling axis and overscale settings the frontend has
   - the custom viewport; the part of it that falls off the screen is cut from the source
   - `video_smooth`, at start and when changed in the menu
   - `video_rotation` and the core's rotation (`RETRO_ENVIRONMENT_SET_ROTATION`)
   - `video_vsync`
   - `video_monitor_index`
   - the input overlay, its opacity and full-screen settings
   - `video_font_enable`, `video_font_path` and `video_font_size`, with the message position and colour
   - the on-screen statistics
   - `video_max_swapchain_images` (2 to 4 pages for the frame)
   - a core's views, composed on the CPU at the size that draws none larger than it is and scaled by the HVS over the whole screen; under a rotation each view is turned in its own place, as the frontend lays them out

### Aspect ratio, integer scale and the viewport

The frontend lays the frame out exactly as it does for every other driver, and the driver places the frame's DispmanX element on that rectangle. The rest of the screen is black: a full-screen black layer sits under the frame and hides the console. A change of aspect, scale or viewport moves the element in the same update as the next page flip, so it shows on the next vsync and nothing is made again.

### Filtering

The HVS scales with a polyphase filter whose kernel is global to the display. With `video_smooth` off, the driver loads a nearest-neighbour kernel (`vc_gencmd_send("scaling_kernel ...")`), at start and whenever the setting is turned off in the menu; turned on, it loads the smooth kernel. With `video_smooth` on from the start, the driver leaves the firmware's kernel alone, so a `scaling_kernel` set in `config.txt` stands. The kernel outlives the process, so a driver that loaded the nearest kernel puts the smooth one back on exit.

Nearest neighbour gives even pixels only at whole multiples: use it together with `video_scale_integer`.

### Rotation

The HVS can flip an image but cannot turn it a quarter. A half turn is done by the HVS (`DISPMANX_ROTATE_180`) at no cost. For a quarter turn the CPU turns the frame a quarter counter-clockwise as it lays it out for the firmware, and three quarters is that turn with the HVS's half on top, so only one CPU turn exists. A turned frame is therefore copied once on the CPU.

### vsync

With vsync on, a frame waits for the flip before it to be done, as DispmanX takes one flip in flight. With vsync off (fast-forward included), a frame that comes while a flip is still pending is not shown, and nothing waits: the core runs as fast as it can and the screen shows a frame every vsync.

### The menu

RGUI is drawn on a DispmanX layer of its own above the frame, blended by its own alpha as the other drivers blend it, where the frame shows or over the whole screen when the menu asks for that. It is uploaded only when RGUI redraws. XMB, Ozone and MaterialUI draw through a GPU display driver, which this driver does not have; RGUI is the menu to use with it.

### Display

`video_monitor_index` picks the firmware display the driver opens: 0 the firmware's default, 1 HDMI 0, 2 HDMI 1, 3 the LCD (DSI), 4 composite. A display that is not connected falls back to the default, with a warning in the log. The screen is used at the mode the firmware set up.

### Input overlay

Each overlay image is a DispmanX element of its own, above the menu, over a resource that holds the image as loaded. The HVS crops it to its texture coordinates, scales it, mirrors it and blends it by its own alpha times the image's opacity. Moving, fading, mirroring or switching the overlay off changes the elements in the same update as the next page flip; nothing is drawn or uploaded by the CPU after the overlay is loaded. An image that is switched off, fully transparent or wholly off the screen has no element, so the HVS spends nothing on it.

### Messages and statistics

On-screen messages and the statistics are each drawn on a DispmanX layer of their own, at the top, at the screen's resolution rather than the frame's. A layer is as large as its text, drawn and uploaded only when the text, its place or its colour changes, into the one of its two resources not on the screen; the next page flip shows it. Unchanged text costs nothing per frame. Notifications drawn as widgets need a GPU display driver; without one, notifications come as these messages.

### Screenshots

Screenshots take the screen as the HVS composes it - frame, menu, overlay and text - with `vc_dispmanx_snapshot`, and cut it to the frame's viewport. Off the screen is black. Recording takes the core's frames.

## How a frame reaches the screen

The firmware copies a resource's rows from the frame in one bulk transfer (`vc_dispmanx_resource_write_data`): the VideoCore reads the frame from the ARM's memory itself, and the call returns once it has. Its rows must be the resource's: the firmware keeps a resource's rows 16 pixels aligned and reads the frame's rows that far apart.

   - A frame whose rows are already that far apart goes to the firmware from where the core put it, with no copy on the ARM. A core whose rows carry padding sends the padding with them.
   - A core that asks for a framebuffer to render into (`RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER`) is given one with the firmware's row pitch, so its frame goes as it is, whatever its width, without the padding of the core's own buffer. It is ordinary cached memory, so a core that reads its frame back is given it too. Not while a quarter turn is set, nor while views draw.
   - Under threaded video, the wrapper's frame buffers come from the driver with the firmware's row pitch: the copy the wrapper makes anyway lays the rows out, and the frame goes from there.
   - Any other frame - an odd width in the core's own buffer, or a quarter turn - is laid out once by the CPU into a buffer the surface keeps, and goes from there.

The bulk transfer is the one copy the public DispmanX API leaves: it has no resource the ARM can write in place (`vc_dispmanx_resource_write_data_handle`, which reads VideoCore memory, exists only for code running on the VideoCore).

Three pages (`video_max_swapchain_images`) let the core fill one while one is on screen and one waits for the vsync. The vsync callback frees the page that was on screen; the frame thread waits on a lock-free eventcount when no page is free.

## Issues and TODOs

   - The display mode is the one the firmware set up, or the CRT modeline the videocore display server switched to (which remakes the driver at the new size); `video_fullscreen_x`/`_y` and the refresh rate do not pick another.

The HVS composes every element anew each scanline, and a display list that does not fit its budget underflows: an overlay with dozens of images visible at once costs more of it than one with a few.

The driver is tested against a fake VideoCore in `samples/gfx/dispmanx_display`, which keeps each resource at its own row pitch as the firmware does, so a frame laid out wrongly shows up skewed. It builds for Linux and, cross-compiled with mingw-w64, for Windows.
