# HUB75 RGB LED matrix video driver

RetroArch can render software-core video directly to HUB75 RGB LED panels
connected to a Raspberry Pi 5 GPIO header. The driver is a self-contained
translation unit that drives the RP1 RIO registers directly; it has no external
matrix-library dependency and does not require X11, Wayland, DRM, or a desktop
session.

## Build on Raspberry Pi

Enable the driver and build RetroArch:

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
| `HUB75_BRIGHTNESS` | Brightness from 1 to 100 |
| `HUB75_SCALING` | Scaling mode: `fit`, `fill`, `stretch`, or `integer` |
| `HUB75_PWM_BITS` | PWM colour depth from 1 to 8 |
| `HUB75_GPIO_SLOWDOWN` | RP1 GPIO timing slowdown from 1 to 10 |

The initial driver supports the regular HUB75 GPIO mapping, direct ABCDE row
addressing, and one parallel output. `HUB75_CHAIN` may be used for panels
daisy-chained horizontally.

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

For a single 128x64 panel on Raspberry Pi 5, `fit` preserves the complete
game image. The RP1 RIO backend is initialized at runtime rather than selected
at compile time:

```sh
sudo env \
  HUB75_ROWS=64 \
  HUB75_COLS=128 \
  HUB75_CHAIN=1 \
  HUB75_SCALING=fit \
  ./retroarch -v
```

GPIO initialization normally requires root. Run RetroArch locally on the Pi;
timing-sensitive HUB75 output should not be driven through a remote GPIO
bridge.

## Cross-compile from macOS without Docker

With an `aarch64-linux-gnu` cross toolchain installed, the following performs
a clean build for 64-bit Raspberry Pi Linux:

```sh
cd /path/to/RetroArch

make clean OBJDIR_BASE=obj-linux-arm64
rm -f config.mk

OS=Linux ./configure \
  --host=aarch64-linux-gnu \
  --enable-hub75

make -j"$(sysctl -n hw.ncpu)" \
  OBJDIR_BASE=obj-linux-arm64

file retroarch
```

The resulting executable should be reported as an `ELF 64-bit LSB` binary for
`ARM aarch64`. Raspberry Pi 5 does not require separate compile-time CPU
selection; the driver selects its RP1 RIO backend when it starts.

Additional RetroArch features may require a Raspberry Pi sysroot. Disable
features you do not need or supply their ARM64 headers and libraries through
the toolchain/sysroot. The HUB75 driver itself only uses Linux system APIs.

Setting `OS=Linux` is required when running RetroArch's configure script on
macOS; otherwise it identifies the build machine as Darwin while testing the
Linux cross compiler. A separate `OBJDIR_BASE` also prevents native macOS and
ARM64 dependency files from colliding.
