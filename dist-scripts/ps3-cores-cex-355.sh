#!/bin/sh

make -C ../ -f Makefile.ps3.salamander clean || exit 1
make -C ../ -f Makefile.ps3.rgl clean || exit 1
make -C ../ -f Makefile.ps3 clean || exit 1

make -C ../ -f Makefile.ps3.salamander || exit 1
make -C ../ -f Makefile.ps3.rgl || exit 1

EXE_PATH=/usr/local/cell/host-win32/bin

for f in *_ps3.a ; do
   name=`echo "$f" | sed 's/\(_libretro\|\)_ps3.a$//'`
   cp -f "$f" ../libretro_ps3.a
   make -C ../ -f Makefile.ps3 -j3 || exit 1
   make_self_wc ../retroarch_ps3.elf ../CORE.SELF
   mv -f ../CORE.SELF ../ps3/pkg/USRDIR/cores/"$name.SELF"
   rm -f ../retroarch_ps3.elf ../retroarch_ps3.self ../CORE.SELF
done

cp -r ../media/rmenu/*.png ../ps3/pkg/USRDIR/cores/borders/Menu/

make -C ../ -f Makefile.shaders deploy-ps3

make_self_wc ../retroarch-salamander_ps3.elf ../ps3/pkg/USRDIR/EBOOT.BIN
rm -rf ../retroarch-salamander_ps3.elf
python2 ../ps3/ps3py/pkg.py --contentid UP0001-SSNE10000_00-0000000000000001 ../ps3/pkg/ retroarch-ps3-cfw-0.9.8.1.pkg
