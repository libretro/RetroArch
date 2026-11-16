#!/bin/sh
# Builds the D3DKMT binding and vblank clock test with mingw-w64 and
# runs it under Wine on a headless X server. It links the window
# procedure sample's stand-ins for what win32_common.c calls.
set -eu

dir=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$dir/../../.." && pwd)
stubs="$root/samples/gfx/win32_window_proc"

CC=${CC:-x86_64-w64-mingw32-gcc}
if ! command -v "$CC" >/dev/null 2>&1; then
   echo "no $CC: install mingw-w64" >&2
   exit 1
fi

WINE=${WINE:-}
if [ -z "$WINE" ]; then
   for w in wine wine64 /usr/lib/wine/wine64; do
      if command -v "$w" >/dev/null 2>&1; then
         WINE=$w
         break
      fi
   done
fi
if [ -z "$WINE" ]; then
   echo "no wine" >&2
   exit 1
fi

# As the window procedure sample builds win32_common.c, with D3DKMT
# and the tree's own copy of its header.
FLAGS="-std=gnu99 -w -O1 -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_THREADS \
 -DHAVE_DYNAMIC -DHAVE_DYLIB -DHAVE_OVERLAY -DHAVE_GFX_WIDGETS -DHAVE_RGUI \
 -DHAVE_DINPUT -DHAVE_XINPUT -DHAVE_WINRAWINPUT -DHAVE_WINDOW -DHAVE_MONITOR \
 -DHAVE_CLIP_WINDOW -DHAVE_TASKBAR -DHAVE_VULKAN -DHAVE_GDI -DHAVE_OPENGL \
 -DHAVE_D3D11 -DHAVE_D3DKMT -D_WIN32_WINNT=0x0601 \
 -I. -Ilibretro-common/include -Ideps -Iinput/include -Igfx/include \
 -isystem gfx/include/dxsdk -I$stubs"

cd "$root"
objs=""
for src in "$dir/win32_d3dkmt_test.c" "$stubs/win32_window_proc_stubs.c" \
      libretro-common/encodings/encoding_utf.c \
      libretro-common/compat/compat_strl.c \
      libretro-common/rthreads/rthreads.c \
      libretro-common/rthreads/retro_eventcount.c \
      libretro-common/features/features_cpu.c \
      libretro-common/time/rtime.c; do
   obj="$dir/$(basename "$src" .c).o"
   "$CC" $FLAGS -c -o "$obj" "$src"
   objs="$objs $obj"
done
"$CC" -o "$dir/win32_d3dkmt_test.exe" $objs \
   -lgdi32 -luser32 -limm32 -lshell32 -lcomdlg32 -lole32 -ldwmapi -lopengl32

# A display Wine can talk to; the runner has no seat of its own.
if [ -z "${DISPLAY:-}" ]; then
   Xvfb :79 -screen 0 1280x720x24 >/dev/null 2>&1 &
   xvfb_pid=$!
   DISPLAY=:79
   export DISPLAY
   trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
   sleep 2
fi

export WINEDEBUG=${WINEDEBUG:--all}
cd "$dir"
if ! timeout 120 "$WINE" ./win32_d3dkmt_test.exe; then
   echo "FAIL win32_d3dkmt_test" >&2
   exit 1
fi
rm -f ./*.o win32_d3dkmt_test.exe
