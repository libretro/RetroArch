#!/bin/sh
# Builds d3d12_hw_interface_v2_test with mingw-w64 and runs it under Wine,
# where vkd3d implements D3D12 over Vulkan (Mesa's lavapipe will do: no
# GPU is needed). See the test for what it checks.
#
# Two runs. The contract as libretro_d3d12.h version 2 states it must
# pass. A core that skips wait_sync_index must FAIL: if it passes, the
# frontend in the test has stopped being slow enough to be lapped, and
# the first run proves nothing.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$here"

CC=${CC:-x86_64-w64-mingw32-gcc}
WINE=${WINE:-$(command -v wine64 || command -v wine)}

"$CC" -O1 -Wall -I "$root/libretro-common/include" \
   -o d3d12_hw_interface_v2_test.exe d3d12_hw_interface_v2_test.c \
   -ld3d12 -ldxguid -luuid

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
# Prefer the software Vulkan driver when it is there, so a runner without
# a GPU still has a device for vkd3d.
if [ -z "${VK_ICD_FILENAMES:-}" ] && [ -f /usr/share/vulkan/icd.d/lvp_icd.json ]; then
   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
   export VK_ICD_FILENAMES
fi

echo "== version 2 as the header states it"
out=$(timeout 300 "$WINE" ./d3d12_hw_interface_v2_test.exe | tr -d '\r') || true
echo "$out"
case "$out" in
   *"no D3D12 device"*)
      # CI sets REQUIRE_D3D12=1: a lane that installs vkd3d and lavapipe
      # and still has no device is broken, not skippable.
      if [ -n "${REQUIRE_D3D12:-}" ]; then
         echo "FAIL: no D3D12 device under Wine, and REQUIRE_D3D12 is set"; exit 1
      fi
      echo "SKIP: Wine has no D3D12 device here (vkd3d or a Vulkan driver is missing)"; exit 0 ;;
   *"d3d12_hw_interface_v2: ok"*) ;;
   *) echo "FAIL"; exit 1 ;;
esac

echo "== a core that skips wait_sync_index (must fail)"
out=$(timeout 300 "$WINE" ./d3d12_hw_interface_v2_test.exe nowait | tr -d '\r') || true
echo "$out"
case "$out" in
   *"d3d12_hw_interface_v2: ok"*)
      echo "FAIL: it passed; the frontend in the test is no longer being lapped"; exit 1 ;;
esac
echo "   fails, as it should"
