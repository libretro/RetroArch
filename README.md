# SSNES

SSNES is a simple frontend for the libsnes library.

# libsnes

libsnes is the emulator core of [bSNES](http://www.byuu.org), the most accurate SNES emulator to date, in library form. The libsnes API has been implemented by SNES9x as well, allowing multiple different cores to be used for an emulator that supports libsnes.
This enables the possibility of custom front-ends for the emulator.

# Binaries

Latest Windows binaries are currently hosted on my [homepage](http://themaister.net/ssnes.html).

# Philosophy

SSNES attempts to be very small and lean, while still having all the useful core features expected from an emulator. 
It is used through command-line.

# Platforms

SSNES has been ported to the following platforms :

   - PlayStation3
   - Xbox 360 (Libxenon/XeXDK)
   - Wii (Libogc)

# Dependencies

SSNES requires these libraries to build:

   - [libsnes](http://byuu.org/bsnes/)
   - SDL

SSNES can utilize these libraries if enabled:

   - nvidia-cg-toolkit
   - libxml2 (bSNES XML shaders)
   - libfreetype2 (TTF font rendering on screen)
   - libsamplerate

SSNES needs at least one of these audio driver libraries:

   - ALSA
   - OSS
   - RoarAudio
   - RSound
   - OpenAL
   - JACK
   - SDL
   - XAudio2 (Win32)
   - PulseAudio

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

To configure joypads, start up <tt>jstest /dev/input/js0</tt> to determine which joypad buttons (and axis) to use. It is also possible to use the <tt>ssnes-joyconfig</tt> tool as well for simple configuration.

# Compiling and installing

<b>Linux/Unix</b><br/>
As most packages, SSNES is built using the standard <tt>./configure && make && make install</tt>
Do note that the build system is not autotools based, but resembles it.

Notable options for ./configure: 
--with-libsnes=: Normally libsnes is located with -lsnes, however, this can be overridden.
--enable-dynamic: Do not link to libsnes at compile time, but load libsnes dynamically at runtime. libsnes\_path in config file defines which library to load. Useful for development.

Do note that these two options are mutually exclusive.

<b>Win32</b><br/>
It is possible with MinGW to compile for Windows in either msys or Linux/Unix based systems. Do note that Windows build uses a static Makefile since configuration scripts create more harm than good on this platform. Libraries, headers, etc, needed to compile and run SSNES can be fetched with a Makefile target.

In Linux/Unix:<br/>
<tt>make -f Makefile.win32 libs</tt></br>
<tt>make -f Makefile.win32 CC=i486-mingw32-gcc CXX=i486-mingw32-g++</tt></br>

In MSYS:
<tt>mingw32-make -f Makefile.win32 libs</tt>. # You will need to have wget in your patch for this command! MSYS should provide this.</br>
<tt>mingw32-make -f Makefile.win32</tt>

<b>Win32 (MSVC)</b><br />
In addition to Mingw, it is also possible to compile a Win32 version of SSNES with Microsoft Visual Studio 2010.

You will need Microsoft Visual Studio 2010 intalled (or higher) in order to compile SSNES with the MSVC compiler.

The solution file can be found at the following location:

<tt>msvc/SSNES/SSNES.sln</tt>

<b>PlayStation3</b><br/>

<tt>make -f Makefile.ps3</tt>

A PKG file will be built which you will be able to install on a jailbroken PS3.

NOTE: A pre-existing libsnes library needs to be present in the root directory in order to link SSNES PS3. This file needs to be called 'libsnes.a'.

<b> Xbox 360 (XeXDK)</b><br />

You will need Microsoft Visual Studio 2010 installed (or higher) in order to compile SSNES 360.

The solution file can be found at the following location:

<tt>msvc-360/SSNES-360/SSNES-360.sln</tt>

NOTE: A pre-existing libsnes library needs to be present in the root directory in order to link SSNES 360.

<b> Xbox 360 (Libxenon)</b><br />

You will need to have the libxenon libraries and a working Devkit Xenon toolchain installed in order to compile SSNES 360 Libxenon.

<tt>make -f Makefile.xenon</tt>

NOTE: A pre-existing libsnes library needs to be present in the root directory in order to link SSNES 360 Libxenon. This file needs to be called 'libsnes.a'.

<b> Wii</b><br >

You will need to have the libogc libraries and a working Devkit PPC toolchain installed in order to compile SSNES Wii.

<tt>make -f Makefile.wii</tt>

NOTE: A pre-existing libsnes library needs to be present in the root directory in order to link SSNES Wii. This file needs to be called 'libsnes.a'.

# Filters, bSNES XML shaders and Cg shader support

This is not strictly not necessary for an emulator, but it can be enabled if desired. 
For best performance, Cg shaders or bSNES XML shaders are recommended as they do not eat up valuable CPU time (assuming your GPU can handle the shaders). 
Cg shaders and XML shaders (GLSL) are compiled at run-time.
All shaders share a common interface to pass some essential arguments such as texture size and viewport size. (Common for pixel art scalers)
Some Cg shaders are included in hqflt/cg/ and could be used as an example.
bSNES XML shaders can be found on various places on the net. Best place to start looking are the bSNES forums.

The Cg shaders closely resemble the GLSL shaders found in bSNES shader pack, so porting them is trivial if desired.

