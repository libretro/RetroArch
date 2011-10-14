#!/bin/sh
############### 
# Build script which builds and packages SSNES for MinGW 32/64-bit.
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
      git clone git://github.com/Themaister/SSNES-Phoenix.git Phoenix
      cd Phoenix
   else
      cd Phoenix
      git pull origin master
   fi

   make -f Makefile.win clean || die "Failed to clean ..."
   make -f Makefile.win CC="$C_COMPILER" CXX="$CXX_COMPILER" WINDRES="$WINDRES" -j4 all || die "Failed to build ..."
   touch ssnes-phoenix.cfg
   cp ssnes-phoenix.cfg ssnes-phoenix.exe ../ || die "Failed to copy ..."

   cd ..
}

do_build()
{
   SSNES_DIR="$1"
   LIBZIPNAME="$2"
   BUILDTYPE="$3"

   if [ ! -d "$SSNES_DIR" ]; then
      git clone git://github.com/Themaister/SSNES.git "$SSNES_DIR"
      cd "$SSNES_DIR"
   else
      cd "$SSNES_DIR"
      git pull origin master
   fi

   if [ ! -f "$LIBZIPNAME" ]; then
      make -f Makefile.win libs_${BUILDTYPE} || die "Failed to extract"
   fi

   make -f Makefile.win clean || die "Failed to clean ..."
   make -f Makefile.win CC="$C_COMPILER" CXX="$CXX_COMPILER" -j4 all SLIM=1 || die "Failed to build ..."
   make -f Makefile.win CC="$C_COMPILER" CXX="$CXX_COMPILER" dist_${BUILDTYPE} SLIM=1 || die "Failed to dist ..."
   if [ -z "`find . | grep "ssnes-win"`" ]; then
      die "Did not find build ..."
   fi

   if [ "$BUILD_PHOENIX_GUI" = "yes" ]; then
      do_phoenix_build
   fi

   ZIP_BASE="`find . | grep "ssnes-win" | head -n1`"
   ZIP_SLIM="`echo $ZIP_BASE | sed -e 's|\.zip|-slim.zip|'`"
   ZIP_FULL="`echo $ZIP_BASE | sed -e 's|\.zip|-full.zip|'`"

   if [ "$BUILD_PHOENIX_GUI" = "yes" ]; then
      zip "$ZIP_BASE" ssnes-phoenix.exe ssnes-phoenix.cfg
   fi
   mv -v "$ZIP_BASE" "../$ZIP_SLIM" || die "Failed to move final build ..."

   make -f Makefile.win clean || die "Failed to clean ..."
   make -f Makefile.win CC="$C_COMPILER" CXX="$CXX_COMPILER" -j4 all || die "Failed to build ..."
   make -f Makefile.win CC="$C_COMPILER" CXX="$CXX_COMPILER" dist_${BUILDTYPE} || die "Failed to dist ..."

   if [ "$BUILD_PHOENIX_GUI" = "yes" ]; then
      zip "$ZIP_BASE" ssnes-phoenix.exe ssnes-phoenix.cfg
   fi
   mv -v "$ZIP_BASE" "../$ZIP_FULL" || die "Failed to move final build ..."
   
   cd ..
}

if [ "$BUILD_32BIT" = yes ]; then
   message "Building for 32-bit!"
   C_COMPILER=${MINGW32_BASE}-gcc
   CXX_COMPILER=${MINGW32_BASE}-g++
   WINDRES=${MINGW32_BASE}-windres
   do_build "SSNES-w32" "SSNES-win32-libs.zip" "x86"
fi

if [ "$BUILD_64BIT" = yes ]; then
   message "Building for 64-bit!"
   C_COMPILER=${MINGW64_BASE}-gcc
   CXX_COMPILER=${MINGW64_BASE}-g++
   WINDRES=${MINGW64_BASE}-windres
   do_build "SSNES-w64" "SSNES-win64-libs.zip" "x86_64"
fi

message "Built successfully! :)"

