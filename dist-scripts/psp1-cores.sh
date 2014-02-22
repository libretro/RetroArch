#!/bin/sh

#make -C ../ -f Makefile.wii.salamander clean || exit 1
make -C ../ -f Makefile.psp1 clean || exit 1

#make -C ../ -f Makefile.wii.salamander || exit 1
make -C ../ -f Makefile.psp1 || exit 1

for f in *_psp1.a ; do
   name=`echo "$f" | sed 's/\(_libretro\|\)_psp1.a$//'`
   whole_archive=
   big_stack=
   if [ $name = "nxengine" ] ; then
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   fi
   if [ $name = "tyrquake" ] ; then
      big_stack="BIG_STACK=1"
   fi
   cp -f "$f" ../libretro_psp1.a
   make -C ../ -f Makefile.psp1 $whole_archive $big_stack -j3 || exit 1
   mv -f ../retroarchpsp.prx ../psp1/pkg/${name}_libretro_psp1.prx
   rm -f ../retroarchpsp.prx ../retroarchpsp.elf
done
