# What is Switchres 2.0
Switchres is a modeline generation engine for emulation.

Its purpose is on-the-fly creation of fully customized video modes that accurately reproduce those of the emulated systems. Based on a monitor profile, it will provide the best video mode for a given width, height, and refresh rate.

Switchres features the most versatile modeline generation ever, ranging from 15-kHz low resolutions up to modern 8K, with full geometry control, smart scaling, refresh scaling, mode rotation, aspect ratio correction and much more.

Switchres can be integrated into open-source emulators either as a library, or used as a standalone emulator launcher. It's written in C++ and a C wrapper is also available.

Switchres 2.0 is a rewrite of the original Switchres code used in GroovyMAME. It currently supports mode switching on the following platforms, with their respective backends:
  - **Windows**:
    - AMD ADL (AMD Radeon HD 5000+)
    - ATI legacy (ATI Radeon pre-HD 5000)
    - PowerStrip (ATI, Nvidia, Matrox, etc., models up to 2012)
  - **Linux**:
    - X11/Xorg
    - KMS/DRM (WIP)

Each platform supports a different feature set, being X11/Xorg the most performant currently. In general, AMD video cards offer the best compatibility, and are a real requirement for the Windows platform.

# Using Switchres as a library
If you are an emulator writer, you can integrate Switchres into your emulator in two ways:

- **Switchres shared library** (.dll or .so). This method offers a simplified way to add advanced mode switching features to your emulator, with minimal knowledge of Switchres internals.

- **Full Switchres integration**. If your emulator is written in C++, you can gain full access to Switchres' gears by including a Switchres manager class into your project, à la GroovyMAME.

Ask our devs for help and advice.

# Using Switchres standalone
The standalone binary supports the following options:
```
Usage: switchres <width> <height> <refresh> [options]
Options:
  -c, --calc                        Calculate video mode and exit
  -s, --switch                      Switch to video mode
  -l, --launch <command>            Launch <command>
  -m, --monitor <preset>            Monitor preset (generic_15, arcade_15, pal, ntsc, etc.)
  -a  --aspect <num:den>            Monitor aspect ratio
  -r  --rotated                     Original mode's native orientation is rotated
  -d, --display <OS_display_name>   Use target display (Windows: \\\\.\\DISPLAY1, ... Linux: VGA-0, ...)
  -f, --force <w>x<h>@<r>           Force a specific video mode from display mode list
  -i, --ini <file.ini>              Specify an ini file
  -b, --backend <api_name>          Specify the api name
  -k, --keep                        Keep changes on exit (warning: this disables cleanup)
```

A default `switchres.ini` file will be searched in the current working directory, then in `.\ini` on Windows, `./ini` then `/etc` on Linux. The repo has a switchres.ini example.

## Examples
`switchres 320 240 60 --calc` will calculate and show a modeline for 320x240@60, computed using the current monitor preset in `switchres.ini`.

`switchres 320 240 60 -m ntsc -s` will switch your primary screen to 320x240 at 60Hz using the ntsc monitor model. Then it will wait until you press enter, and restore your initial screen resolution on exit.

`switchres 384 256 55.017605 -m arcade_15 -s -d \\.\DISPLAY1 -l "mame rtype"` will switch your display #1 to 384x256@55.017605 using the arcade_15 preset, then launch ``mame rtype``. Once mame has exited, it will restore the original resolution.

`switchres 640 480 57 -d 0 -m arcade_15 -d 1 -m arcade_31 -s` will set 640x480@57i (15-kHz preset) on your first display (index #0), 640x480@57p (31-kHz preset) on your second display (index #1)

# License
GNU General Public License, version 2 or later (GPL-2.0+).
