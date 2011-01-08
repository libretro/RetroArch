# SSNES

SSNES is a simple frontend for the libsnes library.

# libsnes

libsnes is the emulator core of [bSNES](http://www.byuu.org), the most accurate SNES emulator to date, in library form.
This enables the possibility of custom front-ends for the emulator.

# Philosophy

SSNES attempts to be very small and lean, while still having all the useful core features expected from an emulator. 
It is used through command-line.

# Dependencies

SSNES requires these libraries to build:

   - [libsnes](http://byuu.org/bsnes/)
   - SDL
   - libsamplerate

SSNES can utilize these libraries if enabled:

   - nvidia-cg-toolkit
   - libxml2 (bSNES XML shaders)

SSNES needs one of these audio driver libraries:

   - ALSA
   - OSS
   - RoarAudio
   - RSound
   - OpenAL
   - JACK

# Building libsnes

   - Download bSNES source (link over).
   - Add -fPIC to flags in Makefile.
   - <tt>$ make library profile=performance</tt> (accuracy, compatibility are the other profiles. compatibility is a great choice for decent PCs. :D)
   - <tt># make prefix=/usr library-install</tt>
   - <tt># cp snes/libsnes/libsnes.hpp /usr/include/</tt>
   - ?!?!
   - Profit!

# Configuring

The default configuration is defined in config.def.h. 
These can later be tweaked by using the ssnes config file. 
A sample configuration file is installed to /etc/ssnes.cfg. 
This is the system-wide config file. 
Each user should create a config file in $XDG\_CONFIG\_HOME/ssnes/ssnes.cfg.
The users only need to configure a certain option if the desired value deviates from the value defined in config.def.h.

To configure joypads, start up <tt>jstest /dev/input/js0</tt> to determine which joypad buttons (and axis) to use.

# Compiling and installing

As most packages, SSNES is built using the standard <tt>./configure && make && make install</tt>
Do note that the build system is not autotools based, but resembles it.

Notable options for ./configure: 
--with-libsnes=: Normally libsnes is located with -lsnes, however, this can be overridden.
--enable-dynamic: Do not link to libsnes at compile time, but load libsnes dynamically at runtime. libsnes\_path in config file defines which library to load. Useful for development.

Do note that these two options are mutually exclusive.



# Filters, bSNES XML shaders and Cg shader support

This is not strictly not necessary for an emulator, but it can be enabled if desired. 
For best performance, Cg shaders or bSNES XML shaders are recommended as they do not eat up valuable CPU time (assuming your GPU can handle the shaders). 
Cg shaders and XML shaders (GLSL) are compiled at run-time.
All shaders share a common interface to pass some essential arguments such as texture size and viewport size. (Common for pixel art scalers)
Some Cg shaders are included in hqflt/cg/ and could be used as an example.
bSNES XML shaders can be found on various places on the net. Best place to start looking are the bSNES forums.

The Cg shaders closely resemble the GLSL shaders found in bSNES shader pack, so porting them is trivial if desired.

