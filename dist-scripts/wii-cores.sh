#!/bin/sh

make -C ../ -f Makefile.wii clean || exit 1

for f in *_wii.a ; do
   name=`echo "$f" | sed 's/\(_libretro\|\)_wii.a$//'`
   cp -f "$f" ../libretro.a
   make -C ../ -f Makefile.wii -j3 || exit 1
   mv -f ../retroarch_wii.dol "$name.dol"
   rm -f ../retroarch_wii.dol ../retroarch_wii.elf ../retroarch_wii.elf.map
done
