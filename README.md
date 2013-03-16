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

   - GUI frontend for PC: [RetroArch-Phoenix](https://github.com/Themaister/RetroArch-Phoenix)
   - Cg/HLSL shaders: [common-shaders](https://github.com/twinaphex/common-shaders)
   - More Cg shaders: [Emulator-Shader-Pack](https://github.com/Themaister/Emulator-Shader-Pack)
   - Helper scripts to build libretro implementations: [libretro-super](https://github.com/libretro/libretro-super)

# Philosophy

RetroArch attempts to be very small and lean,
while still having all the useful core features expected from an emulator. 
It is used through command-line. It is also designed to be portable.

# Platforms

RetroArch has been ported to the following platforms outside PC:

   - PlayStation3
   - Xbox 360 (Libxenon/XeXDK)
   - Xbox 1
   - Wii, Gamecube (Libogc)
   - Raspberry Pi
   - Android
   - iOS

# Dependencies (PC)

On Windows, RetroArch can run with only Win32 as dependency. On Linux, you need:

   - GL headers
   - X11 headers and libs, or EGL/KMS/GBM

OSX port of RetroArch still requires SDL 1.2 libraries.

RetroArch can utilize these libraries if enabled:

   - nvidia-cg-toolkit
   - libxml2 (GLSL XML shaders)
   - libfreetype2 (TTF font rendering on screen)

RetroArch needs at least one of these audio driver libraries:

   - ALSA
   - OSS
   - RoarAudio
   - RSound
   - OpenAL
   - JACK
   - SDL
   - PulseAudio
   - XAudio2 (Win32)
   - DirectSound (Win32)
   - CoreAudio (OSX, iOS)

To run properly, RetroArch requires a libretro implementation present, however, as it's typically loaded
dynamically, it's not required at build time.

# Dependencies (Console ports, mobile)

Console ports have their own dependencies, but generally do not require
anything other than what the respective SDKs provide.

# Configuring

The default configuration is defined in config.def.h. 
These can later be tweaked by using a config file. 
A sample configuration file is installed to /etc/retroarch.cfg. 
This is the system-wide config file. 
Each user should create a config file in $XDG\_CONFIG\_HOME/retroarch/retroarch.cfg.
The users only need to configure a certain option if the desired value deviates from the value defined in config.def.h.

To configure joypads, use the <tt>retroarch-joyconfig</tt> tool.
It is also possible to configure joypads using the RetroArch-Phoenix GUI frontend.

# Compiling and installing

<b>PC</b><br/>
Instructions for compiling on PC can be found in the [wiki](https://github.com/Themaister/RetroArch/wiki).

<b>PlayStation3</b><br/>

RetroArch PS3 needs to be compiled in the following order:

1) Compile RetroArch Salamander

<tt>make -f Makefile.ps3.salamander</tt>

2) Compile the RGL video driver

<tt>make -f Makefile.ps3.rgl</tt>

3) Compile RetroArch as a library

<tt>make -f Makefile.ps3.retroarch</tt>

4) Finally, compile RetroArch packed together with the GUI:

<tt>make -f Makefile.ps3</tt>

<b>PlayStation3 - Creating a PKG installable file</b><br />

You can add 'pkg' as a parameter in order to make a PKG file - for example:

<tt>make -f Makefile.ps3 pkg</tt>

This creates an NPDRM package. This can be installed on debug PS3s.

To make a non-NPDRM package that can be installed on a jailbroken/CFW PS3 (such as PSGroove or PS3 CFWs and other 3.55 CFW derivatives), do:

<tt>make -f Makefile.ps3 pkg-signed</tt>

If you're using Kmeaw 3.55 firmware, the package needs to be signed:

<tt>make -f Makefile.ps3 pkg-signed-cfw</tt>

NOTE: A pre-existing libretro library needs to be present in the root directory in order to link RetroArch PS3. This file needs to be called <em><b>'libretro_ps3.a'</b></em>.

<b> Xbox 360 (XeXDK)</b><br />

You will need Microsoft Visual Studio 2010 installed (or higher) in order to compile RetroArch 360.

The solution file can be found at the following location:

<tt>msvc-360/RetroArch-360.sln</tt>

NOTE: A pre-existing libretro library needs to be present in the 'msvc-360/RetroArch-360/Release' directory in order to link RetroArch 360. This file needs to be
called <em><b>'libretro_xdk360.lib'</b></em>.

<b> Xbox 360 (Libxenon)</b><br />

You will need to have the libxenon libraries and a working Devkit Xenon toolchain installed in order to compile RetroArch 360 Libxenon.

<tt>make -f Makefile.xenon</tt>

NOTE: A pre-existing libretro library needs to be present in the root directory in order to link RetroArch 360 Libxenon. This file needs to be called <em><b>'libretro_xenon360.a'</b></em>.

<b> Wii</b><br >

You will need to have the libogc libraries and a working Devkit PPC toolchain installed in order to compile RetroArch Wii.

<tt>make -f Makefile.wii</tt>

NOTE: A pre-existing libretro library needs to be present in the root directory in order to link RetroArch Wii. This file needs to be called <em><b>'libretro_wii.a'</b></em>.

