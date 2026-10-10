#!/bin/sh
# Builds d3d11_ring_dispatch_test with mingw-w64 and runs it under Wine.
# No device is created, so no X server is needed. See the test for what
# it checks.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$here"

CC=${CC:-x86_64-w64-mingw32-gcc}
WINE=${WINE:-$(command -v wine64 || command -v wine || true)}
lc=$root/libretro-common

"$CC" -O1 -Wall -Werror=implicit-function-declaration \
   -DHAVE_THREADS -DHAVE_D3D11 \
   -I "$root" -I "$lc/include" -I "$root/gfx/include/dxsdk" \
   -o d3d11_ring_dispatch_test.exe d3d11_ring_dispatch_test.c \
   "$lc/rthreads/retro_eventcount.c" "$lc/rthreads/rthreads.c" \
   "$lc/compat/compat_strl.c"

case "$(uname -s)" in
   MINGW*|MSYS*) ./d3d11_ring_dispatch_test.exe ;;
   *) WINEDEBUG=${WINEDEBUG:--all} timeout 300 "$WINE" ./d3d11_ring_dispatch_test.exe ;;
esac
