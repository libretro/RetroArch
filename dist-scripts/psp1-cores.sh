#!/bin/sh

platform=psp1

mkdir -p ../${platform}/pkg/cores/

make -C ../${platform}/kernelFunctionsPrx/ clean || exit 1
make -C ../${platform}/kernelFunctionsPrx/ || exit 1
cp -f ../kernel_functions.prx ../pkg/${platform}/kernel_functions.prx

make -C ../ -f Makefile.${platform}.salamander clean || exit 1
make -C ../ -f Makefile.${platform}.salamander || exit 1
mv -f ../EBOOT.PBP ../pkg/${platform}/EBOOT.PBP

make -C ../ -f Makefile.${platform} clean || exit 1

for f in *_${platform}.a ; do
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).a$//"`
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
      make -C ../ -f Makefile.${platform} clean || exit 1
   fi

   cp -f "$f" ../libretro_${platform}.a
   make -C ../ -f Makefile.${platform} $whole_archive $big_stack -j3 || exit 1
   mv -f ../EBOOT.PBP ../pkg/${platform}/cores/${name}_libretro.PBP
   rm -f ../retroarchpsp.elf

   if [ $big_stack="BIG_STACK=1" ] ; then
      make -C ../ -f Makefile.${platform} clean || exit 1
   fi
done
