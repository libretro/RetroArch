#!/bin/sh
#For this script to work correctly, you must place the "data" folder containing your ps3 keys for scetool to use in the dist-scripts folder.
PLATFORM=ps3

make -C ../ -f Makefile.${PLATFORM}.salamander clean || exit 1
make -C ../ -f Makefile.${PLATFORM}.cobra clean || exit 1

make -C ../ -f Makefile.${PLATFORM}.salamander || exit 1

GENPS3ISO_PATH=/cygdrive/c/Cobra_ODE_GenPS3iso_v2.3/genps3iso.exe 
SCETOOL_PATH=/cygdrive/c/Users/aaa801/ps3tools/ps3tools/tools/scetool/scetool.exe
SCETOOL_FLAGS="--sce-type SELF --compress-data FALSE --self-type APP --key-revision 0004 --self-fw-version 0003004100000000 --self-app-version 0001000000000000 --self-auth-id 1010000001000003 --self-vendor-id 01000002 --self-cap-flags 00000000000000000000000000000000000000000000003b0000000100040000"

for f in *_${PLATFORM}.a ; do
   name=`echo "$f" | sed "s/\(_libretro_${PLATFORM}\|\).a$//"`
   whole_archive=
   if [ $name = "nxengine" ] ; then
      echo "NXEngine found, applying whole archive linking..."
      whole_archive="WHOLE_ARCHIVE_LINK=1"
      echo $name yes
   fi
   cp -f "$f" ../libretro_${PLATFORM}.a
   make -C ../ -f Makefile.${PLATFORM}.cobra $whole_archive -j3 || exit 1
   $SCETOOL_PATH $SCETOOL_FLAGS --encrypt ../retroarch_${PLATFORM}.elf ../CORE.SELF
   mv -f ../CORE.SELF ../${PLATFORM}/iso/PS3_GAME/USRDIR/cores/"${name}_libretro_${PLATFORM}.SELF"
   rm -f ../retroarch_${PLATFORM}.elf ../retroarch_${PLATFORM}.self ../CORE.SELF
done

cp -r ../media/rmenu/*.png ../ps3/iso/PS3_GAME/USRDIR/cores/borders/Menu/

make -C ../ -f Makefile.griffin platform=${PLATFORM}-cobra shaders-deploy

$SCETOOL_PATH $SCETOOL_FLAGS --encrypt ../retroarch-salamander_${PLATFORM}.elf ../${PLATFORM}/iso/PS3_GAME/USRDIR/EBOOT.BIN
rm -rf ../retroarch-salamander_${PLATFORM}.elf

$GENPS3ISO_PATH ../${PLATFORM}/iso RetroArch-COBRA-ODE.iso
