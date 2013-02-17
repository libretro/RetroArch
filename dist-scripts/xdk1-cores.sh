#!/bin/sh

ip='192.168.1.10'

#make -C ../ -f Makefile.wii.salamander clean || exit 1
#make -C ../ -f Makefile.wii clean || exit 1

#make -C ../ -f Makefile.wii.salamander || exit 1
#make -C ../ -f Makefile.wii.salamander pkg || exit 1
mkdir -p ../msvc/RetroArch-Xbox1/Debug
mkdir -p ../msvc/RetroArch-Xbox1/Release
mkdir -p ../msvc/RetroArch-Xbox1/Release_LTCG

for f in *_xdk.lib ; do
   name=`echo "$f" | sed 's/\(_libretro\|\)_xdk.lib$//'`
   echo $name
   cp -f "$f" ../msvc/RetroArch-Xbox1/Release_LTCG/libretro_xdk.lib
   cmd.exe /k xdk1_env.bat $name
done
