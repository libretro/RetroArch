------------------------------------------------------------------------------
RETROARCH 360 - 0.9.8.4
------------------------------------------------------------------------------
RetroConsole Level: 2
Author: Themaister, Squarepusher/Twin Aphex
------------------------------------------------------------------------------

------------------------------------------------------------------------------
HOW TO INSTALL THIS
------------------------------------------------------------------------------
Copy the entire folder to your harddrive.

ROMs go into the 'roms' directory, or some subdirectory in the RetroArch-360 
directory.

------------------------------------------------------------------------------
HOW TO USE THIS
------------------------------------------------------------------------------
On first startup, RetroArch will select one of the dozen or so 
emulator/game cores. The name of the core currently loaded will be 
shown at the bottom side of the screen.

You can now select a ROM that this core supports and load it in the Filebrowser.

To select a different core - go to 'Change Libretro core' in the Main Menu. 
Press A to go to a filebrowser where you can select a different core. Press A 
to switch to the emulator/game core.

------------------------------------------------------------------------------
INGAME CONTROLS
------------------------------------------------------------------------------
During ingame operation you can do some extra actions:

Right Thumb Stick - Down         - Fast-forwards the game
Right Thumb Stick - Up           - Rewinds the game in real-time 
                                   ('Rewind' has to be enabled in the 'Settings' 
                                   screen - warning - comes at a slight 
                                   performance decrease but will be worth it 
                                   if you love this feature)
RStick Left + RT                 - Decrease save state slot
Rtick Right + RT                 - Increase save state slot
RStick Up + RT                   - Load selected save state slot
RStick Down + RT                 - Save selected save state slot
Right Thumb + Left Thumb         - Go back to 'Menu'/'Quick Menu'

------------------------------------------------------------------------------
WHAT IS RETROARCH?
------------------------------------------------------------------------------
RetroArch is a modular multi-system emulator system that is designed to 
be fast, lightweight and portable. It has features few other emulator 
frontends have, such as real-time rewinding and game-aware shading.

------------------------------------------------------------------------------
WHAT IS LIBRETRO?
------------------------------------------------------------------------------
Libretro is the API that RetroArch uses. It makes it easy to port games 
and emulators to a single core backend, such as RetroArch.

For the user, this means - more ports to play with, more crossplatform 
portability, less worrying about developers having to reinvent the wheel 
writing boilerplate UI/port code - so that they can get busy with writing 
the emulator/porting the emulator/game.

------------------------------------------------------------------------------
WHAT'S THE BIG DEAL?
------------------------------------------------------------------------------
Right now it's unique in that it runs the same emulator cores on 
multiple systems (such as Xbox 360, PS3, PC, Wii, etc). 

For each emulator 'core', RetroArch makes use of a library API that we 
like to call 'libretro'.

Think of libretro as an interface for emulator and game ports. You can 
make a libretro port once and expect the same code to run on all the 
platforms that RetroArch supports. It's designed with simplicity and 
ease of use in mind so that the porter can worry about the port at hand 
instead of having to wrestle with an obfuscatory API.

The purpose of libretro is to help ease the work of the emulator/game 
porter by giving him an API that allows him to target multiple platforms 
at once without having to redo any code. He doesn't have to worry about 
writing input/video/audio drivers - all of that is supplied to him by 
RetroArch. All he/she has to do is to have the emulator port hook 
into the libretro API and that's it - we take care of the rest.

------------------------------------------------------------------------------
XBOX 360 PORT
------------------------------------------------------------------------------
The Xbox 360 port of RetroArch has the following features:

- Real-time rewinding
- Switching between emulator cores seamlessly, and ability to install 
new libretro cores

Included with RetroArch 360 are a bunch of shaders - including the latest 
version of the popular xBR shader. It is possible to use two shaders 
simultaneously to get the best possible graphical look.

------------------------------------------------------------------------------
EMULATOR/GAME CORES BUNDLED WITH XBOX 360 PORT
------------------------------------------------------------------------------
The following emulators/games have been ported to RetroArch and are included in 
the Xbox 360 release of RetroArch.

For more information about them, see the included 
'retroarch-libretro-README.txt' file.

- Final Burn Alpha (Arcade - various) [version 0.2.97.28]
- FCEUmm (Nintendo Entertainment System) [recent SVN version]
- NEStopia (Nintendo Entertainment System) [1.44]
- Gambatte (Game Boy | Super Game Boy | Game Boy Color) [version 0.5.0 WIP]
- Genesis Plus GX (Sega SG-1000 | Master System | Game Gear | Genesis/Mega Drive |
  Sega CD) [version 1.7.3]
- SNES9x Next (Super Nintendo/Super Famicom) (v1.52.4)
- VBA Next (Game Boy Advance)
- Prboom (for playing Doom 1/Doom 2/Ultimate Doom/Final Doom)
- Mednafen PCE Fast (PC Engine/PC Engine CD/Turbografx 16)
- Mednafen Wonderswan (WonderSwan/WonderSwan Color/WonderSwan Crystal)
- Mednafen NGP (Neo Geo Pocket Color)

All of the emulators listed above are the latest versions currently 
available. Most of them have been specifically optimized so that 
they will run better on 360 (some games would not reach fullspeed 
without these optimizations).

------------------------------------------------------------------------------
WHAT EXTENSIONS ARE SUPPORTED BY EACH CORE
------------------------------------------------------------------------------
- Prboom               WAD|wad
- SNES9x Next          smc|fig|sfc|gd3|gd7|dx2|bsx|swc|zip|SMC|FIG|SFC|BSX|GD3|
                       GD7|DX2|SWC
- Genesis Plus GX      md|smd|bin|gen|zip|MD|SMD|bin|GEN|ZIP|sms|SMS|gg|GG|sg|SG|
                       cue|CUE
- VBA Next             GBA|gba
- FCEUmm               nes|NES|unif|UNIF
- NEStopia             nes|NES|fds|FDS
- Gambatte             gb|gbc|dmg|zip|GB|GBC|DMG|ZIP
- Final Burn Alpha     zip|ZIP
- Mednafen PCE         pce|PCE|cue|CUE
- Mednafen Wonderswan  ws|WS|wsc|WSC
- Mednafen NGP         ngp|NGP

------------------------------------------------------------------------------
ZIP SUPPORT
------------------------------------------------------------------------------
Selecting a ZIP file will temporarily unzip that file to the harddrive. The
temporary file will be deleted as soon as the game gets unloaded and/or when
you quit RetroArch.

NOTE: For the FBA core (and other cores that have 'block_extract' set to
true) - selecting a ZIP file from the Filebrowser will load that game 
directly.

------------------------------------------------------------------------------
Troubleshooting
------------------------------------------------------------------------------

If you find that RetroArch no longer works for whatever reason, there is
a way to get it back to work -

- Remove retroarch.cfg from the 'Retroarch-360' folder, then start up again.
The Libretro management service in RetroArch should automatically pick a 
random libretro core and write this to the config file.

------------------------------------------------------------------------------
What can you expect in the future?
------------------------------------------------------------------------------
- Make the libxenon port release-worthy.
- Add console-friendly features (nicely formatted names for FBA, some better
way to do core switching, etc)
- Fix NxEngine issues on consoles
- Finish up MAME 0.72 port
- Finish up ScummVM port
- Port of VICE to libretro
- More emulators, more games that will run on RetroArch
- Lots of other crazy ideas that might or might not pan out

------------------------------------------------------------------------------
Credits
------------------------------------------------------------------------------
- Mudlord for his Waterpaint/Noise shaders.
- Hyllian for the xBR shader.
- Opium2k for the nice manual shaders (bundled with PS3 release).
- Deank for assistance with RetroArch Salamander on CFW PS3s and 
  Multiman interoperability.
- FBA devs for adopting the libretro port.
- Ekeeke for help with the Genesis Plus GX port.
- ToadKing for having done a lot of work on RetroArch Wii.
- Freakdave for helping out with the Xbox 1 port.

------------------------------------------------------------------------------
Websites
------------------------------------------------------------------------------
Twitter:                   http://twitter.com/libretro
Source:                    http://github.com/libretro
Homepage:                  http://www.libretro.org
IRC:                       #retroarch (freenode)

------------------------------------------------------------------------------
