#!/bin/sh

# usage:
# copy core libs (*_libretro_wiiu.a) and assets (https://buildbot.libretro.com/assets/frontend/assets.zip) to this
# directory then run the script. the output will be in retroarch/pkg/wiiu

. ../version.all

platform=wiiu
EXT=a

retroarch_dir=../pkg/wiiu/retroarch
apps_dir=../pkg/wiiu/wiiu/apps
mkdir -p $retroarch_dir/cores $apps_dir

echo "-- Building launcher (Salamander) --"
make -C ../ -f Makefile.${platform} SALAMANDER_BUILD=1 clean || exit 1
make -C ../ -f Makefile.${platform} SALAMANDER_BUILD=1 -j$(nproc) || exit 1

if [ -e assets.zip ]; then
  # This is named "build" because we're building the assets, but it's also got a broad rule in the gitignore
  mkdir -p build/assets
  unzip -o assets.zip -d build/assets

  wuhbtool ../retroarch_wiiu_salamander.rpx $apps_dir/retroarch.wuhb \
    --name="RetroArch" \
    --short-name="RetroArch" \
    --author="libretro" \
    --icon=../pkg/wiiu/booticon.png \
    --tv-image=../pkg/wiiu/bootTvTex.tga \
    --drc-image=../pkg/wiiu/bootDrcTex.tga \
    --content=build/ || exit 1

  rm -rf build
fi

make -C ../ -f Makefile.${platform} clean || exit 1

for f in `ls -v *_${platform}.${EXT}`; do
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).${EXT}$//"`
   whole_archive=

   if [ $name = "nxengine" ] ; then
      echo "Applying whole archive linking..."
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   fi

   echo "-- Building core: $name --"
   cp -f "$f" ../libretro_${platform}.${EXT}
   echo NAME: $name

   # Compile core
   make -C ../ -f Makefile.${platform} LIBRETRO=$name $whole_archive -j$(nproc) || exit 1

   if [  -e ../retroarch_wiiu.rpx ] ; then
      cp ../retroarch_wiiu.rpx $retroarch_dir/cores/${name}_libretro.rpx
   fi
done

# Additional build step
