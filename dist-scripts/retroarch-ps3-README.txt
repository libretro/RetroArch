------------------------------------------------------------------------------
RETROARCH PS3 - 0.9.9
------------------------------------------------------------------------------
RetroConsole Level: 2
Author: Themaister, Squarepusher/Twin Aphex
Supports libretro GL: No
------------------------------------------------------------------------------

------------------------------------------------------------------------------
HOW TO INSTALL THIS
------------------------------------------------------------------------------

------------------------------------------------------------------------------
ON DEBUG (DEX) PS3
------------------------------------------------------------------------------
Put the PKG file 'retroarch-ps3-v0.9.9-dex.pkg' on your 
USB stick, put it in your PS3. Go to the PS3 XMB - go to the 
Game tab - Select 'Install Packages', and install the PKG file.

------------------------------------------------------------------------------
ON CFW P3
------------------------------------------------------------------------------
Put the PKG file 'retroarch-ps3-v0.9.9-cfw.pkg' on your 
USB stick. Put it in your CFW PS3. Go to the PS3 XMB - go to the 
Game tab - select 'Install Packages', and install the PKG file.

NOTE: The official release no longer supports Geohot/Wutangzra
CFWs. If you must use them, you have to do pkg_finalize on the PKG
to install it.

------------------------------------------------------------------------------
HOW TO USE THIS
------------------------------------------------------------------------------
On first startup, RetroArch will select one of the dozen or 
so emulator/game cores. The name of the core currently loaded will 
be shown at the top side of the screen.

You can now select a data file (ie. a game executable and/or a ROM) that
this core supports and load it in the Filebrowser.

To select a different core - go to 'Core' in the Main Menu. 
Select a core and then press X to switch to the emulator/game core.

------------------------------------------------------------------------------
INGAME CONTROLS
------------------------------------------------------------------------------
During ingame operation you can do some extra actions:

Right Thumb Stick - Down       - Fast-forwards the game
Right Thumb Stick - Up         - Rewinds the game in real-time 
                                 ('Rewind' has to be enabled in the 
                                'Settings' menu - warning - comes at a 
                                 performance decrease but will be worth it
                                 if you love this feature)
RStick Left + R2               - Decrease save state slot
Rtick Right + R2               - Increase save state slot
RStick Up + R2                 - Load selected save state slot
RStick Down + R2               - Save selected save state slot
L3 + R3                        - Go back to 'Menu'

------------------------------------------------------------------------------
WHAT IS RETROARCH?
------------------------------------------------------------------------------
RetroArch is a modular multi-system emulator system that is 
designed to be fast, lightweight and portable. It has features 
few other emulator frontends have, such as real-time rewinding 
and game-aware shading.

------------------------------------------------------------------------------
WHAT IS LIBRETRO?
------------------------------------------------------------------------------
Libretro is the API that RetroArch uses. It makes it easy to 
port games and emulators to a single core backend, such as 
RetroArch.

For the user, this means - more ports to play with, more 
crossplatform portability, less worrying about developers having 
to reinvent the wheel writing boilerplate UI/port code - so that 
they can get busy with writing the emulator/porting the emulator/game.

------------------------------------------------------------------------------
WHAT'S THE BIG DEAL?
------------------------------------------------------------------------------
Right now it's unique in that it runs the same emulator cores on 
multiple systems (such as Xbox 360, PS3, PC, Wii, Xbox 1, etc). 

For each emulator 'core', RetroArch makes use of a library API that 
we like to call 'libretro'.

Think of libretro as an interface for emulator and game ports. You 
can make a libretro port once and expect the same code to run on all 
the platforms that RetroArch supports. It's designed with simplicity 
and ease of use in mind so that the porter can worry about the port 
at hand instead of having to wrestle with an obfuscatory API.

The purpose of libretro is to help ease the work of the emulator/game 
porter by giving him an API that allows him to target multiple 
platforms at once without having to redo any code. He doesn't have 
to worry about writing input/video/audio drivers - all of that is 
supplied to him by RetroArch. All he has to do is to have the emulator 
port hook into the libretro API and that's it - we take care of the rest.

------------------------------------------------------------------------------
PLAYSTATION3 PORT
------------------------------------------------------------------------------
The PS3 port of RetroArch is one of the most developed console ports
of RetroArch. 

A couple of unique features RetroArch PS3 boasts that is not commonly
found anywhere else: 

- Game-aware shading in every emulator now (*)
- Real-time rewinding
- More shader features (motion blurring, etc)
- Switching between emulator cores seamlessly, and ability to install new 
  libretro cores

Included with RetroArch PS3 are a bunch of shaders - including the 
latest versions of the popular xBR shader. It is possible to use two 
shaders simultaneously to get the best possible graphical look.

* Check out Opium2k's manual shaders for Zelda 3 and others - you can 
find DLC packs for RetroArch at this site:

https://code.google.com/p/retro-arch/

------------------------------------------------------------------------------
EMULATOR/GAME CORES BUNDLED WITH PS3 PORT
------------------------------------------------------------------------------
The following emulators have been ported to RetroArch and are included 
in the PS3 release of RetroArch.

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
- Mednafen VB (Virtual Boy)

All of the emulators listed above are the latest versions currently 
available. Most of them have been specifically optimized so that they 
will run better on PS3 (some games would not reach fullspeed without these optimizations).

------------------------------------------------------------------------------
WHAT EXTENSIONS ARE SUPPORTED BY EACH CORE
------------------------------------------------------------------------------
- Prboom               wad
- Tyrquake             pak
- SNES9x Next          smc|fig|sfc|gd3|gd7|dx2|bsx|swc
- Genesis Plus GX      md|smd|bin|gen|bin|sms|gg|sg|cue
- NXEngine/Cave Story  exe
- VBA Next             gba
- FCEUmm               nes|unif
- NEStopia             nes|fds
- Gambatte             gb|gbc|dmg
- Final Burn Alpha     zip
- Mednafen PCE         pce|cue
- Mednafen Wonderswan  ws|wsc
- Mednafen NGP         ngp
- Mednafen VB          vb

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

- Remove retroarch.cfg from the 'SSNE100000' folder, then start up again. 
The Libretro management service in RetroArch should automatically pick a 
random libretro core and write this to the config file.

------------------------------------------------------------------------------
What can you expect in the future?
------------------------------------------------------------------------------
- Make the libxenon port release-worthy.
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
Opium2K DLC for RetroArch: https://code.google.com/p/retro-arch/
IRC:                       #retroarch (freenode)

------------------------------------------------------------------------------
