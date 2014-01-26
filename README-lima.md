# RetroArch Lima video driver

The Lima video driver for RetroArch uses the open-source Lima driver, which implements the userspace code to enable the Mali GPU contained in a lot of ARM SoC. At the time of writing (24/01/2014) the Lima driver supports GPUs of the type Mali-200 and Mali-400. The full driver stack to enable the Mali GPU is comprised of a part in kernelspace, which is available as open-source from ARM itself, and the aforementioned userspace part, which ARM only supplies as a binary blob.

## Reasons to use the driver

The original binary blob provides hardware-accelerated GLES 2.0 rendering through EGL. Depending on which blob one uses, rendering is either done to a framebuffer provided by a fbdev device or a framebuffer provided by a X11 window. None of these choices are particular good and are also not very performant.

The author uses a Hardkernel ODROID-X2, which is an developer board powered by an Exynos4412 SoC. This SoC incorporates a Mali-400 GPU and dedicated blocks for 2D acceleration and HDMI interfacing. The non-Mali graphics blocks functionality of the SoC is exported through a DRM driver (Exynos DRM).

The DRM exposes a fbdev device through an emulation layer. The layer introduces overhead and also doesn't provide any decent support for proper vertical synchronisation, ruining the experience with lots of tearing artifacts. Switching back and forth between fbdev and X11 solved neither the vsync nor the performance issue.

Users with similar experiences on a Exynos4-based hardware (coupled with a Mali GPU supported by the Lima driver) are invited to try this driver.

## Configuration

The original Lima driver suffers from a similar problem as the blob, since it can only render into a framebuffer provided by a fbdev device. In the repository mentioned below you can find a modified Lima version, which can utilize the Exynos DRM directly.

[lima-drm repository](https://gitorious.org/lima-drm/lima-drm)

The Lima video driver for RetroArch only works with this version. Proceed with the usual steps to install limare from the repository onto your system. After that compile RetroArch against the resulting limare library.

The video driver name is 'lima'. It honors the following video settings:

   - video\_monitor\_index
   - video\_fullscreen\_x and video\_fullscreen\_y

The monitor index maps to the DRM connector index. If it's zero, then it just selects the first "sane" connector, which means that it is connected to a display device and it provides at least one useable mode. If the value is non-zero, it forces the selection of this connector. For example, on the ODROID-X2 the HDMI connector has index 2.

The two fullscreen parameters select the mode the DRM should select. If zero, the native connector mode is selected. If non-zero, the DRM tries to select the wanted mode. This might fail if the mode is not available from the connector.

## Issues and TODOs

The driver still suffers from some issues.

   - The aspect ratio is wrong. The dimensions of the emulator framebuffer on the screen are not computed correctly at the moment.
   - Sometimes opening RGUI the second time results in broken rendering (flickering artifacts, blending not done properly). I don't know where these artifacts come from and they also don't appear again when closing RGUI and opening it again. This needs further investigation (silent memory corruption?).
   - Limare should be able to handle a custom pitch, when uploading texture pixel data. This would save some memcpy for emulator cores which don't provide the framebuffer with full pitch (snes9x-next for example).
