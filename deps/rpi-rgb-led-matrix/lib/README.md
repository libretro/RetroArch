Remapping coordinates (pixels within panels and assigning pixel order across panels)
------------------------------------------------------------------------------------
Some panels do not even output pixels in the natural left to right, up to down order.
For those, see multipler lower down on this page.

For now, let's focus on how panels are chained. 
You might choose a different physical layout than the wiring provides.
The library by default will do this the horizontal way
```
chain 1 - P1 - P2 - P3
chain 2 - P1 - P2 - P3
chain 3 - P1 - P2 - P3
```

Once you go past 4 or 5 panels wide, you may want to have a layout like this.
For that, you want U-mapper.
```
chain 1 - P1 - P2 - P3
          P6 - P5 - P4
chain 2 - P1 - P2 - P3
          P6 - P5 - P4
chain 3 - P1 - P2 - P3
          P6 - P5 - P4
```

Remember that you can always rotate the display in software, so your main goal is to
have 2 or 3 channels in any direction.  This means indeed the channels can go down 
instead with V-mapper and V-mapper:Z (see below for more). Note that in that case the 
pixels go up.
```
  P3
  P2
  P1
chain 1  chain2  chain3
```

But what if you have a layout that really need 4 channels, like Mark Estes' monster 52
panel display (made with 32x16 panels)
```
P01 P02 ... P13
P14 P15 ... P26
P27 P28 ... P39
P40 P41 ... P52
```
The problem here is that V-Mapper does not support U, and coming back down for a 2nd row
but when you are stuck, you should still consider V-Mapper and connecting panels that are
side by side as one logical panel.

So let's rotate that (remember physical orientation doesn't matter, you can add Rotate:90 at the end)
```
P40-P27     P14-P01

P50-P37
^
P51-P38
      ^
P52-P39     P26-P13
^           ^
chain1      chain2
```
Now, V-Mapper:Z does not need to know that you have 2 adjacent panels connected on each
horizontal line, what you do is tell it
```
-led-rows=16 --led-cols=64 --led-pixel-mapper=V-Mapper:Z,Rotate:90
```
The important bit is your 32x16 panels become some single 64x16 horizontal panels as far 
as the library is concerned and count as a single panel for the V configuration. 
That way, you can get the layout you wanted.

This shows how to turn 32x16 panels into 64x16 panels usable as single vertical
panels for Vmapper:Z.  Note that this is not visible in the picture, every other 
level the panels are upside down to allow for shorter ribbon cables in between.
Note that you can also use Vmapper, keep all the panels in the same orientation
and use longer ribbon cables between the panels.
<img width="1232" height="927" alt="image" src="https://github.com/user-attachments/assets/c7986117-9071-48da-be30-0c628f576ac9" />


### Standard mappers
There is an option `--led-pixel-mapper` that allows you to choose between
some re-mapping options, and also programmatic ways to do so.

#### U-mapper (U-shape connection)
Say you have 4 displays with 32x32 and only a single output
like with a Raspberry Pi 1 or the Adafruit HAT -- if we chain
them, we get a display 32 pixel high, (4*32)=128 pixel long. If we arrange
the boards in a U-shape so that they form a square, we get a logical display
of 64x64 pixels:

<img src="../img/chained-64x64.jpg" width="400px"> 

In action:
[![PixelPusher video][pp-vid]](http://youtu.be/ZglGuMaKvpY)

```
So the following chain (Viewed looking at the LED-side of the panels)
    [<][<][<][<] }- Raspbery Pi connector

is arranged in this U-shape (on its side)
    [<][<] }----- Raspberry Pi connector
    [>][>]
```

Now we need to internally map pixels the pixels so that the 'folded' 128x32
screen behaves like a 64x64 screen.

There is a pixel-mapper that can help with this "U-Arrangement", you choose
it with `--led-pixel-mapper=U-mapper`. So in this particular case,

```
  ./demo --led-chain=4 --led-pixel-mapper="U-mapper"
```

This works for longer and more than one chain as well. Here an arrangement with
two chains with 8 panels each

```
   [<][<][<][<]  }--- Pi connector #1
   [>][>][>][>]
   [<][<][<][<]  }--- Pi connector #2
   [>][>][>][>]
```

(`--led-chain=8 --led-parallel=2 --led-pixel-mapper="U-mapper"`).

#### V-mapper and Vmapper:Z (Vertical arrangement)

By default, when you add panels on a chain, they are added horizontally.
If you have 2 panels of 64x32, you get 128x32.
The V-mapper allows the stacking to be vertical and not horizontal and
get the 64x64 you might want.

By default, all the panels are correct side up, and you need more cable length
as you need to cross back to the start of the next panel.
If you wish to use shorter cables, you can add use Vmapper:Z which will give
you serpentine cabling and every other panel will be upside down (see below
for an example).

It is compatible with parallel chains, so you can have multiple stacks
of panels all building a coherent overall display.

Here an example with 3 chains of 4 panels (128x64) for a total of about
98k display pixels.

```
  ./demo --led-rows=64 --led-cols=128 --led-chain=4 -led-parallel=3 --led-pixel-mapper=V-mapper -D0
```

Viewed looking the LED-side of the panels:

```
         Vmapper                             Vmapper:Z

  [O < I] [O < I] [O < I]             [I > O] [I > O] [I > O]
   ,---^   ,---^   ,---^               ^       ^       ^
  [O < I] [O < I] [O < I]             [O < I] [O < I] [O < I]
   ,---^   ,---^   ,---^                   ^       ^       ^
  [O < I] [O < I] [O < I]             [I > O] [I > O] [I > O]
   ,---^   ,---^   ,---^               ^       ^       ^
  [O < I] [O < I] [O < I]             [O < I] [O < I] [O < I]
       ^       ^       ^                   ^       ^       ^
      #1      #2       #3                 #1      #2       #3
         Pi connector (three parallel chains of len 4)
```

 (This is also a good time to notice that 384x256 with 12 128x64 panels, is probably an
upper limit of what you can reasonably output without having an unusable fresh
rate (Try these options to help: --led-pwm-bits=7 --led-pwm-dither-bits=1 and get about 100Hz)).

This shows the wiring of a 3x5 Vmapper:Z array built by Marc MERLIN, using 15x 64x32 panels:
![Vmapper_Z_192x160_3x5.jpg](../img/Vmapper_Z_192x160_3x5.jpg)
With --led-pwm-bits=7 --led-pwm-dither-bits=1, it gets a better 300Hz refresh
but only offers around 31K pixels instead of 98K pixels in the previous example.

Please note that Vmapper can also be used to improve the refresh rate of a long
display even if it is only one panel high (e.g. for a text running output) by
splitting the load into multiple parallel chains.

```

  [O < I] [O < I] [O < I]
       ^       ^       ^
      #1      #2       #3 Pi connector (three parallel chains of len 1)
```

#### Rotate

The "Rotate" mapper allows you to rotate your screen. It takes an angle
as parameter after a colon:

```
  ./demo --led-pixel-mapper="Rotate:90"
```

#### Mirror

The 'Mirror' mapper allows to mirror the output horizontally or vertically.
Without parameter, it mirrors horizontally. The parameter is a single character
'H' or 'V' for horizontal or vertical mirroring.

```
  ./demo --led-pixel-mapper="Mirror:H"
```

#### StackToRow

The `StackToRow` mapper allows parallel strands to subsequently be mapped end-to-end.

After using a combination of the other mappers, you may end up with a virtual
layout similar to the following, where x and y increase to the right and down:
```
[ STRAND #1 ]
[ STRAND #2 ]
[ STRAND #3 ]
[ STRAND #4 ]
```
The mapper can then be used to form the final end-to-end arrangement.

The standard usage (`StackToRow`) is mapped to:
```
[ STRAND #1 ][ STRAND #2 ][ STRAND #3 ][ STRAND #4 ]
```

The serpentine usage (`StackToRow:Z`) is mapped to:
```
[ STRAND #1 ][ 2# ꓷNꓯꓤꓕS ][ STRAND #3 ][ ᔭ# ꓷNꓯꓤꓕS ]
```

The flip usage (`StackToRow:F`) is mapped to:
```
[ STRAND #1 ][ STRAND #2 ][ Ɛ# ꓷNꓯꓤꓕS ][ ᔭ# ꓷNꓯꓤꓕS ]
```

The modifiers can be combined if necessary.

##### Example layout

Using 64x32px panels, the following arrangement is a 64x256px display
(visualized from the LED-side):
```
 ⌈I⌉\ ⌈I⌉\ ⌈I⌉\ ⌈I⌉<#1 |    ⌈O⌉\ ⌈O⌉\ ⌈O⌉\ ⌈O⌉
 ⌊O⌋ \⌊O⌋ \⌊O⌋ \⌊O⌋    | #2>⌊I⌋ \⌊I⌋ \⌊I⌋ \⌊I⌋
```

This layout can be configured with:
```
V-mapper;Rotate:90;StackToRow:F;Rotate:180
```
The remap mapper can be used to describe this layout:
```
Remap:256,64|96,0w|64,0w|32,0w|0,0w|128,0e|160,0e|192,0e|224,0e
```

#### Combining Mappers

You can chain multiple mappers in the configuration, by separating them
with a semicolon. The mappers are applied in the sequence you give them, so
if you want to arrange a couple of panels with the U-arrangement, and then
rotate the resulting screen, use

```
  ./demo --led-chain=8 --led-parallel=3 --led-pixel-mapper="U-mapper;Rotate:90"
```

Here, we first create a 128x192 screen (4 panels wide (`4*32=128`),
with three folded chains (`6*32=192`)) and then rotate it by 90 degrees to
get a 192x128 screen.

#### Programmatic access

If you want to choose these mappers programmatically from your program and
not via the flags, you can do this by setting the `pixel_mapper_config` option
in the options struct in C++ or Python.

```
  options.pixel_mapper_config = "Rotate:90";
```

### Feature remap mapper (experimental)

Please look at https://github.com/hzeller/rpi-rgb-led-matrix/pull/1478 
This is a placeholder until we have more documentation.

each segment is mapped to arbitrary position and orientation on canvas

My use case is two separate displays connected as chains, each with different geometry. But it shall handle almost all possible panel configuration

Syntax is:
```
--led-pixel-mapper='Remap:<new_width>,<new_height>|<panel0_x>,<panel0_y><panel0_orientation>|<panel1_x>,<panel1_y><panel1_orientation>'
```
new_width, new_height - size of created canvas. It can be both larger and smaller than old canvas.
panel0_x, panel0_y - upper-left corner of remapped panel
panel0_orientation - `n`,`s`,`e`,`w` for panel orientation, `x` to discard panel

Exactly one entry must be specified for each LED panel (chain * parallel entries). 
Mapping may be partially outside new canvas (maybe useful for something?)
Unused positions must be discarded (`0,0x`) - useful if chains are of different length
It is possible let some canvas space unused (no panel mapped to it). Writes to this area will be ignored.

For example:
```
 --led-cols=16 --led-rows=8 --led-chain=5 --led-parallel=2 --led-pixel-mapper='Remap:40,32|0,0e|8,0e|16,0e|24,0e|32,0e|0,16n|16,16n|0,24n|16,24n|0,0x'
```
First chain is 5 panels in line, top of the panel is pointing right, 40x16 pixels
Second chain is 2x2 square, panels pointing up, 32x16 pixels, top-left is 0,16 in canvas
Last panel on second chain is not used
```
1 2 3 4 5
1 2 3 4 5
6 6 7 7 
8 8 9 9
```

<img width="400" height="587" alt="image" src="https://github.com/user-attachments/assets/8e50dfba-cb2c-4554-9038-73b5bb19fcfd" />

```
That's how it should work. You need to map all panels (end of chain 3), so you must discard last 6 panels:
 --led-pixel-mapper='Remap:192,128|0,0s|0,32s|...|112,32n|112,0n|0,0x|0,0x|0,0x|0,0x|0,0x|0,0x'
```

### Writing your own mappers

If you want to write your own mappers, e.g. if you have a fancy panel
arrangement, you can do so using the API provided.

In the API, there is an interface to implement,
a [`PixelMapper`](../include/pixel-mapper.h) that allows to program
re-arrangements of pixels in any way. You can plug such an implementation of
a `PixelMapper` into the RGBMatrix to use it:

```
  bool RGBMatrix::ApplyPixelMapper(const PixelMapper *mapper);
```

If you want, you can also register your PixelMapper globally before you
parse the command line options; then this pixel-mapper is automatically
provided in the `--led-pixel-mapper` command line option:

```
   RegisterPixelMapper(new MyOwnPixelMapper());
   RGBMatrix *matrix = RGBMatrix::CreateFromFlags(...);
```

Now your mapper can be used alongside (and combined with) the standard
mappers already there (e.g. "U-mapper" or "Rotate"). Your mapper can have
parameters: In the command-line flag, parameters provided after `:` are passed
as-is to your `SetParameters()` implementation
(e.g. using `--led-pixel-mapper="Rotate:90"`, the `Rotate` mapper
gets a parameter string `"90"` as parameter).

#### Custom mapper example from barryVDs

Please see https://github.com/hzeller/rpi-rgb-led-matrix/issues/1732

![446766687-ef343583-fcf4-47f8-b784-abc83c458c7f](https://github.com/user-attachments/assets/f337e39b-9264-4e4d-bb97-f54686a6e086)



### Multiplex Mappers

Sometimes you even need this for the panel itself: In some panels
(typically the 'outdoor panels', often with 1:4 multiplexing) the pixels
are not mapped in a straight-forward way, but in a snake arrangement for
instance.

There are simplified pixel mappers for this purpose, the
[multiplex mappers](../lib/multiplex-mappers.cc). These are defined there
and then can be accessed via the command line flag `--led-multiplexing=...`.

If you find that whatever parameter you give to `--led-multiplexing=` doesn't
work, you might need to write your own mapper (extend `MultiplexMapperBase`
and implement the one method `MapSinglePanel()`). Then register them with
the `CreateMultiplexMapperList()` function in that file. When you do this,
this will automatically become available in the `--led-multiplexing=` command
line option in C++ and Python.

[run-vid]: ../img/running-vid.jpg
[git-submodules]: http://git-scm.com/book/en/Git-Tools-Submodules
[pixelpush]: https://github.com/hzeller/rpi-matrix-pixelpusher
[pp-vid]: ../img/pixelpusher-vid.jpg
[otf2bdf]: https://github.com/jirutka/otf2bdf
