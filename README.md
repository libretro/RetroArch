# SSNES

SSNES is a simple frontend for the libsnes library.

# libsnes

libsnes is the emulator core of [bSNES](http://www.byuu.org), the most accurate SNES emulator to date, in library form.
This enables the possibility of custom front-ends for the emulator.

# Philosophy

SSNES attempts to be very small and lean, while still having all the useful core features expected from an emulator. 
It is close in spirit to suckless' DWM, in that configuring the emulator requires a recompile. 
The configuration is done through editing a C header file. 
C programming skills are not necessary to configure it (no programming involved), but some basic programming experience might be needed.

# Dependencies

SSNES requires these libraries to build:

   - [libsnes](http://byuu.org/bsnes/)
   - GLFW
   - libsamplerate

SSNES can utilize these libraries if enabled:

   - nvidia-cg-toolkit

SSNES needs one of these audio driver libraries:

   - ALSA
   - OSS
   - RoarAudio
   - RSound
   - OpenAL

# Building libsnes

   - Download bSNES source (link over).
   - Add -fPIC to flags in Makefile.
   - <tt>$ make library profile=performance</tt> (accuracy, compatibility are the other profiles. compatibility is a great choice for decent PCs. :D)
   - <tt># make prefix=/usr library-install</tt>
   - <tt># cp snes/libsnes/libsnes.hpp /usr/include/</tt>
   - ?!?!
   - Profit!

# Configuring

SSNES configuring is done through editing <tt>config.h</tt> and <tt>config.mk</tt>.
The default configs can be found in <tt>config.h.def</tt> and <tt>config.mk.def</tt> respectively.
Do note that you might have to edit <tt>config.mk</tt> if you edit driver and filter options!
By default, ALSA audio driver is assumed.

Most options in <tt>config.h</tt> should be self-explanatory.
To configure joypads, start up <tt>jstest /dev/input/js0</tt> to determine which joypad buttons (and axis) to use.

# Compiling and installing

The good old <tt>make && sudo make install</tt> should do the trick :)


# Filters and Cg shader support

This is not strictly not necessary for an emulator, but it can be enabled if desired. 
For best performance, Cg shaders are recommended as they do not eat up valuable CPU time. 
Cg shaders are compiled at run-time, and shaders could be dropped in.
All shaders share a common interface to pass some essential arguments such as texture size and viewport size. (Common for pixel art scalers)
Some Cg shaders are included in hqflt/cg/ and could be used as an example.

