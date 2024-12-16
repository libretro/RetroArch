#!/bin/sh
set +e

. ../version.all

if [[ -z "$EMSCRIPTEN" ]] ; then
  echo "Run this script with emmake. Ex: emmake $0"
  exit 1
fi

for i in "$@"; do
  case $i in
    --threads)
      PTHREADS=YES
      shift
      ;;
    --legacy)
      LEGACY=YES
      shift
      ;;
    --clean)
      CLEAN=YES
      shift
      ;;
    --asmjs)
      ASMJS=YES
      shift
      ;;
    *)
      echo "Unknown option $i"
      echo "Usage: $0 [option] ..."
      echo "Options:"
      echo "  --threads"
      echo "  --legacy"
      echo "  --clean"
      echo "  --asmjs"
      exit 1
      ;;
  esac
done

clean () {
  make -C ../ -f Makefile.emscripten clean || exit 1
}
containsElement () {
  local e match="$1"
  shift
  for e; do
    if [[ "$e" == "$match" ]]; then
      echo 1
      return 0
    fi
  done
  echo 0
  return 1
}

if [[ "$CLEAN" = "YES" ]]; then
  clean
fi

lastGles=0

largeStack=("mupen64plus_next")
largeHeap=("mupen64plus_next" "picodrive" "pcsx_rearmed" "genesis_plus_gx" "mednafen_psx" "mednafen_psx_hw" "parallel_n64" "ppsspp")
needsGles3=("ppsspp")
needsThreads=("ppsspp")
largeThreads=("ppsspp")

for f in $(ls -v *_emscripten.bc); do
  name=`echo "$f" | sed "s/\(_libretro_emscripten\|\).bc$//"`
  async=1
  wasm=1
  gles3=1
  stack_mem=8388608
  heap_mem=268435456
  pthread=0

  if [ "$LEGACY" = "YES" ]; then
    gles3=0
  fi

  if [[ "$PTHREADS" = "YES" ]]; then
    pthread=8
  fi

  if [[ "$ASMJS" = "YES" ]]; then
    wasm=0
  fi

  if [[ $(containsElement $name "${largeStack[@]}") = 1 ]]; then
    stack_mem=268435456
  fi
  if [[ $(containsElement $name "${largeHeap[@]}") = 1 ]]; then
    heap_mem=536870912
  fi
  if [[ $(containsElement $name "${needsThreads[@]}") = 1 && $pthread = 0 ]]; then
    echo "$name"' requires threads! Please build with --threads! Exiting...'
    exit 1
  fi
  if [[ $(containsElement $name "${largeThreads[@]}") = 1 ]]; then
    pthread=32
  fi
  if [[ $(containsElement $name "${needsGles3[@]}") = 1 && $gles3 = 0 ]]; then
    echo "$name"' does not support gles2 (legacy)! Please build without --legacy! Exiting...'
    exit 1
  fi
  if [[ $pthread != 0 && $wasm = 0 ]]; then
    echo 'This script does not support building threaded asmjs! Exiting...'
    exit 1
  fi

  echo "-- Building core: $name --"
  cp -f "$f" ../libretro_emscripten.a
   
  echo NAME: $name
  echo ASYNC: $async
  echo PTHREAD: $pthread
  echo WASM: $wasm
  echo GLES3: $gles3
  echo STACK_MEMORY: $stack_mem
  echo HEAP_MEMORY: $heap_mem

  if [[ "$CLEAN" = "YES" ]]; then
    if [ $lastGles != $gles3 ] ; then
        clean
    fi
  fi
  lastGles=$gles3

  # Compile core
  echo "BUILD COMMAND: make -C ../ -f Makefile.emscripten PTHREAD=$pthread ASYNC=$async WASM=$wasm HAVE_OPENGLES3=$gles3 STACK_MEMORY=$stack_mem HEAP_MEMORY=$heap_mem -j7 TARGET=${name}_libretro.js"
  make -C ../ -f Makefile.emscripten PTHREAD=$pthread ASYNC=$async WASM=$wasm HAVE_OPENGLES3=$gles3 STACK_MEMORY=$stack_mem HEAP_MEMORY=$heap_mem TARGET=${name}_libretro.js -j$(nproc) || exit 1

  # Move executable files
  out_dir="../../EmulatorJS/data/cores"
  out_name=""

  mkdir -p $out_dir

  core=""
  if [ $name = "mednafen_vb" ]; then
    core="beetle_vb"
  else
    core=${name}
  fi

  out_name=${core}

  if [[ $pthread != 0 ]] ; then
    out_name="${out_name}-thread"
  fi
  if [[ $gles3 = 0 ]] ; then
    out_name="${out_name}-legacy"
  fi
  if [[ $wasm = 1 ]] ; then
    out_name="${out_name}-wasm"
  else
    out_name="${out_name}-asmjs"
  fi
  out_name="${out_name}.data"

  if [ $wasm = 0 ]; then
    7z a ${out_dir}/${out_name} ../${name}_libretro.js.mem ../${name}_*.js
    rm ../${name}_libretro.js.mem
  else
    7z a ${out_dir}/${out_name} ../${name}_libretro.wasm ../${name}_*.js
    rm ../${name}_libretro.wasm
  fi
  rm -f ../${name}_libretro.js
done
