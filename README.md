# RetroArch

RetroArch is the reference frontend for the libretro API, an API which attempts to generalize
a retro gaming system, such as emulators and game engines.
Popular examples include SNES, NES, GameBoy, Arcade machines, Quake, DOOM, etc.
Emulator and game cores are instantiated as dynamic libraries.

## libretro

[libretro](http://libretro.com) is an API that exposes the core of a retro gaming system.
A frontend for libretro (such as RetroArch) handles video output, audio output, input and application lifecycle.
A libretro core written in portable C or C++ can run seamlessly on many platforms with very little/no porting effort.

While RetroArch is the reference frontend for libretro, several other projects have used the libretro
interface to include support for emulators and/or game engines. libretro is completely open and free for anyone to use.

[libretro API header](https://github.com/Themaister/RetroArch/blob/master/libretro.h)

## Binaries

Latest Windows binaries are currently hosted on Themaister's [homepage](http://themaister.net/retroarch.html).
Builds can also be found on the [forum](http://forum.themaister.net/).

## Support

To reach developers, either make an issue here on Github, make a thread on the [forum](http://forum.themaister.net/),
or visit our IRC channel: #retroarch @ irc.freenode.org.

## Documentation

See our [wiki](https://github.com/libretro/RetroArch/wiki). On Unix, man-pages are provided.
More developer-centric stuff is found [here](https://github.com/libretro/libretro.github.com/wiki/Documentation-devs).

## Related projects

   - Cg/HLSL shaders: [common-shaders](https://github.com/twinaphex/common-shaders)
   - More Cg shaders: [Emulator-Shader-Pack](https://github.com/Themaister/Emulator-Shader-Pack)
   - Helper scripts to build libretro implementations: [libretro-super](https://github.com/libretro/libretro-super)

## Philosophy

RetroArch attempts to be small and lean,
while still having all the useful core features expected from an emulator. 
It is designed to be very portable and features a gamepad-centric UI called RGUI.
It also has a full-featured command-line interface.

In some areas, RetroArch goes beyond and emphasizes on not-so-common technical features such as multi-pass shader support,
real-time rewind (Braid-style), FFmpeg video recording, etc.

RetroArch also emphasizes on being easy to integrate into various launcher frontends.

## Platforms

RetroArch has been ported to the following platforms outside PC:

   - PlayStation3
   - Xbox 360 (Libxenon/XeXDK)
   - Xbox 1
   - Wii, Gamecube (Libogc)
   - Raspberry Pi
   - Android
   - iOS
   - Blackberry

## Dependencies (PC)

On Windows, RetroArch can run with only Win32 as dependency. On Linux, you need:

   - GL headers
   - X11 headers and libs, or EGL/KMS/GBM

OSX port of RetroArch requires latest versions of XCode to build.

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
   - XAudio2 (Win32, Xbox 360)
   - DirectSound (Win32, Xbox 1)
   - CoreAudio (OSX, iOS)

To run properly, RetroArch requires a libretro implementation present, however, as it's typically loaded
dynamically, it's not required at build time.

## Dependencies (Console ports, mobile)

Console ports have their own dependencies, but generally do not require
anything other than what the respective SDKs provide.

## Configuring

The default configuration is defined in config.def.h.
It is not recommended to change this unless you know what you're doing.
These can later be tweaked by using a config file.
A sample configuration file is installed to /etc/retroarch.cfg. This is the system-wide config file. 

RetroArch will on startup create a config file in $XDG\_CONFIG\_HOME/retroarch/retroarch.cfg if doesn't exist.
Users only need to configure a certain option if the desired value deviates from the value defined in config.def.h.

To configure joypads, use RGUI or the <tt>retroarch-joyconfig</tt> command-line tool.

## Compiling and installing

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

You can add `pkg` as a parameter in order to make a PKG file - for example:

<tt>make -f Makefile.ps3 pkg</tt>

This creates an NPDRM package. This can be installed on debug PS3s.

To make a non-NPDRM package that can be installed on a jailbroken/CFW PS3 (such as PSGroove or PS3 CFWs and other 3.55 CFW derivatives), do:

<tt>make -f Makefile.ps3 pkg-signed</tt>

If you're using Kmeaw 3.55 firmware, the package needs to be signed:

<tt>make -f Makefile.ps3 pkg-signed-cfw</tt>

NOTE: A pre-existing libretro library needs to be present in the root directory in order to link RetroArch PS3. This file needs to be called <em><b>`libretro_ps3.a`</b></em>.

<b> Xbox 360 (XeXDK)</b><br />

You will need Microsoft Visual Studio 2010 installed (or higher) in order to compile RetroArch 360.

The solution file can be found at the following location:

<tt>msvc-360/RetroArch-360.sln</tt>

NOTE: A pre-existing libretro library needs to be present in the `msvc-360/RetroArch-360/Release` directory in order to link RetroArch 360. This file needs to be
called <em><b>`libretro_xdk360.lib`</b></em>.

<b> Xbox 360 (Libxenon)</b><br />

You will need to have the libxenon libraries and a working Devkit Xenon toolchain installed in order to compile RetroArch 360 Libxenon.

<tt>make -f Makefile.xenon</tt>

NOTE: A pre-existing libretro library needs to be present in the root directory in order to link RetroArch 360 Libxenon. This file needs to be called <em><b>`libretro_xenon360.a`</b></em>.

<b> Wii</b><br >

You will need to have the libogc libraries and a working Devkit PPC toolchain installed in order to compile RetroArch Wii.

<tt>make -f Makefile.wii</tt>

NOTE: A pre-existing libretro library needs to be present in the root directory in order to link RetroArch Wii. This file needs to be called <em><b>`libretro_wii.a`</b></em>.

