#!/bin/sh

PLATFORM=vita

#make -C ../ -f Makefile.${PLATFORM}.salamander clean || exit 1
make -C ../ -f Makefile.griffin platform=${PLATFORM} clean || exit 1

#make -C ../ -f Makefile.${PLATFORM}.salamander || exit 1
#make -C ../ -f Makefile.${PLATFORM}.salamander pkg || exit 1

for f in *_vita.a ; do
   name=`echo "$f" | sed "s/\(_libretro_${PLATFORM}\|\).a$//"`
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
   cp -f "$f" ../libretro_${PLATFORM}.a
   make -C ../ -f Makefile.griffin platform=${PLATFORM} $whole_archive $big_stack -j3 || exit 1
   mv -f ../retroarch_${PLATFORM}.velf ../pkg/${PLATFORM}/${name}_libretro_${PLATFORM}.velf
   rm -f ../retroarch_${PLATFORM}.velf ../retroarch_${PLATFORM}.elf
done
