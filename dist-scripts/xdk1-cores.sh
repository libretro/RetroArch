#!/bin/sh

platform=xdk

ip='192.168.1.10'

mkdir -p ../msvc/RetroArch-Xbox1/Debug
mkdir -p ../msvc/RetroArch-Xbox1/Release
mkdir -p ../msvc/RetroArch-Xbox1/Release_LTCG

for f in *_${platform}.lib ; do
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).lib$//"`
   echo $name
   if [ $name = "tyrquake" ] || [ $name = "genesis_plus_gx" ] ; then
      echo "Applying whole archive linking for this core..."
   	cp -f "$f" ../msvc/RetroArch-Xbox1/Release_LTCG_BigStack/libretro_${platform}.lib
   	cmd.exe /k xdk1_env_bigstack.bat ${name}_libretro_xdk1
   else
   	cp -f "$f" ../msvc/RetroArch-Xbox1/Release_LTCG/libretro_${platform}.lib
   	cmd.exe /k xdk1_env.bat ${name}_libretro_xdk1
   fi
done
