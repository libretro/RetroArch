#!/bin/sh

make -C ../ -f Makefile.ngc clean || exit 1

for f in *_ngc.a ; do
   name=`echo "$f" | sed 's/\(_libretro\|\)_ngc.a$//'`
   whole_archive=
   big_stack=
   if [ $name = "nxengine" ] ; then
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   fi
   if [ $name = "tyrquake" ] ; then
      big_stack="BIG_STACK=1"
   fi
   cp -f "$f" ../libretro_ngc.a
   make -C ../ -f Makefile.ngc $whole_archive $big_stack -j3 || exit 1
   mv -f ../retroarch_ngc.dol ../ngc/pkg/$name.dol
   rm -f ../retroarch_ngc.dol ../retroarch_ngc.elf ../retroarch_ngc.elf.map
done
