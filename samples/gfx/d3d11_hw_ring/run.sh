#!/bin/sh
# Build the D3D11 hardware-ring oracle with mingw-w64 and run it under
# Wine on a headless X server. wined3d implements deferred contexts
# over OpenGL, which is enough to replay a command list and read the
# frontend's copy back.
#
# Three runs, because the point is a difference between them:
#   default     the ring as the driver runs it            - must pass
#   immediate   no ring, core on the immediate context    - must pass
#   reset       the ring resetting the context each frame - must FAIL,
#               since that is what it used to do: right for one frame
#               and black afterwards. A build where that run passes has
#               lost the regression this sample exists for.
set -eu

dir=$(cd "$(dirname "$0")" && pwd)
cd "$dir"

CC=${CC:-x86_64-w64-mingw32-gcc}
if ! command -v "$CC" >/dev/null 2>&1; then
   echo "no $CC: install mingw-w64" >&2
   exit 1
fi
if ! command -v wine >/dev/null 2>&1; then
   echo "no wine" >&2
   exit 1
fi

# The proxy is the thing under test, so it is built here rather than
# taken from a tree build; dxguid and uuid carry the interface GUIDs it
# refers to.
"$CC" -o d3d11_hw_ring_test.exe \
   d3d11_hw_ring_test.c ../../../gfx/common/d3d11_deferred_proxy.c \
   -I../../.. -I../../../libretro-common/include \
   -I../../../gfx/include/dxsdk \
   -Wall -O1 -ld3d11 -ld3dcompiler -ldxgi -ldxguid -luuid

# A display Wine can talk to; the runner has no seat of its own.
if [ -z "${DISPLAY:-}" ]; then
   Xvfb :77 -screen 0 640x480x24 >/dev/null 2>&1 &
   xvfb_pid=$!
   DISPLAY=:77
   export DISPLAY
   trap 'kill "$xvfb_pid" 2>/dev/null || true' EXIT
   sleep 2
fi

export WINEDEBUG=${WINEDEBUG:--all}
rc=0

run_case()
{
   name=$1
   want=$2
   shift 2
   echo "== $name"
   if wine ./d3d11_hw_ring_test.exe "$@" 2>&1; then
      got=pass
   else
      got=fail
   fi
   if [ "$got" != "$want" ]; then
      echo "   $name: wanted $want, got $got"
      rc=1
   else
      echo "   $name: $got, as it should"
   fi
}

run_case "the ring"                pass
run_case "the immediate context"   pass immediate
run_case "the ring as it was"      fail reset

exit $rc
