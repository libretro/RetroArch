#!/bin/sh
# Builds d3d11_hw_interface_v2_test with mingw-w64 and runs it under Wine,
# where wined3d implements D3D11 over OpenGL (Mesa's llvmpipe will do: no
# GPU is needed). See the test for what it checks.
#
# Three runs. The contract as libretro_d3d11.h version 2 states it must
# pass. Two controls must FAIL: a core that ignores what lock_context
# tells it, and a core that skips wait_sync_index. If either passes, the
# frontend in the test has stopped disturbing the context or stopped
# lapping the core, and the first run proves nothing.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$here"

CC=${CC:-x86_64-w64-mingw32-gcc}
WINE=${WINE:-$(command -v wine64 || command -v wine)}

"$CC" -O1 -Wall -I "$root/libretro-common/include" \
   -o d3d11_hw_interface_v2_test.exe d3d11_hw_interface_v2_test.c \
   -ld3d11 -ld3dcompiler -ldxguid -luuid

xpid=
if [ -z "${DISPLAY:-}" ]; then
   Xvfb :78 -screen 0 640x480x24 >/dev/null 2>&1 &
   xpid=$!
   DISPLAY=:78
   export DISPLAY
   sleep 2
fi
trap '[ -n "$xpid" ] && kill "$xpid" 2>/dev/null || true' EXIT
export WINEDEBUG=${WINEDEBUG:--all}

echo "== version 2 as the header states it"
out=$(timeout 300 "$WINE" ./d3d11_hw_interface_v2_test.exe | tr -d '\r') || true
echo "$out"
case "$out" in
   *"no D3D11 device"*)
      # CI sets REQUIRE_D3D11=1: a lane that installs Wine and an X server
      # and still has no device is broken, not skippable.
      if [ -n "${REQUIRE_D3D11:-}" ]; then
         echo "FAIL: no D3D11 device under Wine, and REQUIRE_D3D11 is set"; exit 1
      fi
      echo "SKIP: Wine has no D3D11 device here"; exit 0 ;;
   *"d3d11_hw_interface_v2: ok"*) ;;
   *) echo "FAIL"; exit 1 ;;
esac

echo "== a core that ignores what lock_context tells it (must fail)"
out=$(timeout 300 "$WINE" ./d3d11_hw_interface_v2_test.exe norebind | tr -d '\r') || true
echo "$out"
case "$out" in
   *"d3d11_hw_interface_v2: ok"*)
      echo "FAIL: it passed; the frontend in the test no longer disturbs the context"; exit 1 ;;
esac
echo "   fails, as it should"

echo "== a core that skips wait_sync_index (must fail)"
out=$(timeout 300 "$WINE" ./d3d11_hw_interface_v2_test.exe v2-nowait | tr -d '\r') || true
echo "$out"
case "$out" in
   *"d3d11_hw_interface_v2: ok"*)
      echo "FAIL: it passed; the frontend in the test is no longer being lapped"; exit 1 ;;
esac
echo "   fails, as it should"
