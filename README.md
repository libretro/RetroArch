[![Build Status](https://travis-ci.org/libretro/RetroArch.svg?branch=master)](https://travis-ci.org/libretro/RetroArch)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/8936/badge.svg)](https://scan.coverity.com/projects/retroarch)

# RetroArch

RetroArch is the reference frontend for the libretro API.
Popular examples of implementations for this API includes videogame system emulators and game engines, but also
more generalized 3D programs.
These programs are instantiated as dynamic libraries. We refer to these as "libretro cores".

![XMB menu driver](http://i.imgur.com/BMR1xxr.png "XMB menu driver")

![rgui menu driver](http://i.imgur.com/X3CbBKa.png "rgui menu driver")

![glui menu driver](http://i.imgur.com/ooqv8rw.png "glui menu driver")

## libretro

[libretro](http://libretro.com) is an API that exposes generic audio/video/input callbacks.
A frontend for libretro (such as RetroArch) handles video output, audio output, input and application lifecycle.
A libretro core written in portable C or C++ can run seamlessly on many platforms with very little/no porting effort.

While RetroArch is the reference frontend for libretro, several other projects have used the libretro
interface to include support for emulators and/or game engines. libretro is completely open and free for anyone to use.

[libretro API header](https://github.com/libretro/RetroArch/blob/master/libretro-common/include/libretro.h)

## Binaries

Latest Windows binaries are currently hosted on the [buildbot](http://buildbot.libretro.com/).

## Support

To reach developers, either make an issue here on GitHub, make a thread on the [forum](http://www.libretro.com/forums/),
or visit our IRC channel: #retroarch @ irc.freenode.org.

## Documentation

See our [Documentation Center](https://docs.libretro.com/). On Unix, man-pages are provided.
More developer-centric stuff is found [here](https://github.com/libretro/libretro.github.com/wiki/Documentation-devs).

## Related projects

   - Cg/HLSL shaders: [common-shaders](https://github.com/libretro/common-shaders)
   - slang shaders: [slang-shaders](https://github.com/libretro/slang-shaders)
   - GLSL shaders: [glsl-shaders](https://github.com/libretro/glsl-shaders)
   - Helper scripts to build libretro implementations: [libretro-super](https://github.com/libretro/libretro-super)
   - GitHub mirrors of projects, useful for generating diff files: [libretro-mirrors](https://github.com/libretro-mirrors/)

## Philosophy

RetroArch attempts to be small and lean,
while still having all the useful core features expected from an emulator.
It is designed to be very portable and features a gamepad-centric UI.
It also has a full-featured command-line interface.

In some areas, RetroArch goes beyond and emphasizes on not-so-common technical features such as multi-pass shader support,
real-time rewind (Braid-style), video recording (using FFmpeg), etc.

RetroArch also emphasizes on being easy to integrate into various launcher frontends.

## Platforms

RetroArch has been ported to the following platforms:

   - DOS
   - Windows
   - Linux
   - Emscripten
   - FreeBSD
   - NetBSD
   - OpenBSD
   - Haiku
   - Solaris
   - MacOS X
   - PlayStation 3
   - PlayStation Portable
   - PlayStation Vita
   - Original Microsoft Xbox
   - Microsoft Xbox 360 (Libxenon/XeXDK)
   - Nintendo Wii, GameCube (Libogc)
   - Nintendo WiiU
   - Nintendo 3DS
   - Nintendo Switch
   - Raspberry Pi
   - Android
   - iOS
   - Blackberry

## Dependencies (PC)

There are no true hard dependencies per se.

On Windows, RetroArch can run with only Win32 as dependency.

On Linux, there are no true dependencies. For optimal usage, the
following dependencies come as recommended:

   - GL headers / Vulkan headers
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

To configure joypads, use the built-in menu or the `retroarch-joyconfig` command-line tool.

## Compiling and installing

Instructions for compiling and installing RetroArch can be found in the [Libretro/RetroArch Documentation Center](https://docs.libretro.com/).
