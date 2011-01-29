===================================================
   SSNES - Simple Super Nintendo Emulator System
===================================================

SSNES is a simplistic emulator frontend designed for use without any GUI. 
It is used from command line, but is designed to play well with eventual GUI frontends, 
as the configuration file format is very simple.

This allows for great portability and focus on what matters in a frontend, such as video and audio performance.
Its core libraries for video and input are OpenGL and SDL, with many different audio library supported.

On first use you should get accustomed to how SSNES works.

-  SSNES emphasizes simplicity. It has no graphical user interface, and is primarily used through the command line.
   It is recommended for experienced users.
   It is possible though to create a GUI which can use SSNES.

-  SSNES isn't dependant on any emulator core, such as bSNES, SNES9x or ZSNES. 
   SSNES only talks to the libsnes interface, 
   which was created initially by bSNES but it has an implementation available for SNES9x as well.

-  SSNES is configured through a config file. In the Win32 distribution, there should be a ssnes.cfg included.
   This config file serves as a base for your custom configuration file.
   It is recommended that you go through the config file and alter any configurations you might use.
   This is a good way to learn about the features SSNES have.
   It is possible to pick which configuration file you want with the '-c' flag. 
   E.g.: ssnes.exe -c myconfig.cfg rom.sfc.
   
-  When unsure which command line flags to use, use the command: ssnes.exe --help.

-  When something is not working as expected, try using verbose logging first to see what is happening: 
   E.g.: ssnes.exe -v romfile.sfc
   If something is not working, and you want to file a bug report, 
   please include console output with the -v flag turned on.

-  SSNES does not support zipped roms directly. It only supports roms with extentions .sfc and .smc.
   However, SSNES can read roms from stdin, 
   which makes it possible to create wrapper programs that can support every zip format under the sun.
   This can also be performed by a GUI frontend.
