#!/bin/sh
###############
# Build script which builds and packages RetroArch for MinGW 32/64-bit.
# Preferably build on Linux with a cross chain ... :D
##########

####
## Tweak these to suit your environment.
## Not defining the variable will avoid building that target.
## Set MINGW32_BASE and/or MINGW64_BASE to set toolchain prefix:
## E.g.: i486-mingw32-gcc would get prefix "i486-mingw32".

BUILD_32BIT=yes
BUILD_64BIT=yes
BUILD_PHOENIX_GUI=yes

if [ ! -z "$NOBUILD_32BIT" ]; then
   BUILD_32BIT=no
fi
if [ ! -z "$NOBUILD_64BIT" ]; then
   BUILD_64BIT=no
fi
if [ ! -z "$NOBUILD_PHOENIX_GUI" ]; then
   BUILD_PHOENIX_GUI=no
fi

########

die()
{
   echo "$@"
   exit 1
}

message()
{
   echo ""
   echo "================================"
   echo "$@"
   echo "================================"
   echo ""
}

if [ -z "$MAKE" ]; then
   if uname -s | grep -i MINGW32 > /dev/null 2>&1; then
      MAKE=mingw32-make
   else
      if type gmake > /dev/null 2>&1; then
         MAKE=gmake
      else
         MAKE=make
      fi
   fi
fi

if [ ! -f "`which zip`" ]; then
   echo "Cannot find 'zip'. Cannot package, quitting ..."
   exit 1
fi

if [ ! -f "`which unzip`" ]; then
   echo "Cannot find 'unzip'. Cannot unpack, quitting ..."
   exit 1
fi

if [ ! -f "`which wget`" ]; then
   echo "Cannot find 'wget'. Cannot unpack, quitting ..."
   exit 1
fi

do_phoenix_build()
{
   message "Build Phoenix GUI"
   ### Build Phoenix GUI
   if [ ! -d "Phoenix" ]; then
      git clone git://github.com/Themaister/RetroArch-Phoenix.git Phoenix
      cd Phoenix
   else
      cd Phoenix
      git pull origin master
   fi

   "${MAKE}" -f Makefile.win clean || die "Failed to clean ..."
   "${MAKE}" -f Makefile.win CC="$C_COMPILER" CXX="$CXX_COMPILER" WINDRES="$WINDRES" -j4 all || die "Failed to build ..."
   touch retroarch-phoenix.cfg
   cp retroarch-phoenix.cfg retroarch-phoenix.exe ../ || die "Failed to copy ..."

   cd ..
}

do_build()
{
   RetroArch_DIR="$1"
   LIBZIPNAME="$2"
   BUILDTYPE="$3"

   if [ ! -d "$RetroArch_DIR" ]; then
      git clone git://github.com/libretro/RetroArch.git "$RetroArch_DIR"
      cd "$RetroArch_DIR"
   else
      cd "$RetroArch_DIR"
      git pull origin master
   fi

   if [ ! -f "$LIBZIPNAME" ]; then
      "${MAKE}" -f Makefile.win libs_${BUILDTYPE} || die "Failed to extract"
   fi

   "${MAKE}" -f Makefile.win clean || die "Failed to clean ..."
   "${MAKE}" -f Makefile.win CC="$C_COMPILER" CXX="$CXX_COMPILER" WINDRES="$WINDRES" -j4 all SLIM=1 || die "Failed to build ..."
   "${MAKE}" -f Makefile.win CC="$C_COMPILER" CXX="$CXX_COMPILER" WINDRES="$WINDRES" dist_${BUILDTYPE} SLIM=1 || die "Failed to dist ..."
   if [ -z "`find . | grep "retroarch-win"`" ]; then
      die "Did not find build ..."
   fi

   if [ "$BUILD_PHOENIX_GUI" = "yes" ]; then
      do_phoenix_build
   fi

   ZIP_BASE="`find . | grep "retroarch-win" | head -n1`"
   ZIP_SLIM="`echo $ZIP_BASE | sed -e 's|\.zip|-slim.zip|'`"
   ZIP_FULL="`echo $ZIP_BASE | sed -e 's|\.zip|-full.zip|'`"

   if [ "$BUILD_PHOENIX_GUI" = "yes" ]; then
      zip "$ZIP_BASE" retroarch-phoenix.exe retroarch-phoenix.cfg
   fi
   mv -v "$ZIP_BASE" "../$ZIP_SLIM" || die "Failed to move final build ..."

   "${MAKE}" -f Makefile.win clean || die "Failed to clean ..."
   "${MAKE}" -f Makefile.win CC="$C_COMPILER" CXX="$CXX_COMPILER" WINDRES="$WINDRES" HAVE_D3D9=1 -j4 all || die "Failed to build ..."
   "${MAKE}" -f Makefile.win CC="$C_COMPILER" CXX="$CXX_COMPILER" WINDRES="$WINDRES" HAVE_D3D9=1 dist_${BUILDTYPE} || die "Failed to dist ..."

   if [ "$BUILD_PHOENIX_GUI" = "yes" ]; then
      zip "$ZIP_BASE" retroarch-phoenix.exe retroarch-phoenix.cfg
   fi

   cp -v "$ZIP_BASE" "../$ZIP_FULL" || die "Failed to move final build ..."
   mv -v "$ZIP_BASE" ..
   zip "../$ZIP_BASE" *.dll retroarch-redist-version || die "Failed to build full/redist ..."

   cd ..
}

if [ "$BUILD_32BIT" = yes ]; then
   message "Building for 32-bit!"
   C_COMPILER="${MINGW32_BASE}-gcc"
   CXX_COMPILER="${MINGW32_BASE}-g++"
   WINDRES=${MINGW32_BASE}-windres
   do_build "RetroArch-w32" "RetroArch-win32-libs.zip" "x86"
fi

if [ "$BUILD_64BIT" = yes ]; then
   message "Building for 64-bit!"
   C_COMPILER=${MINGW64_BASE}-gcc
   CXX_COMPILER=${MINGW64_BASE}-g++
   WINDRES=${MINGW64_BASE}-windres
   do_build "RetroArch-w64" "RetroArch-win64-libs.zip" "x86_64"
fi

message "Built successfully! :)"
