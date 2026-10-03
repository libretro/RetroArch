#!/bin/sh
# Builds the window procedure test with mingw-w64, runs it under Wine
# on a headless X server, and compares its trace with what the old
# window procedures gave (expected_trace.txt).
set -eu

dir=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$dir/../../.." && pwd)

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

# What a Windows build of win32_common.c is compiled with, as far as
# the window code goes: every video family and both input drivers.
FLAGS="-std=gnu99 -w -O1 -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_THREADS \
 -DHAVE_DYNAMIC -DHAVE_DYLIB -DHAVE_OVERLAY -DHAVE_GFX_WIDGETS -DHAVE_RGUI \
 -DHAVE_DINPUT -DHAVE_XINPUT -DHAVE_WINRAWINPUT -DHAVE_WINDOW -DHAVE_MONITOR \
 -DHAVE_CLIP_WINDOW -DHAVE_TASKBAR -DHAVE_VULKAN -DHAVE_GDI -DHAVE_OPENGL \
 -DHAVE_D3D11 -D_WIN32_WINNT=0x0601 ${W32T_DEFINES:-} \
 -I. -Ilibretro-common/include -Ideps -Iinput/include -Igfx/include -I$dir"

cd "$root"
objs=""
for src in "$dir/win32_window_proc_test.c" "$dir/win32_window_proc_stubs.c" \
      gfx/common/win32_common.c libretro-common/encodings/encoding_utf.c \
      libretro-common/compat/compat_strl.c; do
   obj="$dir/$(basename "$src" .c).o"
   "$CC" $FLAGS -c -o "$obj" "$src"
   objs="$objs $obj"
done
"$CC" -o "$dir/win32_window_proc_test.exe" $objs \
   -lgdi32 -luser32 -limm32 -lshell32 -lcomdlg32 -lole32 -ldwmapi -lopengl32

# A display Wine can talk to; the runner has no seat of its own.
if [ -z "${DISPLAY:-}" ]; then
   Xvfb :78 -screen 0 1280x720x24 >/dev/null 2>&1 &
   xvfb_pid=$!
   DISPLAY=:78
   export DISPLAY
   trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
   sleep 2
fi

export WINEDEBUG=${WINEDEBUG:--all}
cd "$dir"
rm -f trace.txt
if ! timeout 300 "$WINE" ./win32_window_proc_test.exe trace.txt; then
   echo "FAIL win32_window_proc_test: the test did not run to its end" >&2
   exit 1
fi

if ! diff -u expected_trace.txt trace.txt > trace.diff; then
   echo "FAIL win32_window_proc_test: the window procedure no longer does" >&2
   echo "what the twelve it replaced did; first differences:" >&2
   head -40 trace.diff >&2
   exit 1
fi

echo "PASS win32_window_proc_test ($(wc -l < trace.txt) trace lines as recorded)"
rm -f trace.txt trace.diff ./*.o win32_window_proc_test.exe
