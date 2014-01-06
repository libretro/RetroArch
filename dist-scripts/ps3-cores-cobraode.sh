#!/bin/sh
#For this script to work correctly, you must place the "data" folder containing your ps3 keys for scetool to use in the dist-scripts folder.

make -C ../ -f Makefile.ps3.salamander clean || exit 1
make -C ../ -f Makefile.ps3.rgl clean || exit 1
make -C ../ -f Makefile.ps3.cobra clean || exit 1

make -C ../ -f Makefile.ps3.salamander || exit 1
make -C ../ -f Makefile.ps3.rgl || exit 1

GENPS3ISO_PATH=/cygdrive/c/Cobra_ODE_GenPS3iso_v2.3/genps3iso.exe 
SCETOOL_PATH=/cygdrive/c/Users/aaa801/ps3tools/ps3tools/tools/scetool/scetool.exe
SCETOOL_FLAGS="--sce-type SELF --compress-data FALSE --self-type APP --key-revision 0004 --self-fw-version 0003004100000000 --self-app-version 0001000000000000 --self-auth-id 1010000001000003 --self-vendor-id 01000002 --self-cap-flags 00000000000000000000000000000000000000000000003b0000000100040000"

for f in *_ps3.a ; do
   name=`echo "$f" | sed 's/\(_libretro\|\)_ps3.a$//'`
   whole_archive=
   if [ $name = "nxengine" ] ; then
      whole_archive="WHOLE_ARCHIVE_LINK=1"
      echo $name yes
   fi
   cp -f "$f" ../libretro_ps3.a
   make -C ../ -f Makefile.ps3.cobra $whole_archive -j3 || exit 1
   $SCETOOL_PATH $SCETOOL_FLAGS --encrypt ../retroarch_ps3.elf ../CORE.SELF
   mv -f ../CORE.SELF ../ps3/iso/PS3_GAME/USRDIR/cores/"${name}_libretro_ps3.SELF"
   rm -f ../retroarch_ps3.elf ../retroarch_ps3.self ../CORE.SELF
done

cp -r ../media/rmenu/*.png ../ps3/iso/PS3_GAME/USRDIR/cores/borders/Menu/

make -C ../ -f Makefile.shaders deploy-ps3-cobra

$SCETOOL_PATH $SCETOOL_FLAGS --encrypt ../retroarch-salamander_ps3.elf ../ps3/iso/PS3_GAME/USRDIR/EBOOT.BIN
rm -rf ../retroarch-salamander_ps3.elf

$GENPS3ISO_PATH ../ps3/iso RetroArch-COBRA-ODE.iso