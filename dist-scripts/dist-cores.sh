#!/bin/sh

RARCH_VERSION=1.3.0
PLATFORM=$1
SALAMANDER=no
MAKEFILE_GRIFFIN=no

# PSP
if [ $PLATFORM = "psp1" ] ; then
platform=psp1
SALAMANDER=yes
EXT=a

mkdir -p ../pkg/${platform}/cores/

make -C ../bootstrap/${platform}/kernel_functions_prx/ clean || exit 1
make -C ../bootstrap/${platform}/kernel_functions_prx/ || exit 1
cp -f ../kernel_functions.prx ../pkg/${platform}/kernel_functions.prx

# Vita
elif [ $PLATFORM = "vita" ] ; then
platform=vita
MAKEFILE_GRIFFIN=yes
EXT=a

# CTR/3DS
elif [ $PLATFORM = "ctr" ] ; then
platform=ctr
EXT=a
mkdir -p ../pkg/3ds/cia
mkdir -p ../pkg/3ds/rom
mkdir -p ../pkg/3ds/3ds

# Emscripten
elif [ $PLATFORM = "emscripten" ] ; then
platform=emscripten
EXT=bc

if [ -z "$EMSCRIPTEN" ] ; then
   echo "run this script with emmake. Ex: emmake $0"
   exit 1
fi

# Wii
elif [ $PLATFORM = "wii" ] ; then
platform=wii
MAKEFILE_GRIFFIN=yes
SALAMANDER=yes
EXT=a

# NGC
elif [ $PLATFORM = "ngc" ] ; then
platform=ngc
MAKEFILE_GRIFFIN=yes
EXT=a

# DEX PS3
elif [ $PLATFORM = "dex-ps3" ] ; then
platform=ps3
SALAMANDER=yes
EXT=a

EXE_PATH=/usr/local/cell/host-win32/bin
MAKE_FSELF_NPDRM=$EXE_PATH/make_fself_npdrm.exe
MAKE_PACKAGE_NPDRM=$EXE_PATH/make_package_npdrm.exe

# CEX PS3
elif [ $PLATFORM = "cex-ps3" ]; then
platform=ps3
SALAMANDER=yes
EXT=a

EXE_PATH=/usr/local/cell/host-win32/bin

# ODE PS3
elif [ $PLATFORM = "ode-ps3" ]; then
#For this script to work correctly, you must place the "data" folder containing your ps3 keys for scetool to use in the dist-scripts folder.
platform=ps3
SALAMANDER=yes
EXT=a

GENPS3ISO_PATH=/cygdrive/c/Cobra_ODE_GenPS3iso_v2.3/genps3iso.exe
SCETOOL_PATH=/cygdrive/c/Users/aaa801/ps3tools/ps3tools/tools/scetool/scetool.exe
SCETOOL_FLAGS="--sce-type SELF --compress-data FALSE --self-type APP --key-revision 0004 --self-fw-version 0003004100000000 --self-app-version 0001000000000000 --self-auth-id 1010000001000003 --self-vendor-id 01000002 --self-cap-flags 00000000000000000000000000000000000000000000003b0000000100040000"
fi

# Cleanup Salamander core if it exists
if [ $SALAMANDER = "yes" ]; then
make -C ../ -f Makefile.${platform}.salamander clean || exit 1
fi

# Cleanup existing core if it exists
if [ $PLATFORM = "ode-ps3" ]; then
   make -C ../ -f Makefile.${platform}.cobra clean || exit 1
elif [ $MAKEFILE_GRIFFIN = "yes" ]; then
   make -C ../ -f Makefile.griffin platform=${platform} clean || exit 1
else
   make -C ../ -f Makefile.${platform} clean || exit 1
fi

# Compile Salamander core
if [ $SALAMANDER = "yes" ]; then
   make -C ../ -f Makefile.${platform}.salamander || exit 1
   if [ $PLATFORM = "psp1" ] ; then
   mv -f ../EBOOT.PBP ../pkg/${platform}/EBOOT.PBP
   fi
   if [ $PLATFORM = "wii" ] ; then
   mv -f ../retroarch-salamander_wii.dol ../pkg/${platform}/boot.dol
   fi
fi

for f in *_${platform}.${EXT} ; do
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).${EXT}$//"`
   lto=0
   whole_archive=
   big_stack=
   if [ $name = "nxengine" ] ; then
      echo "Applying whole archive linking..."
      whole_archive="WHOLE_ARCHIVE_LINK=1"
   elif [ $name = "tyrquake" ] ; then
      echo "Applying big stack..."
      lto=0
      big_stack="BIG_STACK=1"
   fi
   echo "-- Building core: $name --"
   cp -f "$f" ../libretro_${platform}.${EXT}

   # Do cleanup if this is a big stack core
   if [ "$big_stack" = "BIG_STACK=1" ] ; then
      if [ $MAKEFILE_GRIFFIN = "yes" ]; then
         make -C ../ -f Makefile.griffin platform=${platform} clean || exit 1
      elif [ $PLATFORM = "emscripten" ]; then
         make -C ../ -f Makefile.emscripten LTO=$lto -j7 clean || exit 1
      else
         make -C ../ -f Makefile.${platform} clean || exit 1
      fi
   fi

   # Compile core
   if [ $PLATFORM = "ode-ps3" ] ; then
      make -C ../ -f Makefile.${platform}.cobra $whole_archive -j3 || exit 1
   elif [ $MAKEFILE_GRIFFIN = "yes" ]; then
      make -C ../ -f Makefile.griffin platform=${platform} $whole_archive $big_stack -j3 || exit 1
   elif [ $PLATFORM = "emscripten" ]; then
      make -C ../ -f Makefile.emscripten LTO=$lto -j7 || exit 1
   elif [ $PLATFORM = "ctr" ]; then
      make -C ../ -f Makefile.${platform} LIBRETRO=$name $whole_archive $big_stack -j3 || exit 1
   else
      make -C ../ -f Makefile.${platform} $whole_archive $big_stack -j3 || exit 1
   fi

   # Do manual executable step
   if [ $PLATFORM = "dex-ps3" ] ; then
      $MAKE_FSELF_NPDRM ../retroarch_${platform}.elf ../CORE.SELF
   elif [ $PLATFORM = "cex-ps3" ] ; then
      make_self_wc ../retroarch_${platform}.elf ../CORE.SELF
   elif [ $PLATFORM = "ode-ps3" ] ; then
      $SCETOOL_PATH $SCETOOL_FLAGS --encrypt ../retroarch_${platform}.elf ../CORE.SELF
   fi

   # Move executable files
   if [ $platform = "ps3" ] ; then
      if [ $PLATFORM = "ode-ps3" ] ; then
         mv -f ../CORE.SELF ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/"${name}_libretro_${platform}.SELF"
      else
         mv -f ../CORE.SELF ../pkg/${platform}/USRDIR/cores/"${name}_libretro_${platform}.SELF"
      fi
   elif [ $PLATFORM = "psp1" ] ; then
      mv -f ../EBOOT.PBP ../pkg/${platform}/cores/${name}_libretro.PBP
   elif [ $PLATFORM = "vita" ] ; then
      mv -f ../retroarch_${platform}.velf ../pkg/${platform}/${name}_libretro_${platform}.velf
   elif [ $PLATFORM = "ctr" ] ; then
      mv -f ../retroarch_3ds.cia ../pkg/3ds/cia/${name}_libretro.cia
      mv -f ../retroarch_3ds.3ds ../pkg/3ds/rom/${name}_libretro.3ds
      mkdir -p ../pkg/3ds/3ds/${name}_libretro
      mv -f ../retroarch_3ds.3dsx ../pkg/3ds/3ds/${name}_libretro/${name}_libretro.3dsx
      mv -f ../retroarch_3ds.smdh ../pkg/3ds/3ds/${name}_libretro/${name}_libretro.smdh
      mv -f ../retroarch_3ds.xml  ../pkg/3ds/3ds/${name}_libretro/${name}_libretro.xml
   elif [ $PLATFORM = "ngc" ] ; then
      mv -f ../retroarch_${platform}.dol ../pkg/${platform}/${name}_libretro_${platform}.dol
   elif [ $PLATFORM = "wii" ] ; then
      mv -f ../retroarch_${platform}.dol ../pkg/${platform}/${name}_libretro_${platform}.dol
   elif [ $PLATFORM = "emscripten" ] ; then
      mv -f ../retroarch.js ../emscripten/$name.js
   fi

   # Remove executable files
   if [ $platform = "ps3" ] ; then
      rm -f ../retroarch_${platform}.elf ../retroarch_${platform}.self ../CORE.SELF
   elif [ $PLATFORM = "psp1" ] ; then
      rm -f ../retroarchpsp.elf
   elif [ $PLATFORM = "vita" ] ; then
      rm -f ../retroarch_${platform}.velf ../retroarch_${platform}.elf
   elif [ $PLATFORM = "ctr" ] ; then
      rm -f ../retroarch_3ds.elf
      rm -f ../retroarch_3ds.bnr
      rm -f ../retroarch_3ds.icn
   elif [ $PLATFORM = "ngc" ] ; then
      rm -f ../retroarch_${platform}.dol ../retroarch_${platform}.elf ../retroarch_${platform}.elf.map
   elif [ $PLATFORM = "wii" ] ; then
      rm -f ../retroarch_${platform}.dol ../retroarch_${platform}.elf ../retroarch_${platform}.elf.map
   elif [ $platform = "emscripten" ] ; then
      rm -f ../retroarch.js
   fi

   # Do cleanup if this is a big stack core
   if [ "$big_stack" = "BIG_STACK=1" ] ; then
      if [ $MAKEFILE_GRIFFIN = "yes" ]; then
         make -C ../ -f Makefile.griffin platform=${platform} clean || exit 1
      elif [ $PLATFORM = "emscripten" ]; then
         make -C ../ -f Makefile.emscripten LTO=$lto -j7 clean || exit 1
      else
         make -C ../ -f Makefile.${platform} clean || exit 1
      fi
   fi
done

# Additional build step
if [ $PLATFORM = "ps3" ] ; then
   make -C ../ -f Makefile.griffin platform=${platform} shaders-deploy
fi

# Packaging
if [ $PLATFORM = "dex-ps3" ] ; then
   $MAKE_FSELF_NPDRM ../retroarch-salamander_${platform}.elf ../pkg/${platform}/USRDIR/EBOOT.BIN
   rm -rf ../retroarch-salamander_${platform}.elf
   $MAKE_PACKAGE_NPDRM ../pkg/${platform}/package.conf ../pkg/${platform}
elif [ $PLATFORM = "cex-ps3" ] ; then
   make_self_wc ../retroarch-salamander_${platform}.elf ../pkg/${platform}/USRDIR/EBOOT.BIN
   rm -rf ../retroarch-salamander_${platform}.elf
   python2 ../ps3/ps3py/pkg.py --contentid UP0001-SSNE10000_00-0000000000000001 ../pkg/${platform} retroarch-${platform}-cfw-$RARCH_VERSION.pkg
elif [ $PLATFORM = "ode-ps3" ] ; then
   $SCETOOL_PATH $SCETOOL_FLAGS --encrypt ../retroarch-salamander_${platform}.elf ../pkg/${platform}_iso/PS3_GAME/USRDIR/EBOOT.BIN
   rm -rf ../retroarch-salamander_${platform}.elf

   $GENPS3ISO_PATH ../pkg/${platform}_iso/RetroArch-COBRA-ODE.iso
fi
