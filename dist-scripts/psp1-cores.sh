#!/bin/sh

mkdir -p ../psp1/pkg/cores/

make -C ../psp1/kernelFunctionsPrx/ clean || exit 1
make -C ../psp1/kernelFunctionsPrx/ || exit 1
cp -f ../kernel_functions.prx ../psp1/pkg/kernel_functions.prx

make -C ../ -f Makefile.psp1.salamander clean || exit 1
make -C ../ -f Makefile.psp1.salamander || exit 1
mv -f ../EBOOT.PBP ../psp1/pkg/EBOOT.PBP

make -C ../ -f Makefile.psp1 clean || exit 1

for f in *_psp1.a ; do
   name=`echo "$f" | sed 's/\(_libretro_psp1\|\).a$//'`
   whole_archive=
   big_stack=
   if [ $name = "nxengine" ] ; then
      echo "NXEngine found, applying whole archive linking..."
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   fi
   if [ $name = "tyrquake" ] ; then
      echo "Tyrquake found, applying big stack..."
      big_stack="BIG_STACK=1"
   fi
   cp -f "$f" ../libretro_psp1.a
   make -C ../ -f Makefile.psp1 $whole_archive $big_stack -j3 || exit 1
   mv -f ../EBOOT.PBP ../psp1/pkg/cores/${name}_libretro.PBP
   rm -f ../retroarchpsp.elf
done
