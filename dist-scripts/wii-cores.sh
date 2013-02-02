#!/bin/sh

make -C ../ -f Makefile.wii.salamander clean || exit 1
make -C ../ -f Makefile.wii clean || exit 1

make -C ../ -f Makefile.wii.salamander || exit 1
make -C ../ -f Makefile.wii.salamander pkg || exit 1

for f in *_wii.a ; do
   name=`echo "$f" | sed 's/\(_libretro\|\)_wii.a$//'`
   cp -f "$f" ../libretro_wii.a
   make -C ../ -f Makefile.wii -j3 || exit 1
   mv -f ../retroarch_wii.dol ../wii/pkg/$name.dol
   rm -f ../retroarch_wii.dol ../retroarch_wii.elf ../retroarch_wii.elf.map
done
