# RetroArch

RetroArch (formerly known as SSNES) is a simple frontend for the libretro API. An API that attempts to generalize
a retro gaming system, such as SNES, NES, GameBoy, Arcade machines, etc.
Emulator/game cores are instantiated as loadable plugins.

# libretro

libretro is an API that exposes the core of a retro gaming system.
A frontend for libretro handles video output, audio output and input.
A libretro core written in portable C or C++ can run seamlessly on many platforms.

[libretro API header](https://github.com/Themaister/RetroArch/blob/master/libretro.h)

# Binaries

Latest Windows binaries are currently hosted on my [homepage](http://themaister.net/retroarch.html).

# Related projects

GUI frontend for PC: [RetroArch-Phoenix](https://github.com/Themaister/RetroArch-Phoenix)
Cg shaders: [common-shaders](https://github.com/twinaphex/common-shaders)
More Cg shaders: [Emulator-Shader-Pack](https://github.com/Themaister/Emulator-Shader-Pack)
Helper scripts to build libretro implementations: [libretro-super](https://github.com/Themaister/libretro-super)

# Philosophy

RetroArch attempts to be very small and lean,
while still having all the useful core features expected from an emulator. 
It is used through command-line. It is also designed to be portable.

# Platforms

RetroArch has been ported to the following platforms outside PC:

   - PlayStation3
   - Xbox 360 (Libxenon/XeXDK)
   - Wii (Libogc)

# Dependencies (PC)

RetroArch requires these libraries to build:

   - SDL

RetroArch can utilize these libraries if enabled:

   - nvidia-cg-toolkit
   - libxml2 (bSNES XML shaders)
   - libfreetype2 (TTF font rendering on screen)
   - libsamplerate

RetroArch needs at least one of these audio driver libraries:

   - ALSA
   - OSS
   - RoarAudio
   - RSound
   - OpenAL
   - JACK
   - SDL
   - XAudio2 (Win32)
   - PulseAudio

To run properly, RetroArch requires a libretro implementation present, however, as it's typically loaded
dynamically, it's not required at build time.

# Dependencies (Console ports)

Console ports have their own dependencies, but generally do not require
anything other than what the respective SDKs provide.

# Configuring

The default configuration is defined in config.def.h. 
These can later be tweaked by using a config file. 
A sample configuration file is installed to /etc/ssnes.cfg. 
This is the system-wide config file. 
Each user should create a config file in $XDG\_CONFIG\_HOME/ssnes/ssnes.cfg.
The users only need to configure a certain option if the desired value deviates from the value defined in config.def.h.

To configure joypads, start up <tt>jstest /dev/input/js0</tt> to determine which joypad buttons (and axis) to use.
It is also possible to use the <tt>ssnes-joyconfig</tt> tool as well for simple configuration.

# Compiling and installing

<b>Linux/Unix</b><br/>
As most packages, RetroArch is built using the standard <tt>./configure && make && make install</tt>
Do note that the build system is not autotools based, but resembles it. Refer to ./configure --help for options.

<b>Win32</b><br/>
It is possible with MinGW to compile for Windows in either msys or Linux/Unix based systems. Do note that Windows build uses a static Makefile since configuration scripts create more harm than good on this platform. Libraries, headers, etc, needed to compile and run RetroArch can be fetched with a Makefile target.

In Linux/Unix:<br/>
<tt>make -f Makefile.win libs</tt></br>
<tt>make -f Makefile.win CC=i486-mingw32-gcc CXX=i486-mingw32-g++</tt></br>

In MSYS:
<tt>mingw32-make -f Makefile.win libs</tt>. # You will need to have wget in your patch for this command! MSYS should provide this.</br>
<tt>mingw32-make -f Makefile.win</tt>

<b>Win32 (MSVC)</b><br />
In addition to Mingw, it is also possible to compile a Win32 version of RetroArch with Microsoft Visual Studio 2010.

You will need Microsoft Visual Studio 2010 intalled (or higher) in order to compile RetroArch with the MSVC compiler.

The solution file can be found at the following location:

<tt>msvc/RetroArch/RetroArch.sln</tt>

<b>PlayStation3</b><br/>

<tt>make -f Makefile.ps3</tt>

A PKG file will be built which you will be able to install on a jailbroken PS3.

NOTE: A pre-existing libretro library needs to be present in the root directory in order to link RetroArch PS3. This file needs to be called 'libretro.a'.

<b> Xbox 360 (XeXDK)</b><br />

You will need Microsoft Visual Studio 2010 installed (or higher) in order to compile RetroArch 360.

The solution file can be found at the following location:

<tt>msvc-360/RetroArch-360/RetroArch-360.sln</tt>

NOTE: A pre-existing libretro library needs to be present in the root directory in order to link RetroArch 360.

<b> Xbox 360 (Libxenon)</b><br />

You will need to have the libxenon libraries and a working Devkit Xenon toolchain installed in order to compile RetroArch 360 Libxenon.

<tt>make -f Makefile.xenon</tt>

NOTE: A pre-existing libretro library needs to be present in the root directory in order to link RetroArch 360 Libxenon. This file needs to be called 'libretro.a'.

<b> Wii</b><br >

You will need to have the libogc libraries and a working Devkit PPC toolchain installed in order to compile RetroArch Wii.

<tt>make -f Makefile.wii</tt>

NOTE: A pre-existing libretro library needs to be present in the root directory in order to link RetroArch Wii. This file needs to be called 'libretro.a'.

