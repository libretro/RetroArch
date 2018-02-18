#!/bin/sh

platform=xdk360

mkdir -p ../pkg/msvc/RetroArch-360/Debug
mkdir -p ../pkg/msvc/RetroArch-360/Release
mkdir -p ../pkg/msvc/RetroArch-360/Release_LTCG

for f in *_${platform}.lib ; do
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).lib$//"`
   echo $name
   cp -f "$f" ../pkg/msvc/RetroArch-360/Release_LTCG/libretro_${platform}.lib
   cmd.exe /k ${platform}_env.bat ${name}_libretro_${platform}
done
