#!/bin/sh
RARCH_VERSION=1.2.2
platform=ps3

make -C ../ -f Makefile.${platform}.salamander clean || exit 1
make -C ../ -f Makefile.${platform} clean || exit 1

make -C ../ -f Makefile.${platform}.salamander || exit 1

EXE_PATH=/usr/local/cell/host-win32/bin

for f in *_${platform}.a ; do
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).a$//"`
   whole_archive=
   if [ $name = "nxengine" ] ; then
      echo "Applying whole archive linking..."
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   fi
   echo $name yes
   cp -f "$f" ../libretro_${platform}.a
   make -C ../ -f Makefile.${platform} $whole_archive -j3 || exit 1
   make_self_wc ../retroarch_${platform}.elf ../CORE.SELF
   mv -f ../CORE.SELF ../pkg/${platform}/USRDIR/cores/"${name}_libretro_${platform}.SELF"
   rm -f ../retroarch_${platform}.elf ../retroarch_${platform}.self ../CORE.SELF
done

make -C ../ -f Makefile.shaders deploy-ps3

make_self_wc ../retroarch-salamander_${platform}.elf ../pkg/${platform}/USRDIR/EBOOT.BIN
rm -rf ../retroarch-salamander_${platform}.elf
python2 ../ps3/ps3py/pkg.py --contentid UP0001-SSNE10000_00-0000000000000001 ../pkg/${platform} retroarch-${platform}-cfw-$RARCH_VERSION.pkg
