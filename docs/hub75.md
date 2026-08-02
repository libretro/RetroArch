# HUB75 RGB LED matrix video driver

RetroArch can render software-core video directly to HUB75 RGB LED panels
connected to a Raspberry Pi GPIO header. The driver uses the vendored
`rpi-rgb-led-matrix` library and does not require X11, Wayland, DRM, or a
desktop session.

## Build on Raspberry Pi

Initialize submodules, enable the driver, and build RetroArch:

```sh
./configure --enable-hub75
make -j4
```

Select the driver in `retroarch.cfg`:

```ini
video_driver = "hub75"
menu_driver = "rgui"
```

The driver uses nearest-neighbour scaling with configurable aspect handling,
supports rotation, and accepts RGB565 and XRGB8888 software frames. Hardware
rendered cores are not supported because they do not provide a CPU-readable
framebuffer.

RetroArch input overlays are composited in software with per-pixel and global
alpha. Full-screen overlays use the complete LED canvas; non-full-screen
overlays are positioned relative to the scaled game viewport.

## Panel configuration

Configuration uses environment variables so panel wiring can be changed
without adding hardware-specific values to `retroarch.cfg`:

| Variable | Meaning |
| --- | --- |
| `HUB75_ROWS` | Rows per panel (commonly 32 or 64) |
| `HUB75_COLS` | Columns per panel (commonly 64) |
| `HUB75_CHAIN` | Panels daisy-chained horizontally |
| `HUB75_PARALLEL` | Parallel output chains |
| `HUB75_BRIGHTNESS` | Brightness from 1 to 100 |
| `HUB75_SCALING` | Scaling mode: `fit`, `fill`, `stretch`, or `integer` |
| `HUB75_PWM_BITS` | PWM colour depth from 1 to 11 |
| `HUB75_GPIO_SLOWDOWN` | GPIO slowdown, usually 1-4 on newer Pi models |
| `HUB75_GPIO_MAPPING` | Pin mapping, e.g. `regular` or `adafruit-hat` |
| `HUB75_PIXEL_MAPPER` | Pixel mapper string for unusual panel layouts |
| `HUB75_RGB_SEQUENCE` | Physical channel order, e.g. `RBG` |
| `HUB75_PANEL_TYPE` | Panel initialization type such as `FM6126A` |
| `HUB75_MULTIPLEXING` | Multiplexing mapper number |
| `HUB75_ROW_ADDR_TYPE` | Row addressing type |
| `HUB75_RP1_PIO` | Set to 1 to use RP1 PIO on Raspberry Pi 5 |

Example for one 64x64 panel:

```sh
sudo env HUB75_ROWS=64 HUB75_COLS=64 HUB75_GPIO_SLOWDOWN=4 \
  ./retroarch -v
```

Scaling modes:

- `fit` preserves the complete image and adds black bars when necessary. This
  is the default.
- `fill` preserves aspect ratio and crops the image symmetrically to fill the
  entire matrix.
- `stretch` uses every LED without cropping, but may distort the image.
- `integer` uses an integer enlargement factor, or samples every Nth source
  pixel when the source is larger than the matrix. It keeps pixels uniform and
  shows the complete image, but may leave larger black borders.

For a single 128x64 panel, `fill` is generally the most useful game mode:

```sh
sudo env HUB75_ROWS=64 HUB75_COLS=128 HUB75_SCALING=fill \
  HUB75_RP1_PIO=1 ./retroarch -v
```

GPIO initialization normally requires root. Run RetroArch locally on the Pi;
timing-sensitive HUB75 output should not be driven through a remote GPIO
bridge.

## Cross-compile from macOS without Docker

With an `aarch64-linux-gnu` cross toolchain installed:

```sh
OS=Linux ./configure --host=aarch64-linux-gnu --enable-hub75
make -j8 OBJDIR_BASE=obj-linux-arm64
```

Additional RetroArch features may require a Raspberry Pi sysroot. Disable
features you do not need or supply their ARM64 headers and libraries through
the toolchain/sysroot; the bundled HUB75 library itself is built with the same
`CC`, `CXX`, and `AR` selected by RetroArch.

Setting `OS=Linux` is required when running RetroArch's configure script on
macOS; otherwise it identifies the build machine as Darwin while testing the
Linux cross compiler. A separate `OBJDIR_BASE` also prevents native macOS and
ARM64 dependency files from colliding.
