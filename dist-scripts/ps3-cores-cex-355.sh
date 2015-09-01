#!/bin/sh
RARCH_VERSION=1.2.2
PLATFORM=ps3

make -C ../ -f Makefile.${PLATFORM}.salamander clean || exit 1
make -C ../ -f Makefile.${PLATFORM} clean || exit 1

make -C ../ -f Makefile.${PLATFORM}.salamander || exit 1

EXE_PATH=/usr/local/cell/host-win32/bin

for f in *_${PLATFORM}.a ; do
   name=`echo "$f" | sed "s/\(_libretro_${PLATFORM}\|\).a$//"`
   whole_archive=
   if [ $name = "nxengine" ] ; then
      echo "Applying whole archive linking..."
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   fi
   echo $name yes
   cp -f "$f" ../libretro_${PLATFORM}.a
   make -C ../ -f Makefile.${PLATFORM} $whole_archive -j3 || exit 1
   make_self_wc ../retroarch_${PLATFORM}.elf ../CORE.SELF
   mv -f ../CORE.SELF ../pkg/${PLATFORM}/USRDIR/cores/"${name}_libretro_${PLATFORM}.SELF"
   rm -f ../retroarch_${PLATFORM}.elf ../retroarch_${PLATFORM}.self ../CORE.SELF
done

make -C ../ -f Makefile.shaders deploy-ps3

make_self_wc ../retroarch-salamander_${PLATFORM}.elf ../pkg/${PLATFORM}/USRDIR/EBOOT.BIN
rm -rf ../retroarch-salamander_${PLATFORM}.elf
python2 ../ps3/ps3py/pkg.py --contentid UP0001-SSNE10000_00-0000000000000001 ../pkg/${PLATFORM} retroarch-${PLATFORM}-cfw-$RARCH_VERSION.pkg
