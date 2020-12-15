#!/bin/sh

. ../version.all
PLATFORM=$1
SALAMANDER=no
MAKEFILE_GRIFFIN=no

# PSL1GHT
if [ $PLATFORM = "psl1ght" ] ; then
platform=psl1ght
SALAMANDER=yes
EXT=a
ps3appid=SSNE10001

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
SCETOOL_FLAGS_EBOOT="--sce-type=SELF --compress-data=TRUE --skip-sections=TRUE --key-revision=04 --self-auth-id=1010000001000003 --self-vendor-id=01000002 --self-type=NPDRM --self-fw-version=0003004100000000 --np-license-type=FREE --np-content-id=UP0001-RETROARCH_00-0000000000000001 --np-app-type=EXEC --self-app-version=0001000000000000 --np-real-fname=EBOOT.BIN --encrypt"

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
else
   make -C ../ -f Makefile.${platform} clean || exit 1
fi

# Compile Salamander core
if [ $SALAMANDER = "yes" ]; then
   make -C ../ -f Makefile.${platform}.salamander $OPTS || exit 1
fi

COUNTER=0

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
   cp -f "$f" ../libretro_${platform}.${EXT}
   echo NAME: $name
   echo ASYNC: $async
   echo LTO: $lto

   # Do cleanup if this is a big stack core
   if [ "$big_stack" = "BIG_STACK=1" ] ; then
      if [ $MAKEFILE_GRIFFIN = "yes" ]; then
         make -C ../ -f Makefile.griffin platform=${platform} clean || exit 1
      else
         make -C ../ -f Makefile.${platform} clean || exit 1
      fi
   fi

   # Compile core
   if [ $MAKEFILE_GRIFFIN = "yes" ]; then
      make -C ../ -f Makefile.griffin $OPTS platform=${platform} $whole_archive $big_stack -j3 || exit 1
   else
      make -C ../ -f Makefile.${platform} $OPTS $whole_archive $big_stack -j3 || exit 1
   fi

   # Do manual executable step
   if [ $PLATFORM = "dex-ps3" ] ; then
      $MAKE_FSELF_NPDRM -c ../retroarch_${platform}.elf ../CORE.SELF
   elif [ $PLATFORM = "cex-ps3" ] ; then
      $SCETOOL_PATH $SCETOOL_FLAGS_CORE ../retroarch_${platform}.elf ../CORE.SELF
   elif [ $PLATFORM = "ode-ps3" ] ; then
      $SCETOOL_PATH $SCETOOL_FLAGS_ODE ../retroarch_${platform}.elf ../CORE.SELF
   fi

   # Move executable files
   if [ $platform = "psl1ght" ] ; then
       mv -fv ../retroarch_psl1ght.self ../pkg/psl1ght/pkg/USRDIR/cores/"${name}_libretro_${platform}.SELF"
       if [ -d ../../dist/info ]; then
           mkdir -p ../pkg/psl1ght/pkg/USRDIR/cores/info
           cp -fv ../../dist/info/"${name}_libretro.info" ../pkg/psl1ght/pkg/USRDIR/cores/info/"${name}_libretro.info"
       fi
   elif [ $platform = "ps3" ] ; then
      if [ $PLATFORM = "ode-ps3" ] ; then
         mv -fv ../CORE.SELF ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/"${name}_libretro_${platform}.SELF"
         if [ -d ../../dist/info ]; then
            mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/info
            cp -fv ../../dist/info/"${name}_libretro.info" ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/info/"${name}_libretro.info"
         fi
      else
         mv -fv ../CORE.SELF ../pkg/${platform}/RETROARCH/USRDIR/cores/"${name}_libretro_${platform}.SELF"
         if [ -d ../../dist/info ]; then
            mkdir -p ../pkg/${platform}/RETROARCH/USRDIR/cores/info
            cp -fv ../../dist/info/"${name}_libretro.info" ../pkg/${platform}/RETROARCH/USRDIR/cores/info/"${name}_libretro.info"
         fi
      fi
   fi

   # Remove executable files
   if [ $platform = "psl1ght" ] ; then
       rm -f ../retroarch_${platform}.elf ../retroarch_${platform}.self ../CORE.SELF
   elif [ $platform = "ps3" ] ; then
      rm -f ../retroarch_${platform}.elf ../retroarch_${platform}.self ../CORE.SELF
   fi

   # Do cleanup if this is a big stack core
   if [ "$big_stack" = "BIG_STACK=1" ] ; then
      if [ $MAKEFILE_GRIFFIN = "yes" ]; then
         make -C ../ -f Makefile.griffin platform=${platform} clean || exit 1
      else
         make -C ../ -f Makefile.${platform} clean || exit 1
      fi
   fi
done

# Additional build step
if [ $platform = "ps3" ] || [ $platform = "psl1ght" ] ; then
   if [ $PLATFORM = "ode-ps3" ] ; then
      echo Deploy : Assets...
      if [ -d ../media/assets ]; then
         mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/assets
         cp -r ../media/assets/* ../pkg/${platform}_iso/PS3_GAME/USRDIR/assets
      fi
      echo Deploy : Databases...
      if [ -d ../media/libretrodb/rdb ]; then
         mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/database/rdb
         cp -r ../media/libretrodb/rdb/* ../pkg/${platform}_iso/PS3_GAME/USRDIR/database/rdb
	  fi
	  if [ -d ../media/libretrodb/cursors ]; then
         mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/database/cursors
         cp -r ../media/libretrodb/cursors/* ../pkg/${platform}_iso/PS3_GAME/USRDIR/database/cursors
      fi
      echo Deploy : Overlays...
      if [ -d ../media/overlays ]; then
         mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/overlays
         cp -r ../media/overlays/* ../pkg/${platform}_iso/PS3_GAME/USRDIR/overlays
      fi
      echo Deploy : Shaders...
      if [ -d ../media/shaders_cg ]; then
         mkdir -p ../pkg/${platform}_iso/PS3_GAME/USRDIR/shaders_cg
         cp -r ../media/shaders_cg/* ../pkg/${platform}_iso/PS3_GAME/USRDIR/shaders_cg
      fi
   else
      if [ $platform = ps3 ]; then
          ps3pkgdir=pkg/ps3/RETROARCH
      else
          ps3pkgdir=pkg/psl1ght/pkg
      fi
      echo Deploy : Assets...
      if [ -d ../media/assets ]; then
         mkdir -p ../${ps3pkgdir}/USRDIR/assets
         cp -r ../media/assets/* ../${ps3pkgdir}/USRDIR/assets
      fi
      echo Deploy : Databases...
      if [ -d ../media/libretrodb/rdb ]; then
         mkdir -p ../${ps3pkgdir}/USRDIR/database/rdb
         cp -r ../media/libretrodb/rdb/* ../${ps3pkgdir}/USRDIR/database/rdb
	  fi
	  if [ -d ../media/libretrodb/cursors ]; then
         mkdir -p ../${ps3pkgdir}/USRDIR/database/cursors
         cp -r ../media/libretrodb/cursors/* ../${ps3pkgdir}/USRDIR/database/cursors
      fi
      echo Deploy : Overlays...
      if [ -d ../media/overlays ]; then
         mkdir -p ../${ps3pkgdir}/USRDIR/overlays
         cp -r ../media/overlays/* ../${ps3pkgdir}/USRDIR/overlays
      fi
      echo Deploy : Shaders...
      if [ -d ../media/shaders_cg ]; then
         mkdir -p ../${ps3pkgdir}/USRDIR/shaders_cg
         cp -r ../media/shaders_cg/* ../${ps3pkgdir}/USRDIR/shaders_cg
      fi
   fi
fi

# Packaging
if [ $PLATFORM = "dex-ps3" ] ; then
   rsync -av ../pkg/${platform}/RETROARCH/USRDIR/cores/*.SELF ../pkg/${platform}/${PLATFORM}/
   $MAKE_FSELF_NPDRM -c ../retroarch-salamander_${platform}.elf ../pkg/${platform}/RETROARCH/USRDIR/EBOOT.BIN
   rm -rf ../retroarch-salamander_${platform}.elf
   $MAKE_PACKAGE_NPDRM ../pkg/${platform}_dex/package.conf ../pkg/${platform}/RETROARCH
   mv UP0001-RETROARCH_00-0000000000000001.pkg ../pkg/${platform}/RetroArch.PS3.DEX.PS3.pkg
elif [ $PLATFORM = "cex-ps3" ] ; then
   rsync -av ../pkg/${platform}/RETROARCH/USRDIR/cores/*.SELF ../pkg/${platform}/${PLATFORM}/
   $SCETOOL_PATH $SCETOOL_FLAGS_EBOOT ../retroarch-salamander_${platform}.elf ../pkg/${platform}/RETROARCH/USRDIR/EBOOT.BIN
   rm -rf ../retroarch-salamander_${platform}.elf
   (cd ../tools/ps3/ps3py && python2 setup.py build)
   find ../tools/ps3/ps3py/build -name '*.dll' -exec cp {} ../tools/ps3/ps3py \;
   ../tools/ps3/ps3py/pkg.py --contentid UP0001-RETROARCH_00-0000000000000001 ../pkg/${platform}/RETROARCH/ ../pkg/${platform}/RetroArch.PS3.CEX.PS3.pkg
elif [ $PLATFORM = "psl1ght" ] ; then
   ${PS3DEV}/bin/pkg.py --contentid UP0001-SSNE10001_00-0000000000000001 ../pkg/psl1ght/pkg/ ../pkg/psl1ght/RetroArch.PSL1GHT.pkg
elif [ $PLATFORM = "ode-ps3" ] ; then
   rsync -av ../pkg/${platform}_iso/PS3_GAME/USRDIR/cores/*.SELF ../pkg/${platform}/${PLATFORM}/
   $SCETOOL_PATH $SCETOOL_FLAGS_ODE ../retroarch-salamander_${platform}.elf ../pkg/${platform}_iso/PS3_GAME/USRDIR/EBOOT.BIN
   rm -rf ../retroarch-salamander_${platform}.elf

   $GENPS3ISO_PATH ../pkg/${platform}_iso ../pkg/${platform}/RetroArch.PS3.ODE.PS3.iso
fi
