#!/bin/sh

. ../version.all
PLATFORM=emscripten
CLEAN=$2
PTHREADS=$3
LEGACY=$4
SALAMANDER=no
MAKEFILE_GRIFFIN=no

clean=no
if [ "$CLEAN" = "clean" ] ; then
clean=yes
fi
threads=0
if [ "$PTHREADS" = "yes" ] ; then
threads=8
fi

platform=emscripten
EXT=bc

if [ -z "$EMSCRIPTEN" ] ; then
   echo "run this script with emmake. Ex: emmake $0"
   exit 1
fi

# Cleanup existing core if it exists
if [ $clean = "yes" ]; then
make -C ../ -f Makefile.${platform} clean || exit 1
fi

COUNTER=0

lastPthreads=0
lastGles=0

#for f in *_${platform}.${EXT} ; do
for f in `ls -v *_${platform}.${EXT}`; do

   echo Buildbot: building ${name} for ${platform}
   name=`echo "$f" | sed "s/\(_libretro_${platform}\|\).${EXT}$//"`
   async=1
   pthread="$threads"
   wasm=1

   gles3=0
   stack_mem=8388608
   heap_mem=268435456

   if [ $name = "mupen64plus_next" ] ; then
      gles3=1
      async=1
      stack_mem=268435456
      heap_mem=536870912
      if [ $wasm = 0 ]; then
        continue;
      fi
   elif [ $name = "picodrive" ] ; then
      heap_mem=536870912
   elif [ $name = "pcsx_rearmed" ] || [ $name = "genesis_plus_gx" ]; then
      heap_mem=536870912
   elif [ $name = "mednafen_psx" ] || [ $name = "mednafen_psx_hw" ]; then
      gles3=1
      heap_mem=536870912
   elif [ $name = "parallel_n64" ] ; then
      gles3=1
      heap_mem=536870912
   elif [ $name = "ppsspp" ] ; then
      gles3=1
      pthread=32
      heap_mem=536870912
   fi
   echo "-- Building core: $name --"
   cp -f "$f" ../libretro_${platform}.a
   
   if [ "$LEGACY" = "yes" ]; then
      gles3=0
   elif [ "$LEGACY" = "no" ]; then
      gles3=1
   fi
   echo NAME: $name
   echo ASYNC: $async
   echo PTHREAD: $pthread
   echo WASM: $wasm
   echo GLES3: $gles3
   echo STACK_MEMORY: $stack_mem
   echo HEAP_MEMORY: $heap_mem

   if [ $clean = "yes" ]; then
      if [ $lastPthreads != $pthread ] ; then
         make -C ../ -f Makefile.${platform} clean || exit 1
      fi
      if [ $lastGles != $gles3 ] ; then
         make -C ../ -f Makefile.${platform} clean || exit 1
      fi
   fi

   lastPthreads=$pthread
   lastGles=$gles3

   # Compile core
   echo "BUILD COMMAND: make -C ../ -f Makefile.emscripten $OPTS PTHREAD=$pthread ASYNC=$async WASM=$wasm HAVE_OPENGLES3=$gles3 STACK_MEMORY=$stack_mem HEAP_MEMORY=$heap_mem -j7 TARGET=${name}_libretro.js"
   make -C ../ -f Makefile.emscripten $OPTS PTHREAD=$pthread ASYNC=$async WASM=$wasm HAVE_OPENGLES3=$gles3 STACK_MEMORY=$stack_mem HEAP_MEMORY=$heap_mem -j$(nproc) TARGET=${name}_libretro.js || exit 1

   # Move executable files
    mkdir -p ../pkg/emscripten/

    out_dir="../../EmulatorJS/data/cores"
    out_name=""

    mkdir -p $out_dir

    core=""
    if [ $name = "mednafen_vb" ]; then
      core="beetle_vb"
    else
       core=${name}
    fi
  
    if [ "$LEGACY" = "yes" ]; then
       if [ $pthread != 0 ] ; then
          out_name=${core}-thread-legacy-wasm.data
       else
          if [ $wasm = 0 ]; then
             out_name=${core}-legacy-asmjs.data
          else
             out_name=${core}-legacy-wasm.data
          fi
       fi
    else
       if [ $pthread != 0 ] ; then
          out_name=${core}-thread-wasm.data
       else
          if [ $wasm = 0 ]; then
             out_name=${core}-asmjs.data
          else
             out_name=${core}-wasm.data
          fi
       fi
    fi

    if [ $wasm = 0 ]; then
      7z a ${out_dir}/${out_name} ../${name}_libretro.js.mem ../${name}_*.js
      rm ../${name}_libretro.js.mem
    else
      7z a ${out_dir}/${out_name} ../${name}_libretro.wasm ../${name}_*.js
      rm ../${name}_libretro.wasm
    fi
    rm ../${name}_libretro.js
    
    # Remove executable files
    rm -f ../${name}_libretro.js
done
