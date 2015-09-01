#!/bin/sh

PLATFORM=xdk360

mkdir -p ../msvc/RetroArch-360/Debug
mkdir -p ../msvc/RetroArch-360/Release
mkdir -p ../msvc/RetroArch-360/Release_LTCG

for f in *_${PLATFORM}.lib ; do
   name=`echo "$f" | sed "s/\(_libretro_${PLATFORM}\|\).lib$//"`
   echo $name
   cp -f "$f" ../msvc/RetroArch-360/Release_LTCG/libretro_${PLATFORM}.lib
   cmd.exe /k ${PLATFORM}_env.bat ${name}_libretro_${PLATFORM}
done
