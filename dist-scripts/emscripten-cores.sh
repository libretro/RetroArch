#!/bin/sh

if [ -z "$EMSCRIPTEN" ] ; then
   echo "run this script with emmake. Ex: emmake $0"
   exit 1
fi

make -C ../ -f Makefile.emscripten clean || exit 1

for f in *_emscripten.bc ; do
   name=`echo "$f" | sed 's/\(_libretro\|\)_emscripten.bc$//'`
   lto=1
   echo "building $name"
   if [ $name = "tyrquake" ] ; then
      lto=0
   fi
   cp -f "$f" ../libretro_emscripten.bc
   make -C ../ -f Makefile.emscripten LTO=$lto -j7 || exit 1
   mv -f ../retroarch.js ../emscripten/$name.js
   rm -f ../retroarch.js
done
