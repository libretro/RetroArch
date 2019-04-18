#!/bin/sh

# usage:
# copy core libs (*_libretro_wiiu.a), info files (https://github.com/libretro/libretro-super/tree/master/dist/info)
# and icons (https://github.com/libretro/retroarch-assets/tree/master/pkg/wiiu) to this directory then run
# the script. the output will be in retroarch/pkg/wiiu

. ../version.all

platform=wiiu
EXT=a

mkdir -p ../pkg/wiiu/retroarch/cores/info
cp *.info ../pkg/wiiu/retroarch/cores/info/
mkdir -p ../pkg/wiiu/rpx/retroarch/cores/info
cp *.info ../pkg/wiiu/rpx/retroarch/cores/info/

make -C ../ -f Makefile.${platform} SALAMANDER_BUILD=1 clean || exit 1
make -C ../ -f Makefile.${platform} SALAMANDER_BUILD=1 BUILD_HBL_ELF=1 BUILD_RPX=1 -j3 || exit 1

mkdir -p ../pkg/wiiu/wiiu/apps/retroarch
mv -f ../retroarch_wiiu_salamander.elf ../pkg/wiiu/wiiu/apps/retroarch/retroarch.elf
cp -f ../pkg/wiiu/meta.xml ../pkg/wiiu/wiiu/apps/retroarch/meta.xml
cp -f ../pkg/wiiu/icon.png ../pkg/wiiu/wiiu/apps/retroarch/icon.png
mkdir -p ../pkg/wiiu/rpx/wiiu/apps/retroarch
mv -f ../retroarch_wiiu_salamander.rpx ../pkg/wiiu/rpx/wiiu/apps/retroarch/retroarch.rpx
rm -f ../retroarch_wiiu_salamander.rpx.elf
cp -f ../pkg/wiiu/meta.xml ../pkg/wiiu/rpx/wiiu/apps/retroarch/meta.xml
cp -f ../pkg/wiiu/icon.png ../pkg/wiiu/rpx/wiiu/apps/retroarch/icon.png

make -C ../ -f Makefile.${platform} clean || exit 1

lookup()
{
   cat | grep "$1 = " | sed "s/$1 = \"//" | sed s/\"//
}

gen_meta_xml()
{
   info="$1"_libretro.info
   if [  -e $info ] ; then
      display_name=`cat $info | lookup "display_name"`
      corename=`cat $info | lookup "corename"`
      authors=`cat $info | lookup "authors" | sed s/\|/\ -\ /g`
      systemname=`cat $info | lookup "systemname"`
      license=`cat $info | lookup "license"`
      date=`date +%Y%m%d%H%M%S`
      build_hash=`git rev-parse --short HEAD 2>/dev/null`
      echo '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>' > "$1"_meta.xml
      echo '<app version="1">' >> "$1"_meta.xml
      echo '  <name>'$corename'</name>' >> "$1"_meta.xml
      echo '  <coder>'$authors'</coder>' >> "$1"_meta.xml
      echo '  <version>'$PACKAGE_VERSION' r'$build_hash'</version>' >> "$1"_meta.xml
      echo '  <release_date>'$date'</release_date>' >> "$1"_meta.xml
      echo '  <short_description>RetroArch</short_description>' >> "$1"_meta.xml
      echo -e '  <long_description>'$display_name'\n\nSystem: '$systemname'\nLicense: '$license'</long_description>' >> "$1"_meta.xml
      echo '  <category>emu</category>' >> "$1"_meta.xml
      echo '  <url>https://github.com/libretro</url>' >> "$1"_meta.xml
      echo '</app>' >> "$1"_meta.xml
   fi
}

for f in `ls -v *_${platform}.${EXT}`; do
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).${EXT}$//"`
   whole_archive=
   build_hbl_elf=1
   build_rpx=1

   if [ $name = "nxengine" ] ; then
      echo "Applying whole archive linking..."
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   fi

   if [ $name = "mame2003" ] ; then
      build_hbl_elf=0
   fi

   if [ $name = "fbalpha2012" ] ; then
      build_hbl_elf=0
   fi

   if [ $name = "mame2003_midway" ] ; then
      build_rpx=0
   fi
   if [ $name = "fbalpha2012_cps1" ] ; then
      build_rpx=0
   fi
   if [ $name = "fbalpha2012_cps2" ] ; then
      build_rpx=0
   fi
   if [ $name = "fbalpha2012_cps3" ] ; then
      build_rpx=0
   fi
   if [ $name = "fbalpha2012_neogeo" ] ; then
      build_rpx=0
   fi

   echo "-- Building core: $name --"
   cp -f "$f" ../libretro_${platform}.${EXT}
   echo NAME: $name

   # Compile core
   make -C ../ -f Makefile.${platform} LIBRETRO=$name BUILD_HBL_ELF=$build_hbl_elf BUILD_RPX=$build_rpx $whole_archive -j3 || exit 1
   gen_meta_xml $name

   if [  -e ../retroarch_wiiu.elf ] ; then
      cp ../retroarch_wiiu.elf ../pkg/wiiu/retroarch/cores/${name}_libretro.elf
      mkdir -p ../pkg/wiiu/wiiu/apps/${name}_libretro
      mv -f ../retroarch_wiiu.elf ../pkg/wiiu/wiiu/apps/${name}_libretro/${name}_libretro.elf
      if [  -e ${name}_meta.xml ] ; then
         cp -f ${name}_meta.xml ../pkg/wiiu/wiiu/apps/${name}_libretro/meta.xml
      else
         cp -f ../pkg/wiiu/meta.xml ../pkg/wiiu/wiiu/apps/${name}_libretro/meta.xml
      fi
      if [  -e $name.png ] ; then
         cp -f $name.png ../pkg/wiiu/wiiu/apps/${name}_libretro/icon.png
      else
         cp -f ../pkg/wiiu/icon.png ../pkg/wiiu/wiiu/apps/${name}_libretro/icon.png
      fi
   fi
   if [  -e ../retroarch_wiiu.rpx ] ; then
      cp ../retroarch_wiiu.rpx ../pkg/wiiu/rpx/retroarch/cores/${name}_libretro.rpx
      mkdir -p ../pkg/wiiu/rpx/wiiu/apps/${name}_libretro
      mv -f ../retroarch_wiiu.rpx ../pkg/wiiu/rpx/wiiu/apps/${name}_libretro/${name}_libretro.rpx
      rm -f ../retroarch_wiiu.rpx.elf
      if [  -e ${name}_meta.xml ] ; then
         cp -f ${name}_meta.xml ../pkg/wiiu/rpx/wiiu/apps/${name}_libretro/meta.xml
      else
         cp -f ../pkg/wiiu/meta.xml ../pkg/wiiu/rpx/wiiu/apps/${name}_libretro/meta.xml
      fi
      if [  -e $name.png ] ; then
         cp -f $name.png ../pkg/wiiu/rpx/wiiu/apps/${name}_libretro/icon.png
      else
         cp -f ../pkg/wiiu/icon.png ../pkg/wiiu/rpx/wiiu/apps/${name}_libretro/icon.png
      fi
   fi
   rm -rf ${name}_meta.xml

done

# Additional build step
