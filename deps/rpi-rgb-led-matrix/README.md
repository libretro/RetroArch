Controlling RGB LED display with Raspberry Pi GPIO
==================================================

A library to control commonly available 128x64, 64x64, 32x32 or 16x32 RGB LED panels
with the Raspberry Pi. Can support PWM up to 11Bit per channel, providing
true 24bpp color with CIE1931 profile.

Supports 3 chains with many panels each on a regular Pi.
On a Raspberry Pi 2 or 3, you can easily chain 12 panels in that chain
(so 36 panels total), but you can theoretically stretch that to up
to 96-ish panels (32 chain length) and still reach
around 100Hz refresh rate with full 24Bit color (theoretical - never tested
this; there might likely be timing problems with the panels that will creep
up then).

With fewer colors or so-called 'outdoor panels' you can control even more,
faster.

The LED-matrix library is (c) Henner Zeller <h.zeller@acm.org>, licensed with
[GNU General Public License Version 2.0 (or any later version)](http://www.gnu.org/licenses/gpl-2.0.txt)
(which means, if you use it in a product somewhere, you need to make the
source and all your modifications available to the receiver of such product so
that they have the freedom to adapt and improve).
Yes, you are also allowed to use
this code under the [GNU General Public License Version 3.0](http://www.gnu.org/licenses/gpl-3.0.txt)
which allows linking with Apache 2.0 code and LGPL-3.0 code.

## Discourse discussion group

**If you'd like help, please do not file a bug, use the discussion board instead:**
https://rpi-rgb-led-matrix.discourse.group/  (obviously please read this whole page first).
If you file a bug asking for personal help instead of using the discourse group, please
state in your bug that you have read this entire page and that you're indeed filing a bug
or request for improvement. Otherwise, please use the discourse group.

Overview
--------
The RGB LED matrix panels can be scored at [Sparkfun][sparkfun],
[AdaFruit][ada] or eBay and Aliexpress. If you are in China, I'd try to get
them directly from some manufacturer, Taobao or Alibaba.

The `RGBMatrix` class provided in `include/led-matrix.h` does what is needed
to control these. You can use this as a library in your own projects or just
use the demo binary provided here which provides some useful examples.

Check out [utils/ directory for some ready-made tools](./utils) to get started
using the library, or the [examples-api-use/](./examples-api-use) directory if
you want to get started programming your own utils.

Panels supported
----------------
This library does not support PWM panels (which actually are better, but need
a completely different driver).\
It should support most other panels with direct addressing (3 to 5 address lines,
ABC for 8x2=16 lines, ABCD for 16x2=32 lines, and ABCDE for 32x2=64 lines).
It also supports panels using shift registers for line addressing, also called ABC
panels up to 64 lines.\
However some amount of panels have uncommon pixel layouts, especially the brighter
panels and may require special pixel mappers (some are called zigzag or zagzig, see
https://rpi-rgb-led-matrix.discourse.group/t/p10rgb-3535-4s-zaggiz-1-4scan-abc-p10-panels-and-https-github-com-2dom-pxmatrix/921 )

Until this wiki has its own confirmed list of working panels, you can refer to
this list: https://github.com/board707/DMD_STM32/wiki/Led_drivers and assume
that all supported panels there should work with this library, or could work
with minimal work to create a pixel mapper.
Please do refer to --led-multiplexing=<0..17> mentioned below and try all values. \
2025/11 update: DMD_STM32 has started adding experimental support for some PWM panels.
These should be labelled as "special" in the table above. They are not supported by this
lib, as it only supports non PWM panels (until someone contributes PWM support).

PWM/E-PWM/S-PWM Panels
----------------------
Newer PWM panels are not currently supported by this lib, but support would really be appreciated.
- https://github.com/hzeller/rpi-rgb-led-matrix/issues/466 - Master bug tracking PWM efforts
- https://github.com/hzeller/rpi-rgb-led-matrix/issues/1866 - Continued discussion

Some experimentation and success has been made with the following fork. Some panel support with FM6373, FM6363, SM16380SH
- https://github.com/kingdo9/rpi-rgb-led-matrix_pwm_experiment


Raspberry Pi 1-5 supported
------------------------------
This library supports the old Raspberry Pi's Version 1 with 26 pin header and
also the B+ models, the Pi Zero, Raspberry Pi 2,3,4 and 5 with 40 pins, as well
as the Compute Modules which have 44 GPIOs.

#### Raspberry Pi 5 support has now been added.
There are two modes to run this on the Pi 5

**--led-rp1-pio=0** - Uses the Pi 5 RP1 RIO backend (default) - Higher CPU usage but much faster performance.

**--led-rp1-pio=1** - Uses the Pi 5 RP1 PIO backend - Very minimal CPU usage.
**NOTE: --led-slowdown-gpio** can have the opposite effect in RIO mode, so going from 2 to 3 may increase performance slightly with --led-rp1-pio=0.

Pi 5 Discussion -  https://github.com/hzeller/rpi-rgb-led-matrix/issues/1603


Thanks to contributions from the following libraries to help implement this.
https://github.com/adafruit/Adafruit_Blinka_Raspberry_Pi5_Piomatter \
https://github.com/bitslip6/rpi-gpu-hub75-matrix \
https://www.i-programmer.info/programming/148-hardware/16887-raspberry-pi-iot-in-c-pi-5-memory-mapped-gpio.html

The 26 pin models can drive one chain of RGB panels, the 40 pin models
**up to three** chains in parallel (each chain 12 or more panels long).
The Compute Module can drive **up to 6 chains in parallel**.
The Raspberry Pi 2 and 3 are faster and generally preferred to the older
models (and the Pi Zero). With the faster models, the panels sometimes
can't keep up with the speed; check out
this [troubleshooting section](#troubleshooting) what to do.

A lightweight, non-GUI, distribution such as [DietPi] is recommended.
[Raspbian Lite][raspbian-lite] is a bit easier to get started with and
is a good second choice.

Types of Displays
-----------------
There are various types of displays that come all with the same Hub75 connector.
They vary in the way the multiplexing is happening so this library supports
options to choose that.
All these are configured by flags (or, programmatically, in an [Options struct](include/led-matrix.h#L57)).

If you have a 64x32 display, you need to supply the flags
`--led-cols=64 --led-rows=32` for instance.

Depending on the Matrix, there are various configuration options that
you might need to set for it to work. See further below in the README for the
[detailed description of these](#changing-parameters-via-command-line-flags).
While the `--led-rows` and `--led-cols` can be derived from simply looking
at the panels, the other options might require some experimenting to find the
right setting if there is no description provided by the manufacturer of
the panel. Going through these options for experiments would typically not do
harm, so you're free to experiment to find your setting.

Flag                                | Description
:---------------      | :-----------------
`--led-cols`          | Columns in the LED matrix, the 'width'.
`--led-rows`          | Rows in the LED matrix, the 'height'.
`--led-multiplexing`  | In particular bright outdoor panels with small multiplex ratios require this. Often an indicator: if there are fewer address lines than expected: ABC (instead of ABCD) for 32 high panels and ABCD (instead of ABCDE) for 64 high panels.
`--led-row-addr-type` | Addressing of rows; in particular panels with only AB address lines might indicate that this is needed.
`--led-panel-type`    | Chipset of the panel. In particular if it doesn't light up at all, you might need to play with this option because it indicates that the panel requires a particular initialization sequence.

Panels can be chained by connecting the output of one panel to the input of
the next panel. You can chain quite a few together, but the refresh rate will
reduce with longer chains.

The 64x64 matrixes typically come in two kinds: with 5 address
lines (A, B, C, D, E), or (A, B); the latter needs a `--led-row-addr-type=1`
parameter. So-called 'outdoor panels' are typically brighter and allow for
faster refresh-rate for the same size, but do some multiplexing internally
of which there are a few types out there; they can be chosen with
the `--led-multiplexing` parameter.
Many 128x64 panels now use ABC addressing using shift registers, instead of direct
ABCDE addressing. Look at using --led-row-addr-type=5 or --led-row-addr-type=3
with them (and you may need to use --led-slowdown-gpio of 2 or more. Please
see this bug for more details: https://github.com/hzeller/rpi-rgb-led-matrix/issues/1774

There are some panels that have a different chip-set than the default HUB75.
These require some initialization sequence. The current supported types are
`--led-panel-type=FM6126A` and `--led-panel-type=FM6127`.

Generally, the higher scan-rate (e.g. 1:8), a.k.a. outdoor panels generally
allow faster refresh rate, but you might need to figure out the multiplexing
mapping if one of the three provided does not work.

Some 32x16 outdoor matrixes with 1:4 scan (e.g. [Qiangli Q10(1/4) or X10(1/4)](http://qiangliled.com/products-63.html))
have 4 address line (A, B, C, D). For such matrices is necessary to
use `--led-row-addr-type=2` parameter. Also the matrix Qiangli Q10(1/4)
have "Z"-stripe pixel mapping and in this case, you'd use two parameters
at the same time `--led-row-addr-type=2 --led-multiplexing=4`.

Sub-Pages
---------
This documentation is split into parts that help you through the process

- <a href="wiring.md"><img src="img/wire-up-icon.png"></a>
    [**Wire up the matrix to your Pi**](./wiring.md). This document describes
    what goes where.
- [How to map pixels between panels or within panels](./lib). This is crucial for figuring out pixel mappers,
  matrix mappers and so forth. This is where you will learn about panel layout with U-Mapper, V-Mapper, V-Mapper:Z
- [Adapter GPIO boards output to up to 3 channels (electrodragon board recommended)](./adapter).
    If you have an [Adafruit HAT] or [Adafruit Bonnet], you can choose that with
    a command line option [described below](#if-you-have-an-adafruit-hat-or-bonnet)
- [All the command line options you can give to demo and in turn use in your library code](./examples-api-use).
- [Installing an Optimized Kernel to fix most flickering issues](./optimized-kernel)

Python Support
--------------
The python bindings have been overhauled as of February 2026 and are now built with scikit-build-core and cmake. While
this update has been tested there are likely edge cases that have not been caught. Please subscribe to the master Python
bug to assist or check for the latest updates.
https://github.com/hzeller/rpi-rgb-led-matrix/issues/1749.

The entire repository itself is PIP-able and can be installed as a package directly. Python dev packages are required.
```
sudo apt-get install python-dev-is-python3 python3-pil cython3
pip install git+https://github.com/hzeller/rpi-rgb-led-matrix
```

Rpi Hardware Support
--------------------
While this code should still work on rPi1 or Rpi0, those are underpowered and not recommended. An ESP32 is cheaper and faster.
Rpi2 is honestly old and slow too.
Rpi3 / Rpi3a / Rpi0 2wl will work for most displays but will be slow if you are rotating a huge display in a mapper (Rpi0 2wl is a slightly faster version of Rpi3)
Rpi4 is the fastest supported chip which will need a higher slowdown gpio value to avoid overwhelming panels (if they display garbage)
Rpi5 is not officially supported as of 2025/11, but see https://github.com/hzeller/rpi-rgb-led-matrix/issues/1603

Let's do it
------------
  1. Run a demo. You find that in the
     [examples-api-use/](./examples-api-use#running-some-demos) directory:
```
make -C examples-api-use
sudo examples-api-use/demo -D0
```
  2. Use the utilities. The [utils](./utils) directory has some ready-made
    useful utilities to show content. [Go there](./utils) to see how to
    compile and run these.
  3. Write your own programs using the Matrix in C++ or one of the
     bindings such as Python or C#.

### Wiring / Boards

Please see the [Adadpter Boards or Self Wiring](./adapter).

Summary is:
- Yes you can self wire without level shifters and it will work most of the time, but if you're not in a hurry get a board
- https://www.electrodragon.com/product/rgb-matrix-panel-drive-board-for-raspberry-pi-v2/ **is the recommended solution with 3 channels and level shifters**. You can't go wrong there, but expect a bit of shipping time.
- If shipping time is crucial and you don't want to wire your own, Adafruit sells a single channel board (the electrodragon one is 3 channels), but note that its wiring is non standard and requires a special compile option or command line argument: https://www.adafruit.com/product/3211

### Utilities

The [utils directory](./utils) is meant for ready utilities to show images or
animated gifs or videos. Read the [README](./utils/README.md) there for
instructions how to compile.

There are external projects that use this library and provide higher level
network protocols, such as the
 * [FlaschenTaschen implementation](https://github.com/hzeller/flaschen-taschen)
   (VLC can send videos to it natively)
 * [PixelPusher implementation](https://github.com/hzeller/rpi-matrix-pixelpusher) (common in light art installations)
 * [ZeroMQ-server](https://github.com/Knifa/led-matrix-zmq-server) to receive
   content.
 * Marc's [FastLED_RPIRGBPanel_GFX](http://marc.merlins.org/perso/arduino/post_2020-01-01_Running-FastLED_-Adafruit_GFX_-and-LEDMatrix-code-on-High-Resolution-RGBPanels-with-a-Raspberry-Pi.html) allows running arduino code on linux/rPi and display on bigger RGBPanel matrices than arduino chips, can.

### API

The library comes as an API that you can use for your own utilities and use-cases.

  * The native library is a C++ library (see [include/](./include)).
    Example uses you find in the [examples-api-use/](./examples-api-use)
    directory.
  * If you prefer to program in C, there is also a
    [C API](./include/led-matrix-c.h).
  * In the [python](./bindings/python) subdirectory, you find a Python API including a
    couple of [examples](./bindings/python/samples) to get started.
  * There are a couple of external bindings, such as
      * [Nodejs binding] by Maxime Journaux
      * [Nodejs/Typescript binding] by Alex Eden
      * [Go binding] by Máximo Cuadros
      * [Rust binding] by Vincent Pasquier

### Changing parameters via command-line flags

For the programs in this distribution and also automatically in your own
programs using this library, there are a lot of parameters provided as command
line flags, so that you don't have to re-compile your programs to tweak them.
Some might need to be changed for your particular kind of panel.

Here is a little run-down of what these command-line flags do and when you'd
like to change them.

First things first: if you have a different wiring than described in
[wiring](./wiring.md), for instance if you have an Adafruit HAT/Bonnet, you can
choose these here:

```
--led-gpio-mapping=<gpio-mapping>: Name of GPIO mapping used. Default "regular"
```

This can have values such as
  - `--led-gpio-mapping=regular` The standard mapping of this library, described in the [wiring](./wiring.md) page.
  - `--led-gpio-mapping=adafruit-hat` The Adafruit HAT/Bonnet, that uses this library or
  - `--led-gpio-mapping=adafruit-hat-pwm` Adafruit HAT with the anti- hardware mod [described below](#improving-flicker-hardware-patch).
  - `--led-gpio-mapping=compute-module` Additional 3 parallel chains can be used with the Compute Module.

Learn more about the mappings in the [wiring documentation](wiring.md#alternative-hardware-mappings).

#### GPIO speed

```
--led-slowdown-gpio=<0..10>: Slowdown GPIO. Needed for faster Pis and/or slower panels (Default: 1).
```

The Raspberry Pi starting with Pi2 are putting out data too fast for almost
all LED panels I have seen. In this case, you want to slow down writing to
GPIO. Zero for this parameter means 'no slowdown'.

The default 1 (one) typically works fine, but often you have to even go further
by setting it to 2 (two). If you have a Raspberry Pi with a slower processor
(Model A, A+, B+, Zero), then a value of 0 (zero) might work and is desirable.

If you find yourself needing very high values of slowdown like 8 for ABC 128x64 panels,
do try --led-row-addr-type=5 instead of --led-row-addr-type=3. Details in this bug:
https://github.com/hzeller/rpi-rgb-led-matrix/issues/1773

A Raspberry Pi 3 or Pi4 might even need higher values for the panels to be
happy.

#### Panel Connection
The next most important flags describe the type and number of displays connected

```
--led-rows=<rows>        : Panel rows. Typically 8, 16, 32 or 64. (Default: 32).
--led-cols=<cols>        : Panel columns. Typically 32 or 64. (Default: 32).
--led-chain=<chained>    : Number of daisy-chained panels. (Default: 1).
--led-parallel=<parallel>: For A/B+ models or RPi2,3b: parallel chains. range=1..3 (Default: 1, 6 for Compute Module).
```

These are the most important ones: here you choose how many panels you have
connected and how many rows are in each panel. Panels can be chained (each panel
has an input and output connector, see the
[wiring documentation](wiring.md#chains)) -- the `--led-chain` flag tells the
library how many panels are chained together. The newer Raspberry Pi's allow
to connect multiple chains in parallel, the `--led-parallel` flag tells it how
many there are.

This illustrates what each of these parameters mean:

<a href="wiring.md#chaining-parallel-chains-and-coordinate-system"><img src="img/coordinates.png"></a>

##### Panel Type

Typically, panels should just work out of the box, but some panels use a
different chip-set that requires some initialization. If you don't see any
output on your panel, try setting:

```
--led-panel-type=FM6126A
```

Some panels have the FM6127 chip, which is also an option.

##### Multiplexing
If you have some 'outdoor' panels or panels with different multiplexing,
the following will be useful:

```
--led-multiplexing=<0..17> : Mux type: 0=direct; 1=Stripe; 2=Checkered...
```

The outdoor panels have different multiplexing which allows them to be faster
and brighter, but by default their output looks jumbled up.
They require some pixel-mapping of which there are a few
types you can try and hopefully one of them works for your panel; The default=0
is no mapping ('standard' panels), while 1, 2, ... are different mappings
to try with. If your panel has a different mapping, you find everything you
need to implement one in [lib/multiplex-mappers.cc](lib/multiplex-mappers.cc).
Please send a pull request if you encounter a panel for which you needed to
implement a new mapping.

Note that you have to set the `--led-rows` and `--led-cols` to the rows and
columns that are physically on each chained panel so that the multiplexing
option can work properly. For instance a `32x16` panel with `1:4` multiplexing
would be controlled with `--led-rows=16 --led-cols=32 --led-multiplexing=1` (or
whatever multiplexing type your panel is, so it can also be `--led-multiplexing=2` ...).

For `64x32` panels with `1:8` multiplexing, this would typically be
`--led-rows=32 --led-cols=64 --led-multiplexing=1`;
however, there are some panels that internally behave like
two chained panels, so then you'd use
`--led-rows=32 --led-cols=32 --led-chain=2 --led-multiplexing=1`;

```
--led-row-addr-type=<0..5>: 0 = default; 1 = AB-addressed panels; 2 = direct row select; 3 = ABC-addressed panels; 4 = ABC Shift + DE direct, 5 = ABC method similar to 3, but faster on some panels (needs less of a slowdown) (Default: 0).
```

This option is useful for certain 64x64 or 32x16 panels. For 64x64 panels,
that only have an `A` and `B` address line, you'd use `--led-row-addr-type=1`.
This is only tested with one panel so far, so if it doesn't work for you,
please send a pull request.

For 32x16 outdoor panels, that have have 4 address line (A, B, C, D), it is
necessary to use `--led-row-addr-type=2`.

Performance improvements and limits, Maximizing refresh rate, ABC vs ABCDE, row-addr-type=5 vs 3
-------------------------------------------------------------------------------------------------
Regardless of which driving hardware you use, ultimately you can only push pixels
so fast to a string of panels before you get flickering due to too low a refresh
rate (less than 80-100Hz), or before you refresh the panel lines too fast and they
appear too dim because each line is not displayed long enough before it is turned off.

Basic performance tips:
- Use --led-show-refresh to see the refresh rate while you try parameters
- use an active-3 board with led-parallel=3 any time possible instead of chaining panels.
- led-pwm-dither-bits=1 gives you a speed boost but potentially less brightness
- led-pwm-lsb-nanoseconds=50 also gives you a speed boost but may lead to less brightness
- led-pwm-bits=7 or even lower decrease color depth but increases refresh speed
- AB panels and other panels with that use values of led-multiplexing bigger than 0,
will also go faster, although as you tune more options given above, their advantage will decrease.
- 32x16 ABC panels are faster than ABCD which are faster than ABCDE, which are faster than 128x64 ABC panels
(which do use 5 address lines, but over only 3 wires)
- For 128x64 ABC speed on shift registers vs ABCDE
- Use at least an rPi3 (rPi4 is still slightly faster but may need --led-slowdown-gpio=2)

Maximizing refresh rate to 300 or even 400Mhz refresh for 3x 128x64 panels:
- ABCDE panels will allow for faster refresh rate than ABC panels because they can be run with
lower --led-slowdown-gpio=1 (which may need to be 2 for ABCDE) on rPi3 (rPi4 may require bigger
slowdowns. You may even end up in a situation where a Pi3 will give you faster refresh than a Pi4
by needing smaller slowdowns
- To achieve 410Hz refresh for 3x 128x64 direct ABCDE panels, use this "~/rpi-rgb-led-matrix/examples-api-use/demo --led-rows=64 --led-cols=128 --led-chain=1 --led-parallel=3 --led-show-refresh --led-scan-mode=0 --led-pwm-bits=7 --led-pwm-lsb-nanoseconds=50 --led-pwm-dither-bits=1 -D0  --led-row-addr-type=0 --led-slowdown-gpio=1"
- To achieve 350hz refresh for 3x 128x64 shift register panels, use this: "~/rpi-rgb-led-matrix/examples-api-use/demo --led-rows=64 --led-cols=128 --led-chain=1 --led-parallel=3 --led-show-refresh --led-scan-mode=0 --led-pwm-bits=7 --led-pwm-lsb-nanoseconds=50 --led-pwm-dither-bits=1 -D0  --led-row-addr-type=5 --led-slowdown-gpio=2"

Please see this bug for discussion of ABC vs ABCDE and using --led-row-addr-type=5 instead of --led-row-addr-type=3:
https://github.com/hzeller/rpi-rgb-led-matrix/issues/1774#issuecomment-2860617056

Maximum resolutions reasonably achievable:
A general rule of thumb is that running 16K pixels (128x128 or otherwise) on a single chain,
is already pushing limits and you will have to make tradeoffs in visual quality. 32K pixels
(like 128x256) is definitely pushing things and you'll get 100Hz or less depending on the
performance options you choose.
This puts the maximum reasonable resolution around 100K pixels (like 384x256) for 3 chains.
You can see more examples and video capture of speed on [Marc MERLIN's page 'RGB Panels, from 192x80, to 384x192, to 384x256 and maybe not much beyond'](http://marc.merlins.org/perso/arduino/post_2020-03-13_RGB-Panels_-from-192x80_-to-384x192_-to-384x256-and-maybe-not-much-beyond.html)
If your refresh rate is below 300Hz, expect likely black bars when taking cell phone pictures.
A real camera with shutter speed lowered accordingly, will get around this.

Ultimately, you should not expect to go past 64K pixels using 3 chains without significant
quality tradeoffs. If you need bigger displays, you should use multiple boards and synchronize the
output.




#### Panel Arrangement

```
--led-pixel-mapper  : Semicolon-separated list of pixel-mappers to arrange pixels.
```

Optional params after a colon e.g. "U-mapper;Rotate:90"

Available | Parameter after colon| Example
----------|----------------------|----------
Mirror    | `H` or `V` for horizontal/vertical mirror. | `Mirror:H`
Rotate    | Degrees.                                   | `Rotate:90`
U-mapper  | -

Mapping the logical layout of your boards to your physical arrangement. See
more in [Remapping coordinates](./examples-api-use#remapping-coordinates).

#### Misc Options

```
--led-brightness=<percent>: Brightness in percent (Default: 100).
```

Self explanatory.


```
--led-pwm-bits=<1..11>    : PWM bits (Default: 11).
```

The LEDs can only be switched on or off, so the shaded brightness perception
is achieved via PWM (Pulse Width Modulation). In order to get a good 8 Bit
per color resolution (24Bit RGB), the 11 bits default per color are good
(why ? Because our eyes are actually perceiving brightness logarithmically, so
we need a lot more physical resolution to get 24Bit sRGB).

With this flag, you can change how many bits it should use for this; lowering it
means the lower bits (=more subtle color nuances) are omitted.
Typically you might be mostly interested in the extremes: 1 Bit for situations
that only require 8 colors (e.g. for high contrast text displays) or 11 Bit
for everything else (e.g. showing images or videos). Why would you bother at all ?
Lower number of bits use slightly less CPU and result in a higher refresh rate.

```
--led-show-refresh        : Show refresh rate.
```

This shows the current refresh rate of the LED panel, the time to refresh
a full picture. Typically, you want this number to be pretty high, because the
human eye is pretty sensitive to flicker. Depending on the settings, the
refresh rate with this library are typically in the hundreds of Hertz but
can drop low with very long chains. Humans have different levels of perceiving
flicker - some are fine with 100Hz refresh, others need 250Hz.
So if you are curious, this gives you the number (shown on the terminal).

The refresh rate depends on a lot of factors, from `--led-rows` and `--led-chain`
to `--led-pwm-bits`, `--led-pwm-lsb-nanoseconds` and `--led-pwm-dither-bits`.
If you are tweaking these parameters, showing the refresh rate can be a
useful tool.

```
--led-limit-refresh=<Hz>  : Limit refresh rate to this frequency in Hz. Useful to keep a
                            constant refresh rate on loaded system. 0=no limit. Default: 0
```

This allows to limit the refresh rate to a particular frequency to approach
a fixed refresh rate.

This can be used to mitigate some situations in which you have a faint flicker,
which can happen due to hardware events (network access)
or other situations such as other IO or heavy memory access by other
processes. Also when you see wildly changing refresh frequencies with
`--led-show-refresh`.

You trade a slightly slower refresh rate and display brightness for less
visible flicker situations.

For this to calibrate, run your program for a while with --led-show-refresh
and watch the line that shows the current refresh rate and minimum refresh
rate observed. So wait a while until that value doesn't
change anymore (e.g. a minute, so that you catch tasks that happen once
a minute, such as ntp updated).
Use this as a guidance what value to choose with `--led-limit-refresh`.

The refresh rate will now be adapted to always reach this value
between frames, so faster refreshes will be slowed down, but the occasional
delayed frame will fit into the time-window as well, thus reducing visible
brightness fluctuations.

You can play with value a little and reduce until you find a good balance
between refresh rate and flicker suppression.

Use this also if you want to have a stable baseline refresh rate when using
the vsync-multiple flag `-V` in the [led-image-viewer] or
[video-viewer] utility programs.

```
--led-no-busy-waiting     : Don't use busy waiting when limiting refresh rate.
```

This allows to switch from busy waiting to sleep waiting when limiting the
refresh rate (`--led-limit-refresh`).

By default, refresh rate limiting uses busy waiting, which is CPU intensive but
gives most accurate timings. This is fine for multi-core boards.

On single core boards (e.g.: Raspberry Pi Zero) busy waiting makes the system
unresponsive for other/background tasks. There, sleep waiting improves the
system's responsiveness at the cost of slightly less accurate timings.

```
--led-scan-mode=<0..1>    : 0 = progressive; 1 = interlaced (Default: 0).
```

This switches from progressive scan and interlaced scan. The latter might
look be a little nicer when you have a very low refresh rate, but typically
it is more annoying because of the comb-effect (remember 80ies TV ?).


```
--led-pwm-lsb-nanoseconds : PWM Nanoseconds for LSB (Default: 130)
```

This allows to change the base time-unit for the on-time in the lowest
significant bit in nanoseconds.
Lower values will allow higher frame-rate, but will also negatively impact
qualty in some panels (less accurate color or more ghosting).

Good values for full-color display (PWM=11) are somewhere between 100 and 300.

If you you use reduced bit color (e.g. PWM=1) and have sharp contrast
applications, then higher values might be good to minimize ghosting.

How to decide ? Just leave the default if things are fine. But some panels have
trouble with sharp contrasts and short pulses that results
in ghosting. It is particularly apparent in situations such as bright text
on black background. In these cases increase the value until you don't see
this ghosting anymore.

The following example shows how this might look like:

Ghosting with low --led-pwm-lsb-nanoseconds  | No ghosting after tweaking
---------------------------------------------|------------------------------
![](img/text-ghosting.jpg)                   |![](img/text-no-ghosting.jpg)

If you tweak this value, watch the framerate (`--led-show-refresh`) while playing
with this number.

```
--led-pwm-dither-bits   : Time dithering of lower bits (Default: 0)
```

The lower bits can be time dithered, i.e. their brightness contribution is
achieved by only showing them some frames (this is possible,
because the PWM is implemented as binary code modulation).
This will allow higher refresh rate (or same refresh rate with increased
`--led-pwm-lsb-nanoseconds`).
The disadvantage could be slightly lower brightness, in particular for longer
chains, and higher CPU use.
CPU use is not of concern for Raspberry Pi 2 or 3 (as we run on a dedicated
core anyway) but probably for Raspberry Pi 1 or Pi Zero.
Default: no dithering; if you have a Pi 3 and struggle with low frame-rate due
to high multiplexing panels (1:16 or 1:32) or long chains, it might be
worthwhile to try.

```
--led-no-hardware-pulse   : Don't use hardware pin-pulse generation.
```

This library uses a hardware subsystem that also is used by the sound. You can't
use them together. If your panel does not work, this might be a good start
to debug if it has something to do with the sound subsystem (see Troubleshooting
section). This is really only recommended for debugging; typically you actually
want the hardware pulses as it results in a much more stable picture.

<a name="no-drop-priv"></a>

```
--led-no-drop-privs       : Don't drop privileges from 'root' after initializing the hardware.
```

You need to start programs as root as it needs to access some low-level hardware
at initialization time. After that, it is typically not desirable to stay in this
role, so the library then drops the privileges.

This flag allows to switch off this behavior, so that you stay root.
Not recommended unless you have a specific reason for it (e.g. you need root
to access other hardware or you do the privilege dropping yourself).

```
--led-daemon              : Make the process run in the background as daemon.
```

If this is set, the program puts itself into the background (running
as 'daemon').
You might want this if started from an init script at boot-time.

```
--led-inverse             : Switch if your matrix has inverse colors on.
--led-rgb-sequence        : Switch if your matrix has led colors swapped (Default: "RGB")
```

These are if you have a different kind of LED panel in which the logic of the
color bits is reversed (`--led-inverse`) or where the Red, Green and Blue LEDs
are mixed up (`--led-rgb-sequence`). You know it when you see it.

Troubleshooting
---------------
Here are some tips in case things don't work as expected.

### Use minimal Raspbian distribution
In general, run a minimal configuration on your Pi.
  
  * Common cause for flickering \
    **Ensure 5V is getting to LED Display and Raspberry Pi devices !!** \
    Use a Multimeter to check, also use dmesg command to check for any Raspberry Pi undervolt messages.\
    It may be best to power the LED Display externally especially if using Raspberry Pi 4.

    Additionally see [Installing an Optimized Kernel to fix most flickering issues](./optimized-kernel)

  * Do not use a graphical user interface (Even though the
    Raspberry Pi foundation makes you believe that you can do that: don't.
    Using a Pi with a GUI is a frustratingly slow use of an otherwise
    perfectly good embedded device.).
    Always operate your Raspberry Pi [headless].

  * Switch off on-board sound
    (`dtparam=audio=off` in `/boot/config.txt` pre-bookworm)
    (`dtparam=audio=off` in `/boot/firmware/config.txt` post-bookworm)
    External USB sound adapters work, and are much better quality anyway,
    so that is recommended if you happen to need sound. The on-board sound
    uses a timing circuit that the RGB-Matrix needs (it seems in some
    distributions, such as arch-linux, this is not enough and you need
    to explicitly blacklist the snd_bcm2835 module).\     
    
    post-bookworm \
    nano /etc/modprobe.d/blacklist-audio.conf \
    blacklist snd_bcm2835 \
    CTRL+S CTRL+X to Save File and Exit


  * Don't run anything that messes in parallel with the GPIO pins, e.g.
    PiGPIO library/daemon or devices that use the i2c or 1-wire interface if
    they are on the same pins you need for the panel.

  * I have also seen reports that on some Pis, the one-wire protocol is
    enabled (w1-gpio). This will also not work (disable by removing
    `dtoverlay=w1-gpio` in `/boot/config.txt` (pre-bookworm) or in
    `/boot/firmware/config.txt` (post-bookworm); or using `raspi-config`,
    Interface Options -> 1-Wire)

  * If you see some regular flickering, make sure that there is no other
    process running on the system that could cause that. For instance, it is
    known that merely running `top` creates a faint flicker every second it
    updates. Or a regular ntp run can also cause flicker once a minute
    (switch off with `sudo timedatectl set-ntp false`). Maybe instead you
    might want to run ntp at system start-up but then not regularly updating.
    There might be other things running regularly you don't need;
    consider a `sudo systemctl stop cron` for instance.
    To address some irregular flicker, consider the
    [`--led-limit-refresh`](#misc-options) option.

  * There are probably other processes that are running that you don't need
    and remove them; I usually remove right away stuff I really don't need e.g.
    ```
    sudo apt-get remove bluez bluez-firmware pi-bluetooth triggerhappy pigpio
    ```
    Take a close look at your systemd (`systemctl`) and see if there are other
    things running you don't need. If you have seen packages in standard
    Raspbians that interfere with the matrix code, let me know to include it
    here.
    In general: This is why starting with a minimal installation is a good
    idea: there is simply less cruft that you have to disable.

  * When attempting to connect sensors like the DHT22 to available GPIO pins
    and display their results on your LED panel, you might encounter an issue
    where the sensors fail to provide any data due to the Raspberry Pi resource
    limitations. This situation is particularly evident on older models like the
    Raspberry Pi 1 B. To address this, it is advisable to select a Raspberry Pi
    model with higher resources to ensure proper functionality.


The default install of **[Raspbian Lite][raspbian-lite]** or **[DietPi]**
seem to be good starting points, as they have a reasonably minimal
configuration to begin with. Raspbian Lite is not as lite anymore
as it used to be; I prefer DietPi these days. Note that any of the above
changes to `/boot/...` must be applied to `/boot/firmware/...` if you are on a
non-Raspbian distro (e.g. Ubuntu for ARM).

### Bad interaction with Sound
If sound is enabled on your Pi, this will not work together with the LED matrix,
as both need the same internal hardware sub-system (a first test to see if you
are affected is to run the progrem with `--led-no-hardware-pulse` and see if
things work fine then).

If you run `lsmod` and see the `snd_bcm2835` module, this could be causing trouble.
(The library actually exits if it finds this module to be loaded).

In that case, you should create a kernel module blacklist file like the
following on your system and update your initramfs:

```
cat <<EOF | sudo tee /etc/modprobe.d/blacklist-rgb-matrix.conf
blacklist snd_bcm2835
EOF

sudo update-initramfs -u
```

Reboot and confirm that the module is not loaded.

### I have followed some tutorial on the Internet and it doesn't work

Well, if you use this library, please read the documentation provided _here_,
not on some other website. Most important for you to get started
is the [wiring guide](./wiring.md). There are some tutorials floating around
that refer to a very old version of this library.

### I have a Pi1 Revision1 and top part of Panel doesn't show green

Use `--led-gpio-mapping=regular-pi1`

### Logic level voltage not sufficient
Some panels don't interpret the 3.3V logic level well, or the RPi output drivers
have trouble driving longer cables, in particular with
faster Raspberry Pis Version 2. This results in artifacts like randomly
showing up pixels, color fringes, or parts of the panel showing 'static'.

If you encounter this, try these things

   - Make sure to have as short as possible flat-cables connecting your
     Raspberry Pi with the LED panel.

   - In particular if the chips close to the input of the LED panel
     read 74HC245 instead of 74HCT245 or 74AHCT245, then this board will not
     work properly with 3.3V inputs coming from the Pi.
     Use an [adapter board](./adapter/active-3) with a bus-driver that acts as
     level shifter between 3.3V and 5V.
     (In any case, it is always a good idea to use the level shifters).

   - A temporary hack to make HC245 inputs work with the 3.3V levels is to
     supply only like 4V to the LED panel. But the colors will be off, so not
     really useable as long-term solution.

   - If you can't implement the above things, or still have problems, you can
     slow down the GPIO writing a bit. This will of course reduce the
     frame-rate, so it comes at a cost.

For GPIO slow-down, add the flag `--led-slowdown-gpio=2` to the invocation of
the binary.

If you have an Adafruit HAT or Bonnet
---------------------------

Generally, if you want to connect RGB panels via an adapter instead of
hand-wiring, I suggest to build one of the adapters whose open-hardware
files you find in the [adapter/](./adapter) subdirectory. It is a fun solder
exercise with large surface mount components.

However, Adafruit [offers an adapter][adafruit-hat] which is already ready-made,
but it only allows for a single chain. If the
ready-made vs. single-chain tradeoff is worthwhile, then you might go for that
(I am not affiliated with Adafruit).

### Switch the Pinout

The Adafruit HAT/Bonnet uses this library but a modified pinout to support other
features on the HAT. You can choose the Adafruit pinout with a command line
flag.

Just pass the option `--led-gpio-mapping=adafruit-hat`. This works on the C++
and Python examples.

### Improving flicker: hardware patch

To improve flicker, we need to do a little hardware modification,
but it is very simple: solder a wire between GPIO 4 and 18 as shown in the
following picture (click to enlarge):

<a href="img/adafruit-mod.jpg"><img src="img/adafruit-mod.jpg" height="80px"></a>

Then, start your programs with `--led-gpio-mapping=adafruit-hat-pwm`.

Now you should have less visible flicker. This essentially
switches on the hardware pulses feature for the Adafruit HAT/Bonnet.

### Improving flicker: Optimized Kernel
See [Installing an Optimized Kernel to fix most flickering issues](./optimized-kernel)


### 64x64 with E-line on Adafruit HAT/Bonnet
There are LED panels that have 64x64 LEDs packed, but they need 5 address lines,
which is 1:32 multiplexing (they have an `E` address-line). The first generation
of the Adafruit HAT/Bonnet was not prepared for this, but it can be done with another
hardware mod. Beginning October 2018, Adafruit began selling an updated version of
the HAT that supports 64x64 panels simply by bridging two pads on the PCB with solder.

You can identify which HAT you have by looking for the **Address E** pads, circled here:

<a href="https://cdn-learn.adafruit.com/assets/assets/000/063/005/original/led_matrices_addr-e-pad.jpg" target="_blank"><img src="https://cdn-learn.adafruit.com/assets/assets/000/063/005/original/led_matrices_addr-e-pad.jpg" height=80></a>

### New Adafruit RGB Matrix Hat (with Address E pads)

Look for the Address E pads located between the HUB75 connector and Pi camera cutout.

Melt a blob of solder between the center “E” pad the the “8” pad just above it
(for 64x64 matrices in the Adafruit shop)…*_or_* the “16” pad below (rare, for some
third-party 64x64 matrices…check datasheet).

### Old Adafruit HAT/Bonnet (without)

Newer HAT/Bonnet versions have a special pad for the E-line next to the IDC socket
which makes this mod easily doable with a bit of solder. For details, refer to
[the Adafruit page on driving matrices](https://learn.adafruit.com/adafruit-rgb-matrix-plus-real-time-clock-hat-for-raspberry-pi/driving-matrices).

For older HATs, the only option is a little more advanced hack, so it's only really
for people who are comfortable with this kind of thing.
First, you have to figure out which is the input of the E-Line on your matrix
(they seem to be either on Pin 4 or Pin 8 of the IDC connector).
You need to disconnect that Pin from the ground plane (e.g. with an Exacto
knife) and connect GPIO 24 to it. The following images illustrate the case for
IDC Pin 4.

<a href="img/adafruit-64x64-front.jpg"><img src="img/adafruit-64x64-front.jpg" height="80px"></a>
<a href="img/adafruit-64x64-back.jpg"><img src="img/adafruit-64x64-back.jpg" height="80px"></a>

If the direct connection does not work, you need to send it through a free
level converter of the Adafruit HAT/Bonnet. Since all unused inputs are grounded
with traces under the chip, this involves lifting a leg from the
HCT245 (figure out a free bus driver from the schematic). If all of the
above makes sense to you, you have the Ninja level to do it!

It might be more convenient at this point to consider the [Active3 adapter](./adapter/active-3)
that has that covered already.

Running as root
---------------
The library requires to access hardware registers to control the LED matrix,
and create accurate timings. These hardware accesses require to run as root
user.

For security reasons, it is usually not a good idea to run an application
as root entirely, so this library makes sure to drop privileges immediately
after the hardware is initialized.

You can switch off the privilege dropping with the
[`--led-no-drop-privs`](#user-content-no-drop-priv) flag, or, if you do this
programmatically,
choose the configuration in the
[`RuntimeOptions struct`](https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/include/led-matrix.h#L401).

Note, you _could_ run as non-root, which will use `/dev/gpiomem`
to at least write to GPIO, however the precise timing hardware registers are
not accessible. This will result in flicker and color degradation. Starting
as non-root is not recommended.

CPU use
-------

These displays need to be updated constantly to show an image with PWMed
LEDs. This is dependent on the length of the chain: for each chain element,
about 1'000'000 write operations have to happen every second!
(chain_length * 32 pixel long * 16 rows * 11 bit planes * 180 Hz refresh rate).

We can't use hardware support for writing these as DMA is too slow,
thus the constant CPU use on an RPi is roughly 30-40% of one core.
Keep that in mind if you plan to run other things on this computer (This
is less noticeable on Raspberry Pi, Version 2 or 3 that has more cores).

Also, the output quality is susceptible to other heavy tasks running on that
computer - there might be changes in the overall brightness when this affects
the refresh rate.

If you have a loaded system and one of the newer Pis with 4 cores, you can
reserve one core just for the refresh of the display. Add:
```
isolcpus=domain,managed_irq,3 nohz_full=3 rcu_nocbs=3 irqaffinity=0,1,2
```

to the end of the line in `/boot/cmdline.txt` (pre-bookworm) or
`boot/firmware/cmdline.txt` (post-bookworm). It needs to be in the same line
line as the existing arguments -- no newline. This will use the last core
only to refresh the display then, but it also means, that no other process can
utilize it then. Still, I'd typically recommend it.

This is just a partial fix, if you need better, please look at
[installing an Optimized Kernel to fix most flickering issues](./optimized-kernel)

Limitations
-----------
If you are using the Adafruit HAT/Bonnet in the default configuration, then we
can't make use of the PWM hardware (which only outputs
to a particular pin), so you'll see random brightness glitches. I strongly
suggest to do the aforementioned hardware mod.

The system needs constant CPU to update the display. Using the DMA controller
was considered but after extensive experiments
( https://github.com/hzeller/rpi-gpio-dma-demo )
dropped due to its slow speed..

There is an upper limit in how fast the GPIO pins can be controlled, which
limits the frame-rate. Raspberry Pi 2's and newer are generally faster.

Even with everything in place, you might see faint brightness fluctuations
in particular if there is something going on on the network or in a terminal
on the Pi; maybe there are also hardware limitations (memory bus
contention?).

To address the brightness fluctuations, you might experiment with the
`FIXED_FRAME_MICROSECONDS` compile time option in [lib/Makefile](lib/Makefile)
that has instructions how to set it up.

Fun
---
I am always happy to see users successfully using the software for wonderful
things, like this installation by Dirk in Scharbeutz, Germany:

![](./img/user-action-shot.jpg)

A a few pictures from  https://github.com/marcmerlin :
![thumb1024_102_20240629_288x192_RGBPanels_Google](https://github.com/user-attachments/assets/2adbef86-96bf-4ff8-afb4-493aa9174de7)
![thumb1024_137_20240629_288x192_RGBPanels_Google](https://github.com/user-attachments/assets/d87ca86c-2804-4aee-b6f2-f4c88145908c)
![prev1280_140_20240629_288x192_RGBPanels_Google](https://github.com/user-attachments/assets/2a621c91-4a84-4b4e-a9d2-092073cdee41)

And probably the highest resolution build (384x256):
![thumb1024_429_20200310_FastLED_RPIRGBPanel_GFX_384x256](https://github.com/user-attachments/assets/60114825-efc4-4fef-88b0-749b53fa17b8)
![prev1280_551_20200310_FastLED_RPIRGBPanel_GFX_TableME_384x256](https://github.com/user-attachments/assets/94ea3c99-b751-4d12-8709-ef8b2bd197b9)

https://www.youtube.com/watch?v=85PI2C6oBsQ

[led-image-viewer]: ./utils#image-viewer
[video-viewer]: ./utils#video-viewer
[matrix64]: ./img/chained-64x64.jpg
[sparkfun]: https://www.sparkfun.com/products/12584
[ada]: http://www.adafruit.com/product/1484
[rt-paper]: https://www.osadl.org/fileadmin/dam/rtlws/12/Brown.pdf
[adafruit-hat]: https://www.adafruit.com/products/2345
[raspbian-lite]: https://downloads.raspberrypi.org/raspbian_lite_latest
[DietPi]: https://dietpi.com/
[Adafruit HAT]: https://www.adafruit.com/products/2345
[Adafruit Bonnet]: https://www.adafruit.com/product/3211
[Nodejs binding]: https://github.com/zeitungen/node-rpi-rgb-led-matrix
[Go binding]: https://github.com/mcuadros/go-rpi-rgb-led-matrix
[Rust binding]: https://crates.io/crates/rpi-led-matrix
[Nodejs/Typescript binding]: https://github.com/alexeden/rpi-led-matrix
[headless]: https://www.raspberrypi.com/documentation/computers/configuration.html#setting-up-a-headless-raspberry-pi
[how to build bigger displays]: https://marc.merlins.org/perso/arduino/post_2020-03-13_RGB-Panels_-from-192x80_-to-384x192_-to-384x256-and-maybe-not-much-beyond.html

Maintenance
-----------
Henner as of 2025, isn't actively maintaining this project due to plenty of other projects that keep him busy :)
A few people have stepped him to help:
- https://github.com/marcmerlin is no expert on the codebase but is willing to merge safe looking and well tested patches
- https://github.com/board707 has provided valuable technical expertise on new panel types and chips as well as ABC mode 5 code for newer panels
- https://github.com/ty-porter and https://github.com/chrisgilldc have provided python binding expertise and help
