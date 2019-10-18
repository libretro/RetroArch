#!/bin/sh

. ../version.all
PLATFORM=$1
SALAMANDER=no
MAKEFILE_GRIFFIN=no

# PSP
if [ $PLATFORM = "unix" ] ; then
platform=unix
SALAMANDER=no
EXT=a

mkdir -p ../pkg/${platform}/

# For statically linked cores, we need to configure once
cd ..
LDFLAGS=-L. ./configure --disable-dynamic
cd dist-scripts

elif [ $PLATFORM = "ps2" ] ; then
platform=ps2
SALAMANDER=NO
EXT=a

mkdir -p ../pkg/${platform}/cores/

elif [ $PLATFORM = "psp1" ] ; then
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
SALAMANDER=yes
EXT=a
mkdir -p ../pkg/vita/vpk

# Nintendo Switch (libnx)
elif [ $PLATFORM = "libnx" ] ; then
platform=libnx
EXT=a

# CTR/3DS
elif [ $PLATFORM = "ctr" ] ; then
platform=ctr
SALAMANDER=yes
EXT=a
mkdir -p ../pkg/${platform}/build/cia
mkdir -p ../pkg/${platform}/build/3dsx
mkdir -p ../pkg/${platform}/build/rom

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
OPTS=DEX_BUILD=1

EXE_PATH=${CELL_SDK}/host-win32/bin
MAKE_FSELF_NPDRM=${EXE_PATH}/make_fself_npdrm.exe
MAKE_PACKAGE_NPDRM=${EXE_PATH}/make_package_npdrm.exe

# CEX PS3
elif [ $PLATFORM = "cex-ps3" ]; then
#For this script to work correctly, you must place scetool.exe and the "data" folder containing your ps3 keys for scetool to use in the dist-scripts folder.
platform=ps3
SALAMANDER=yes
EXT=a
OPTS=CEX_BUILD=1

EXE_PATH=${CELL_SDK}/host-win32/bin
SCETOOL_PATH=${PS3TOOLS_PATH}/scetool/scetool.exe
SCETOOL_FLAGS_CORE="--sce-type=SELF --compress-data=TRUE --skip-sections=TRUE --key-revision=04 --self-auth-id=1010000001000003 --self-vendor-id=01000002 --self-type=APP --self-app-version=0001000000000000 --self-fw-version=0003004100000000 --encrypt"
SCETOOL_FLAGS_EBOOT="--sce-type=SELF --compress-data=TRUE --skip-sections=TRUE --key-revision=04 --self-auth-id=1010000001000003 --self-vendor-id=01000002 --self-type=NPDRM --self-fw-version=0003004100000000 --np-license-type=FREE --np-content-id=UP0001-SSNE10000_00-0000000000000001 --np-app-type=EXEC --self-app-version=0001000000000000 --np-real-fname=EBOOT.BIN --encrypt"

cp -rfv ${PS3TOOLS_PATH}/scetool/data .

# ODE PS3
elif [ $PLATFORM = "ode-ps3" ]; then
#For this script to work correctly, you must place scetool.exe and the "data" folder containing your ps3 keys for scetool to use in the dist-scripts folder.
platform=ps3
SALAMANDER=yes
EXT=a
OPTS=ODE_BUILD=1

EXE_PATH=${CELL_SDK}/host-win32/bin
GENPS3ISO_PATH=${PS3TOOLS_PATH}/ODE/genps3iso_v2.5
SCETOOL_PATH=${PS3TOOLS_PATH}/scetool/scetool.exe
SCETOOL_FLAGS_ODE="--sce-type=SELF --compress-data=TRUE --self-type=APP --key-revision=04 --self-fw-version=0003004100000000 --self-app-version=0001000000000000 --self-auth-id=1010000001000003 --self-vendor-id=01000002 --self-cap-flags=00000000000000000000000000000000000000000000003b0000000100040000  --encrypt"
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
elif [ $PLATFORM = "unix" ]; then
   LINK=g++ make -C ../ -f Makefile clean || exit 1
else
   make -C ../ -f Makefile.${platform} clean || exit 1
fi

# Compile Salamander core
if [ $SALAMANDER = "yes" ]; then
   make -C ../ -f Makefile.${platform}.salamander $OPTS || exit 1
   if [ $PLATFORM = "psp1" ] ; then
   mv -f ../EBOOT.PBP ../pkg/${platform}/EBOOT.PBP
   fi
   if [ $PLATFORM = "vita" ] ; then
     mkdir -p ../pkg/${platform}/retroarch.vpk/vpk/sce_sys/livearea/contents
     vita-make-fself -c -s ../retroarchvita_salamander.velf ../pkg/${platform}/retroarch.vpk/vpk/eboot.bin
     vita-mksfoex -s TITLE_ID=RETROVITA "RetroArch" -d ATTRIBUTE2=12 ../pkg/${platform}/retroarch.vpk/vpk/sce_sys/param.sfo
     cp ../pkg/${platform}/assets/ICON0.PNG ../pkg/${platform}/retroarch.vpk/vpk/sce_sys/icon0.png
     cp -R ../pkg/${platform}/assets/livearea ../pkg/${platform}/retroarch.vpk/vpk/sce_sys/
     make -C ../ -f Makefile.${platform}.salamander clean || exit 1
   fi
   if [ $PLATFORM = "ctr" ] ; then
   mv -f ../retroarch_3ds_salamander.cia ../pkg/${platform}/build/cia/retroarch_3ds.cia
   mkdir -p ../pkg/${platform}/build/3dsx/3ds/RetroArch
   mv -f ../retroarch_3ds_salamander.3dsx ../pkg/${platform}/build/3dsx/3ds/RetroArch/RetroArch.3dsx
   mv -f ../retroarch_3ds_salamander.smdh ../pkg/${platform}/build/3dsx/3ds/RetroArch/RetroArch.smdh
   # the .3ds port cant use salamander since you can only have one ROM on a cartridge at once
   make -C ../ -f Makefile.${platform}.salamander clean || exit 1
   fi
   if [ $PLATFORM = "wii" ] ; then
   mv -f ../retroarch-salamander_wii.dol ../pkg/${platform}/boot.dol
   fi
fi

COUNTER=0

if [ $PLATFORM = "libnx" ]; then
   echo Buildbot: building static core for ${platform}
   mkdir -p ../pkg/${platform}/switch
   make -C ../ -f Makefile.${platform} HAVE_STATIC_DUMMY=1 -j3 || exit 1
   mv -f ../retroarch_switch.nro ../pkg/${platform}/switch/retroarch_switch.nro
   make -C ../ -f Makefile.${platform} clean || exit 1
fi

#for f in *_${platform}.${EXT} ; do
for f in `ls -v *_${platform}.${EXT}`; do

   echo Buildbot: building ${name} for ${platform}
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).${EXT}$//"`
   async=0
   pthread=0
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
   elif [ $name = "mupen64plus" ] ; then
      async=1
   elif [ $name = "dosbox" ] ; then
      async=0
   fi
   echo "-- Building core: $name --"
   if [ $PLATFORM = "unix" ]; then
      cp -f "$f" ../libretro.${EXT}
   else
      cp -f "$f" ../libretro_${platform}.${EXT}
   fi
   echo NAME: $name
   echo ASYNC: $async
   echo LTO: $lto

   # Do cleanup if this is a big stack core
   if [ "$big_stack" = "BIG_STACK=1" ] ; then
      if [ $MAKEFILE_GRIFFIN = "yes" ]; then
         make -C ../ -f Makefile.griffin platform=${platform} clean || exit 1
      elif [ $PLATFORM = "emscripten" ]; then
         make -C ../ -f Makefile.emscripten PTHREAD=$pthread ASYNC=$async LTO=$lto -j7 clean || exit 1
      elif [ $PLATFORM = "unix" ]; then
         make -C ../ -f Makefile LINK=g++ LTO=$lto -j7 clean || exit 1
      else
         make -C ../ -f Makefile.${platform} clean || exit 1
      fi
   fi

   # Compile core
   if [ $MAKEFILE_GRIFFIN = "yes" ]; then
      make -C ../ -f Makefile.griffin $OPTS platform=${platform} $whole_archive $big_stack -j3 || exit 1
   elif [ $PLATFORM = "emscripten" ]; then
       echo "BUILD COMMAND: make -C ../ -f Makefile.emscripten PTHREAD=$pthread ASYNC=$async LTO=$lto -j7 TARGET=${name}_libretro.js"
       make -C ../ -f Makefile.emscripten $OPTS PTHREAD=$pthread ASYNC=$async LTO=$lto -j7 TARGET=${name}_libretro.js || exit 1
   elif [ $PLATFORM = "unix" ]; then
      make -C ../ -f Makefile LINK=g++ $whole_archive $big_stack -j3 || exit 1
   elif [ $PLATFORM = "ctr" ]; then
      make -C ../ -f Makefile.${platform} $OPTS LIBRETRO=$name $whole_archive $big_stack -j3 || exit 1
   elif [ $PLATFORM = "libnx" ]; then
      make -C ../ -f Makefile.${platform} $OPTS APP_TITLE="$name" LIBRETRO=$name $whole_archive $big_stack -j3 || exit 1
   elif [ $PLATFORM = "ps2" ]; then
      # TODO PS2 should be able to compile in parallel
      make -C ../ -f Makefile.${platform} $OPTS $whole_archive $big_stack || exit 1
   else
      make -C ../ -f Makefile.${platform} $OPTS $whole_archive $big_stack -j3 || exit 1
   fi

   # Do manual executable step
   if [ $PLATFORM = "ps2" ] ; then
      make -C ../ -f Makefile.${platform} package -j3
   elif [ $PLATFORM = "dex-ps3" ] ; then
      $MAKE_FSELF_NPDRM -c ../retroarch_${platform}.elf ../CORE.SELF
   elif [ $PLATFORM = "cex-ps3" ] ; then
      $SCETOOL_PATH $SCETOOL_FLAGS_CORE ../retroarch_${platform}.elf ../CORE.SELF
   elif [ $PLATFORM = "ode-ps3" ] ; then
      $SCETOOL_PATH $SCETOOL_FLAGS_ODE ../retroarch_${platform}.elf ../CORE.SELF
   fi

   # Move executable files
   if [ $platform = "ps3" ] ; then
      if [ $PLATFORM = "ode-ps3" ] ; then
         mv -fv ../CORE.SELF ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/"${name}_libretro_${platform}.SELF"
         if [ -d ../../dist/info ]; then
            mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/info
            cp -fv ../../dist/info/"${name}_libretro.info" ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/info/"${name}_libretro.info"
         fi
      else
         mv -fv ../CORE.SELF ../pkg/${platform}/SSNE10000/USRDIR/cores/"${name}_libretro_${platform}.SELF"
         if [ -d ../../dist/info ]; then
            mkdir -p ../pkg/${platform}/SSNE10000/USRDIR/cores/info
            cp -fv ../../dist/info/"${name}_libretro.info" ../pkg/${platform}/SSNE10000/USRDIR/cores/info/"${name}_libretro.info"
         fi
      fi
   elif [ $PLATFORM = "ps2" ] ; then
      mv -f ../retroarchps2-release.elf ../pkg/${platform}/cores/retroarchps2_${name}.elf
   elif [ $PLATFORM = "psp1" ] ; then
      mv -f ../EBOOT.PBP ../pkg/${platform}/cores/${name}_libretro.PBP
   elif [ $PLATFORM = "vita" ] ; then
      COUNTER=$((COUNTER + 1))
      COUNTER_ID=`printf  "%05d" ${COUNTER}`
      cp ../retroarch_${platform}.self ../pkg/${platform}/retroarch.vpk/vpk/${name}_libretro.self
         if [ -d ../../dist/info ]; then
            mkdir -p ../pkg/${platform}/retroarch.vpk/vpk/info
            cp -fv ../../dist/info/"${name}_libretro.info" ../pkg/${platform}/retroarch.vpk/vpk/info/"${name}_libretro.info"
         fi
   elif [ $PLATFORM = "ctr" ] ; then
      mv -f ../retroarch_3ds.cia ../pkg/${platform}/build/cia/${name}_libretro.cia
      mv -f ../retroarch_3ds.3dsx ../pkg/${platform}/build/3dsx/${name}_libretro.3dsx
      mv -f ../retroarch_3ds.3ds ../pkg/${platform}/build/rom/${name}_libretro.3ds
   elif [ $PLATFORM = "libnx" ] ; then
      mkdir -p ../pkg/${platform}/retroarch/cores/
      mv -f ../retroarch_switch.nro ../pkg/${platform}/retroarch/cores/${name}_libretro_${platform}.nro
   elif [ $PLATFORM = "unix" ] ; then
      mv -f ../retroarch ../pkg/${platform}/${name}_libretro.elf
   elif [ $PLATFORM = "ngc" ] ; then
      mv -f ../retroarch_${platform}.dol ../pkg/${platform}/${name}_libretro_${platform}.dol
   elif [ $PLATFORM = "wii" ] ; then
      mv -f ../retroarch_${platform}.dol ../pkg/${platform}/${name}_libretro_${platform}.dol
   elif [ $PLATFORM = "emscripten" ] ; then
      mkdir -p ../pkg/emscripten/
      mv -f ../${name}_libretro.js ../pkg/emscripten/${name}_libretro.js
      mv -f ../${name}_libretro.wasm ../pkg/emscripten/${name}_libretro.wasm
      if [ $pthread != 0 ] ; then
         mv -f ../pthread-main.js ../pkg/emscripten/pthread-main.js
      fi
   fi

   # Remove executable files
   if [ $platform = "ps3" ] ; then
      rm -f ../retroarch_${platform}.elf ../retroarch_${platform}.self ../CORE.SELF
   elif [ $PLATFORM = "ps2" ] ; then
      rm -f ../retroarchps2.elf
   elif [ $PLATFORM = "psp1" ] ; then
      rm -f ../retroarchpsp.elf
   elif [ $PLATFORM = "vita" ] ; then
      rm -f ../retroarch_${platform}.velf ../retroarch_${platform}.elf ../eboot.bin
   elif [ $PLATFORM = "ctr" ] ; then
      rm -f ../retroarch_3ds.elf
      rm -f ../retroarch_3ds.bnr
      rm -f ../retroarch_3ds.icn
   elif [ $PLATFORM = "libnx" ] ; then
      rm -f ../retroarch_switch.elf
      rm -f ../retroarch_switch.nacp
      rm -f ../retroarch_switch.nso
   elif [ $PLATFORM = "unix" ] ; then
      rm -f ../retroarch
   elif [ $PLATFORM = "ngc" ] ; then
      rm -f ../retroarch_${platform}.dol ../retroarch_${platform}.elf ../retroarch_${platform}.elf.map
   elif [ $PLATFORM = "wii" ] ; then
      rm -f ../retroarch_${platform}.dol ../retroarch_${platform}.elf ../retroarch_${platform}.elf.map
   elif [ $platform = "emscripten" ] ; then
      rm -f ../${name}_libretro.js
   fi

   # Do cleanup if this is a big stack core
   if [ "$big_stack" = "BIG_STACK=1" ] ; then
      if [ $MAKEFILE_GRIFFIN = "yes" ]; then
         make -C ../ -f Makefile.griffin platform=${platform} clean || exit 1
      elif [ $PLATFORM = "emscripten" ]; then
         make -C ../ -f Makefile.emscripten PTHREAD=$pthread ASYNC=$async LTO=$lto -j7 clean || exit 1
      elif [ $PLATFORM = "unix" ]; then
         make -C ../ -f Makefile LTO=$lto -j7 clean || exit 1
      else
         make -C ../ -f Makefile.${platform} clean || exit 1
      fi
   fi
done

# Additional build step
if [ $platform = "ps3" ] ; then
   if [ $PLATFORM = "ode-ps3" ] ; then
      echo Deploy : Assets...
      if [ -d ../media/assets ]; then
         mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/assets
         cp -r ../media/assets/* ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/assets
      fi
      echo Deploy : Databases...
      if [ -d ../media/libretrodb/rdb ]; then
         mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/database/rdb
         cp -r ../media/libretrodb/rdb/* ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/database/rdb
	  fi
	  if [ -d ../media/libretrodb/cursors ]; then
         mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/database/cursors
         cp -r ../media/libretrodb/cursors/* ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/database/cursors
      fi
      echo Deploy : Overlays...
      if [ -d ../media/overlays ]; then
         mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/overlays
         cp -r ../media/overlays/* ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/overlays
      fi
      echo Deploy : Shaders...
      if [ -d ../media/shaders_cg ]; then
         mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/shaders_cg
         cp -r ../media/shaders_cg/* ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/shaders_cg
      fi
   else
      echo Deploy : Assets...
      if [ -d ../media/assets ]; then
         mkdir -p ../pkg/${platform}/SSNE10000/USRDIR/cores/assets
         cp -r ../media/assets/* ../pkg/${platform}/SSNE10000/USRDIR/cores/assets
      fi
      echo Deploy : Databases...
      if [ -d ../media/libretrodb/rdb ]; then
         mkdir -p ../pkg/${platform}/SSNE10000/USRDIR/cores/database/rdb
         cp -r ../media/libretrodb/rdb/* ../pkg/${platform}/SSNE10000/USRDIR/cores/database/rdb
	  fi
	  if [ -d ../media/libretrodb/cursors ]; then
         mkdir -p ../pkg/${platform}/SSNE10000/USRDIR/cores/database/cursors
         cp -r ../media/libretrodb/cursors/* ../pkg/${platform}/SSNE10000/USRDIR/cores/database/cursors
      fi
      echo Deploy : Overlays...
      if [ -d ../media/overlays ]; then
         mkdir -p ../pkg/${platform}/SSNE10000/USRDIR/cores/overlays
         cp -r ../media/overlays/* ../pkg/${platform}/SSNE10000/USRDIR/cores/overlays
      fi
      echo Deploy : Shaders...
      if [ -d ../media/shaders_cg ]; then
         mkdir -p ../pkg/${platform}/SSNE10000/USRDIR/cores/shaders_cg
         cp -r ../media/shaders_cg/* ../pkg/${platform}/SSNE10000/USRDIR/cores/shaders_cg
      fi
   fi
fi

# Packaging
if [ $PLATFORM = "dex-ps3" ] ; then
   rsync -av ../pkg/${platform}/SSNE10000/USRDIR/cores/*.SELF ../pkg/${platform}/${PLATFORM}/
   $MAKE_FSELF_NPDRM -c ../retroarch-salamander_${platform}.elf ../pkg/${platform}/SSNE10000/USRDIR/EBOOT.BIN
   rm -rf ../retroarch-salamander_${platform}.elf
   $MAKE_PACKAGE_NPDRM ../pkg/${platform}_dex/package.conf ../pkg/${platform}/SSNE10000
   mv UP0001-SSNE10000_00-0000000000000001.pkg ../pkg/${platform}/RetroArch.PS3.DEX.PS3.pkg
elif [ $PLATFORM = "cex-ps3" ] ; then
   rsync -av ../pkg/${platform}/SSNE10000/USRDIR/cores/*.SELF ../pkg/${platform}/${PLATFORM}/
   $SCETOOL_PATH $SCETOOL_FLAGS_EBOOT ../retroarch-salamander_${platform}.elf ../pkg/${platform}/SSNE10000/USRDIR/EBOOT.BIN
   rm -rf ../retroarch-salamander_${platform}.elf
   (cd ../tools/ps3/ps3py && python2 setup.py build)
   find ../tools/ps3/ps3py/build -name '*.dll' -exec cp {} ../tools/ps3/ps3py \;
   ../tools/ps3/ps3py/pkg.py --contentid UP0001-SSNE10000_00-0000000000000001 ../pkg/${platform}/SSNE10000/ ../pkg/${platform}/RetroArch.PS3.CEX.PS3.pkg
elif [ $PLATFORM = "ode-ps3" ] ; then
   rsync -av ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/*.SELF ../pkg/${platform}/${PLATFORM}/
   $SCETOOL_PATH $SCETOOL_FLAGS_ODE ../retroarch-salamander_${platform}.elf ../pkg/${platform}_iso/PS3_GAME/USRDIR/EBOOT.BIN
   rm -rf ../retroarch-salamander_${platform}.elf

   $GENPS3ISO_PATH ../pkg/${platform}_iso ../pkg/${platform}/RetroArch.PS3.ODE.PS3.iso
fi
