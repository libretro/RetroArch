#!/bin/sh

platform=vita

#make -C ../ -f Makefile.${platform}.salamander clean || exit 1
make -C ../ -f Makefile.griffin platform=${platform} clean || exit 1

#make -C ../ -f Makefile.${platform}.salamander || exit 1
#make -C ../ -f Makefile.${platform}.salamander pkg || exit 1

for f in *_vita.a ; do
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).a$//"`
   whole_archive=
   big_stack=
   if [ $name = "nxengine" ] ; then
      echo "NXEngine found, applying whole archive linking..."
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   fi
   if [ $name = "tyrquake" ] ; then
      echo "Tyrquake found, applying big stack..."
      big_stack="BIG_STACK=1"
   fi
   cp -f "$f" ../libretro_${platform}.a
   make -C ../ -f Makefile.griffin platform=${platform} $whole_archive $big_stack -j3 || exit 1
   mv -f ../retroarch_${platform}.velf ../pkg/${platform}/${name}_libretro_${platform}.velf
   rm -f ../retroarch_${platform}.velf ../retroarch_${platform}.elf
done
