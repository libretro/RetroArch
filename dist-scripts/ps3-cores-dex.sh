#!/bin/sh

PLATFORM=ps3

make -C ../ -f Makefile.${PLATFORM}.salamander clean || exit 1
make -C ../ -f Makefile.${PLATFORM} clean || exit 1

make -C ../ -f Makefile.${PLATFORM}.salamander || exit 1

EXE_PATH=/usr/local/cell/host-win32/bin
MAKE_FSELF_NPDRM=$EXE_PATH/make_fself_npdrm.exe
MAKE_PACKAGE_NPDRM=$EXE_PATH/make_package_npdrm.exe

for f in *_${PLATFORM}.a ; do
   name=`echo "$f" | sed "s/\(_libretro_${PLATFORM}\|\).a$//"`
   whole_archive=
   if [ $name = "nxengine" ] ; then
      echo "Applying whole archive linking..."
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   fi
   if [ $name = "tyrquake" ] ; then
      echo "Applying big stack..."
      big_stack="BIG_STACK=1"
   fi
   echo "-- Building core: $name --"
   cp -f "$f" ../libretro_${PLATFORM}.a
   make -C ../ -f Makefile.${PLATFORM} $whole_archive $big_stack -j3 || exit 1
   $MAKE_FSELF_NPDRM ../retroarch_${PLATFORM}.elf ../CORE.SELF
   mv -f ../CORE.SELF ../pkg/${PLATFORM}/USRDIR/cores/"${name}_libretro_${PLATFORM}.SELF"
   rm -f ../retroarch_${PLATFORM}.elf ../retroarch_${PLATFORM}.self ../CORE.SELF
done

cp -r ../media/rmenu/*.png ../pkg/${PLATFORM}/USRDIR/cores/borders/Menu/

make -C ../ -f Makefile.griffin platform=${PLATFORM} shaders-deploy

$MAKE_FSELF_NPDRM ../retroarch-salamander_${PLATFORM}.elf ../pkg/${PLATFORM}/USRDIR/EBOOT.BIN
rm -rf ../retroarch-salamander_${PLATFORM}.elf
$MAKE_PACKAGE_NPDRM ../pkg/${PLATFORM}/package.conf ../pkg/${PLATFORM}
