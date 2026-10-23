# RetroArch Exynos-G2D video driver

The Exynos-G2D video driver for RetroArch uses the Exynos DRM layer for presentation and the Exynos G2D block to scale and blit the emulator framebuffer to the screen. The G2D subsystem is a separate functional block on modern Samsung Exynos SoCs (in particular Exynos4412 and Exynos5250) that accelerates various kind of 2D blit operations. It can fill, copy, scale and blend pixel buffers and therefore provides adequate functionality for RetroArch purposes.

## Reasons to use the driver

Hardware accelerated rendering on devices based on an Exynos SoC is usually restricted to the use of the GPU block, which is either a Mali or PowerVR IP. Both GPU types have the problem that interfacing with them requires a proprietary driver stack, comprised of kernel and userspace code. While the kernel code is open source, the userspace code is only available as a binary blob to the enduser.

If you want to use such a device with an upstream kernel, the GPU block will most likely not work for you. Also the chances of Mali or PowerVR kernel code being accepted upstream is very slim. Still, one might want to ask the question if using the GPU block for such trivial operations (basically scale and blend) is the right approach in the first place.

Since the G2D block is present on all modern Exynos SoCs, the natural way of proceeding would be to use it instead of the GPU block. The G2D is still a dedicated piece of hardware, so all operations are offloaded from the CPU. It should be noted though, that using the G2D instead of the GPU removes the possibility to use GPU shaders to enhance the image quality of your emulator core of choice. If the user relies on these enhancements, then he's advised to continue using the GPU, most likely by using the EGL/GLES video driver.

The driver was written on a Hardkernel ODROID-X2, a developer board powered by an Exynos4412 SoC. The G2D needs the exynos DRM driver of the mainline kernel with its G2D support enabled (`CONFIG_DRM_EXYNOS_G2D`).

## Performance analysis

Some simple benchmarking was done to evaluate the performance of the G2D block. The test run was done with the snes9x-next emulation core and a game title that uses a native resolution of 256x224 pixels. The output screen was configured to a 1280x720 mode. Scaling to the output screen was done by keeping the native aspect ratio. At the time the driver placed this in an output rectangle of 822x720; it now uses the core's aspect ratio (4:3 here, so 960x720).

    total memcpy calls: 18795
    total g2d calls: 18795
    total memcpy time: 8.978532 seconds
    total g2d time: 29.703944 seconds
    average time per memcpy call: 477.708540 microseconds
    average time per g2d call: 1580.417345 microseconds

The average time to display the emulator framebuffer on screen is roughly 2058 microseconds, or around 486 frames per second. Assuming that the time consumption increases linearly with the amount of pixels processed, which is usually a safe assumption, scaling to an output rectangle of size 1920x1080 would yield a average duration of 7207 microseconds, which is still 138 frames per second.

## Configuration

The video driver uses the libdrm API to interface with the DRM. The Exynos API of upstream libdrm is enough: build libdrm with its Exynos support (`-Dexynos=enabled` with meson) and RetroArch with `--enable-exynos`.

The video driver name is 'exynos'. It honors the following video settings:

   - video\_monitor\_index
   - video\_fullscreen\_x and video\_fullscreen\_y, at video\_refresh\_rate
   - video\_vsync: with it off, a frame that comes before the last flip is done is not flipped to, and nothing waits
   - the aspect ratio, from the core or as chosen
   - video\_font\_enable, video\_font\_path and video\_font\_size, with the message position and colour
   - the on-screen statistics
   - CRT SwitchRes modelines and resolution switches

The monitor index maps to the DRM connector index. If it is zero, then it just selects the first 'sane' connector, which means that it is connected to a display device and it provides at least one useable mode. If the value is non-zero, it forces the selection of this connector. For example, on the author's ODROID-X2, the HDMI connector has index 1.

The two fullscreen parameters select the mode the DRM should select. If zero, the native connector mode is selected. If non-zero, the mode of that size closest to the refresh rate is selected; if the connector has none, the native mode is used.

A core that renders into a buffer the frontend provides (`get_current_software_framebuffer`) is given the GEM buffer the G2D reads its frame from, so the frame is never copied. A core that reads its frame back is not given it, as the mapping may be uncached. Screenshots read the page on screen.

Where the G2D sits behind an IOMMU (an `iommu_group` under `/sys/bus/platform/drivers/exynos-drm-g2d/`), it reads a core's frame from the core's own memory (`G2D_IMGBUF_USERPTR`), so the frame is not copied either. The kernel keeps a mapping per address and size and syncs the CPU's caches only when it makes one, so each frame is given its address with a size other than the last, and the kernel maps it afresh. Memory the kernel will not map - read-only, or a frame ending exactly on a page with no room for a second size - is copied. Without an IOMMU the kernel would hand the G2D the address of the first page for the whole frame, so the driver copies.

The input overlay's images each have a GEM buffer of their own, premultiplied, which the G2D scales and blends over the page; an image's alpha is applied to its buffer when it changes, as the G2D's blend takes none. Where an image lies outside the frame, that area is cleared first, so it is not blended over what it left on that page before.

Messages and statistics are drawn into a buffer of their own, and only the area the glyphs cover is cleared and blended.

The G2D calls libdrm offers can neither rotate nor mirror. A core's rotation is applied by the CPU as the frame is copied into a GEM buffer, so a rotated frame is always copied; a mirrored overlay image is written mirrored into its buffer when its layout changes.

## Issues and TODOs

   - Without an IOMMU, a core's frame is copied into a GEM buffer unless the core renders into the one it is lent.
   - A core's views are drawn as one frame while a rotation is set.

The driver is tested against a fake kernel and G2D in `samples/gfx/exynos_g2d`.
