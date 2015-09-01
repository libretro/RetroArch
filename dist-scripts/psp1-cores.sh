#!/bin/sh

PLATFORM=psp1

mkdir -p ../${PLATFORM}/pkg/cores/

make -C ../${PLATFORM}/kernelFunctionsPrx/ clean || exit 1
make -C ../${PLATFORM}/kernelFunctionsPrx/ || exit 1
cp -f ../kernel_functions.prx ../pkg/${PLATFORM}/kernel_functions.prx

make -C ../ -f Makefile.${PLATFORM}.salamander clean || exit 1
make -C ../ -f Makefile.${PLATFORM}.salamander || exit 1
mv -f ../EBOOT.PBP ../pkg/${PLATFORM}/EBOOT.PBP

make -C ../ -f Makefile.${PLATFORM} clean || exit 1

for f in *_${PLATFORM}.a ; do
   name=`echo "$f" | sed "s/\(_libretro_${PLATFORM}\|\).a$//"`
   whole_archive=
   big_stack=

   if [ $name = "nxengine" ] ; then
      echo "Applying whole archive linking..."
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   fi

   if [ $name = "tyrquake" ] ; then
      echo "Applying big stack..."
      big_stack="BIG_STACK=1"
   fi

   if [ $big_stack="BIG_STACK=1" ] ; then
      make -C ../ -f Makefile.${PLATFORM} clean || exit 1
   fi

   cp -f "$f" ../libretro_${PLATFORM}.a
   make -C ../ -f Makefile.${PLATFORM} $whole_archive $big_stack -j3 || exit 1
   mv -f ../EBOOT.PBP ../pkg/${PLATFORM}/cores/${name}_libretro.PBP
   rm -f ../retroarchpsp.elf

   if [ $big_stack="BIG_STACK=1" ] ; then
      make -C ../ -f Makefile.${PLATFORM} clean || exit 1
   fi
done
