#!/bin/sh

platform=ps3

make -C ../ -f Makefile.${platform}.salamander clean || exit 1
make -C ../ -f Makefile.${platform} clean || exit 1

make -C ../ -f Makefile.${platform}.salamander || exit 1

EXE_PATH=/usr/local/cell/host-win32/bin
MAKE_FSELF_NPDRM=$EXE_PATH/make_fself_npdrm.exe
MAKE_PACKAGE_NPDRM=$EXE_PATH/make_package_npdrm.exe

for f in *_${platform}.a ; do
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).a$//"`
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
   cp -f "$f" ../libretro_${platform}.a
   make -C ../ -f Makefile.${platform} $whole_archive $big_stack -j3 || exit 1
   $MAKE_FSELF_NPDRM ../retroarch_${platform}.elf ../CORE.SELF
   mv -f ../CORE.SELF ../pkg/${platform}/USRDIR/cores/"${name}_libretro_${platform}.SELF"
   rm -f ../retroarch_${platform}.elf ../retroarch_${platform}.self ../CORE.SELF
done

cp -r ../media/rmenu/*.png ../pkg/${platform}/USRDIR/cores/borders/Menu/

make -C ../ -f Makefile.griffin platform=${platform} shaders-deploy

$MAKE_FSELF_NPDRM ../retroarch-salamander_${platform}.elf ../pkg/${platform}/USRDIR/EBOOT.BIN
rm -rf ../retroarch-salamander_${platform}.elf
$MAKE_PACKAGE_NPDRM ../pkg/${platform}/package.conf ../pkg/${platform}
