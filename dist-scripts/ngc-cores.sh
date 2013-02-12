#!/bin/sh

make -C ../ -f Makefile.ngc.salamander clean || exit 1
make -C ../ -f Makefile.ngc clean || exit 1

make -C ../ -f Makefile.ngc.salamander || exit 1
make -C ../ -f Makefile.ngc.salamander pkg || exit 1

for f in *_ngc.a ; do
   name=`echo "$f" | sed 's/\(_libretro\|\)_ngc.a$//'`
   whole_archive=
   if [ $name = "nxengine" ] ; then
      whole_archive="WHOLE_ARCHIVE_LINK=1"
      echo $name yes
   fi
   cp -f "$f" ../libretro_ngc.a
   make -C ../ -f Makefile.ngc $whole_archive -j3 || exit 1
   mv -f ../retroarch_ngc.dol ../ngc/pkg/$name.dol
   rm -f ../retroarch_ngc.dol ../retroarch_ngc.elf ../retroarch_ngc.elf.map
done
