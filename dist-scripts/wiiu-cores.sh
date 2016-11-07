#!/bin/sh

RARCH_VERSION=1.3.6

platform=wiiu
EXT=a

mkdir -p ../pkg/wiiu/wiiu/apps/

make -C ../ -f Makefile.${platform} clean || exit 1

gen_meta_xml()
{
   info="$1"_libretro.info
   if [  -e $info ] ; then
      display_name=`cat $info | grep "display_name = " | sed "s/display_name = \"//" | sed s/\"//`
      corename=`cat $info | grep "corename = " | sed "s/corename = \"//" | sed s/\"//`
      authors=`cat $info | grep "authors = " | sed "s/authors = \"//" | sed s/\"// | sed s/\|/\ -\ /g`
      echo '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>' > "$libretro"_meta.xml
      echo '<app version="1">' >> "$libretro"_meta.xml
      echo '  <name>'$corename'</name>' >> "$libretro"_meta.xml
      echo '  <coder>'$authors'</coder>' >> "$libretro"_meta.xml
      echo '  <version>1.36</version>' >> "$libretro"_meta.xml
      echo '  <release_date>'`date +%Y%m%d%H%M%S`'</release_date>' >> "$libretro"_meta.xml
      echo '  <short_description>RetroArch</short_description>' >> "$libretro"_meta.xml
      echo '  <long_description>'$display_name'</long_description>' >> "$libretro"_meta.xml
      echo '</app>' >> "$libretro"_meta.xml
   fi
}

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
   make -C ../ -f Makefile.${platform} LIBRETRO=$name $whole_archive -j3 || exit 1
   mkdir -p ../pkg/wiiu/wiiu/apps/${name}_libretro
   mv -f ../retroarch_wiiu.elf ../pkg/wiiu/wiiu/apps/${name}_libretro/${name}_libretro.elf

   gen_meta_xml $name
   if [  -e $info ] ; then
      mv -f "$libretro"_meta.xml ../pkg/wiiu/wiiu/apps/${name}_libretro/meta.xml
   else
      cp -f ../pkg/wiiu/meta.xml ../pkg/wiiu/wiiu/apps/${name}_libretro/meta.xml
   fi
   if [  -e $name.png ] ; then
      cp -f $name.png ../pkg/wiiu/wiiu/apps/${name}_libretro/icon.png
   else
      cp -f ../pkg/wiiu/icon.png ../pkg/wiiu/wiiu/apps/${name}_libretro/icon.png
   fi
done

# Additional build step
