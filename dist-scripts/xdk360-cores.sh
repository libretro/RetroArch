#!/bin/sh

#make -C ../ -f Makefile.wii.salamander clean || exit 1
#make -C ../ -f Makefile.wii clean || exit 1

#make -C ../ -f Makefile.wii.salamander || exit 1
#make -C ../ -f Makefile.wii.salamander pkg || exit 1
mkdir -p ../msvc/RetroArch-360/Debug
mkdir -p ../msvc/RetroArch-360/Release
mkdir -p ../msvc/RetroArch-360/Release_LTCG

for f in *_xdk360.lib ; do
   name=`echo "$f" | sed 's/\(_libretro\|\)_xdk360.lib$//'`
   echo $name
   cp -f "$f" ../msvc/RetroArch-360/Release_LTCG/libretro_xdk360.lib
   cmd.exe /k xdk360_env.bat $name
done
