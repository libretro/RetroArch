#!/bin/sh
# One D3D driver per line: translation unit, its define, and the poke
# entries whose threaded path must reach video_thread_texture_handle.
# Then D3D12's unload and compressed load, which retire what they let
# go behind the queue's fence rather than wait for it.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
CC=${MINGW_CC:-x86_64-w64-mingw32-gcc}
OBJDUMP=${OBJDUMP:-x86_64-w64-mingw32-objdump}
cd "$root"

if ! command -v "$CC" > /dev/null 2>&1; then
   echo "skip  d3d texture marshal (no $CC)"
   exit 0
fi

INC="-isystemgfx/include/dxsdk -I. -Ilibretro-common/include -Ideps -Ideps/rcheevos/include -Iinput/include"
DEFS="-DRARCH_INTERNAL -DHAVE_AUDIOMIXER -DHAVE_THREADS -DHAVE_CONFIGFILE -DHAVE_MENU -DHAVE_D3D -DHAVE_RGUI -DHAVE_OVERLAY"
fail=0

check_driver() {
   tu="$1"; def="$2"; shift 2
   obj="$here/$(basename "$tu" .c).o"
   if ! $CC -Wall -Wno-unused-function $INC $DEFS "$def" -O1 -c "$tu" -o "$obj"; then
      echo "FAIL  $tu does not compile"
      fail=1
      return
   fi
   dis=$($OBJDUMP -dr "$obj")
   for sym in "$@"; do
      body=$(printf '%s\n' "$dis" | awk -v s="<$sym>:" '
         $2 == s { p = 1; next }
         p && /^$/ { exit }
         p { print }')
      if [ -z "$body" ]; then
         echo "FAIL  $sym: not in $tu's object"
         fail=1
      elif printf '%s\n' "$body" | grep -q "video_thread_texture_handle"; then
         echo "ok    $sym goes through video_thread_texture_handle"
      else
         echo "FAIL  $sym: threaded path never reaches video_thread_texture_handle"
         fail=1
      fi
   done
   rm -f "$obj"
}

# The named functions let their resources go through d3d12_retire and
# never wait on the queue themselves: a drain per texture unload is a
# GPU round trip for every icon a menu's context destroy frees.
check_retire() {
   tu="$1"; def="$2"; shift 2
   obj="$here/$(basename "$tu" .c).o"
   if ! $CC -Wall -Wno-unused-function $INC $DEFS "$def" -O1 -c "$tu" -o "$obj"; then
      echo "FAIL  $tu does not compile"
      fail=1
      return
   fi
   dis=$($OBJDUMP -dr "$obj")
   for sym in "$@"; do
      body=$(printf '%s\n' "$dis" | awk -v s="<$sym>:" '
         $2 == s { p = 1; next }
         p && /^$/ { exit }
         p { print }')
      if [ -z "$body" ]; then
         echo "FAIL  $sym: not in $tu's object"
         fail=1
      elif printf '%s\n' "$body" | grep -q "d3d12_queue_drain\|WaitForSingleObject"; then
         echo "FAIL  $sym: waits on the queue"
         fail=1
      elif ! printf '%s\n' "$body" | grep -q "d3d12_retire"; then
         echo "FAIL  $sym: does not retire behind the fence"
         fail=1
      else
         echo "ok    $sym retires behind the fence, without a wait"
      fi
   done
   rm -f "$obj"
}

check_driver gfx/drivers/d3d10.c -DHAVE_D3D10 \
   d3d10_gfx_load_texture d3d10_gfx_unload_texture d3d10_gfx_update_texture
check_driver gfx/drivers/d3d11.c -DHAVE_D3D11 \
   d3d11_gfx_load_texture d3d11_gfx_unload_texture d3d11_gfx_update_texture
# D3D12 has no device-level mutex: a one-shot copy on the queue and an
# SRV out of the descriptor bitmap belong to the video thread, the
# compressed load included.
check_driver gfx/drivers/d3d12.c -DHAVE_D3D12 \
   d3d12_gfx_load_texture d3d12_gfx_unload_texture d3d12_gfx_update_texture \
   d3d12_gfx_load_texture_compressed
check_retire gfx/drivers/d3d12.c -DHAVE_D3D12 \
   d3d12_gfx_unload_texture_internal d3d12_gfx_load_texture_compressed_internal

exit $fail
