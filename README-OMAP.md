# RetroArch OMAP video driver

The OMAP video driver for RetroArch uses the omapfb (OMAP framebuffer) driver from the Linux kernel. omapfb is not to be confused with omapdrm, which is the corresponding DRM driver.
OMAP framebuffer support is available on platforms like the Pandora (OMAP3) handheld console, the Beagleboard (OMAP3) single-board computer or the Pandaboard (OMAP4), which is also a single-board computer.
The OMAP display hardware provides free scaling to native screen dimensions, using a high-quality polyphase filter.

## DSS setup

The DSS is the underlying layer, which manages the OMAP display hardware. Through DSS we can setup which framebuffer device outputs to which display device. For example there are three framebuffer devices (fb0, fb1 and fb2) on the Pandaboard, each one connected to a 'overlay' device. The DSS controls are exported in '/sys/devices/platform/omapdss'. Here we configure fb1 to connect to our HDMI display connected to the board.

First we disable the overlay we want to use and the two displays:
<tt>echo -n 0 > overlay1/enabled
echo -n 0 > display0/enabled
echo -n 0 > display1/enabled</tt>

Check that 'manager1' (name = tv) is connected to HDMI:
<tt>cat manager1/display:
hdmi</tt>

The free scaling property mentioned above is not available on all overlays. Here 'overlay1' supports zero-cost scaling.

Now we connect 'overlay1' to 'manager1':
<tt>echo -n tv > overlay1/manager</tt>

Last but not least enable the overlay and the HDMI display:
<tt>echo -n 1 > overlay1/enabled
echo -n 1 > display0/enabled</tt>

## Configuration

The video driver name is 'omap'. It honors the following video settings:

   - video_monitor_index (selects the fb device used, index = 1 -> fb0, index = 2 -> fb1, etc.)
   - video_vsync (use to disable vsync, however this is not recommended)
