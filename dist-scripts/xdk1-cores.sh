#!/bin/sh

PLATFORM=xdk

ip='192.168.1.10'

mkdir -p ../msvc/RetroArch-Xbox1/Debug
mkdir -p ../msvc/RetroArch-Xbox1/Release
mkdir -p ../msvc/RetroArch-Xbox1/Release_LTCG

for f in *_${PLATFORM}.lib ; do
   name=`echo "$f" | sed "s/\(_libretro_${PLATFORM}\|\).lib$//"`
   echo $name
   if [ $name = "tyrquake" ] || [ $name = "genesis_plus_gx" ] ; then
      echo "Applying whole archive linking for this core..."
   	cp -f "$f" ../msvc/RetroArch-Xbox1/Release_LTCG_BigStack/libretro_${PLATFORM}.lib
   	cmd.exe /k xdk1_env_bigstack.bat ${name}_libretro_xdk1
   else
   	cp -f "$f" ../msvc/RetroArch-Xbox1/Release_LTCG/libretro_${PLATFORM}.lib
   	cmd.exe /k xdk1_env.bat ${name}_libretro_xdk1
   fi
done
