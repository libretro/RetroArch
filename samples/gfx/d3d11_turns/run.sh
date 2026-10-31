#!/bin/sh
# Builds d3d11_turns_test with mingw-w64 and runs it under Wine. No
# D3D11 device is needed: the turns are a critical section and an event.
#
# Two runs: the turns as the driver takes them must pass, and a core
# that does not give way must FAIL, or the test no longer tells them
# apart.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$here"

CC=${CC:-x86_64-w64-mingw32-gcc}
WINE=${WINE:-$(command -v wine64 || command -v wine)}
export WINEDEBUG=${WINEDEBUG:--all}

flags="-O1 -Wall -Wextra -Werror -std=c89 -pedantic -Wno-long-long -I $root/libretro-common/include"
"$CC" $flags -o d3d11_turns_test.exe d3d11_turns_test.c
"$CC" $flags -DD3D11_TURNS_TEST_UNFAIR -o d3d11_turns_unfair_test.exe d3d11_turns_test.c

echo "== the turns as the driver takes them"
out=$(timeout 120 "$WINE" ./d3d11_turns_test.exe | tr -d '\r') || true
echo "$out"
case "$out" in
   *"PASS: d3d11 turns"*) ;;
   *) echo "FAIL"; exit 1 ;;
esac

echo "== a core that does not give way (must fail)"
out=$(timeout 120 "$WINE" ./d3d11_turns_unfair_test.exe | tr -d '\r') || true
echo "$out"
case "$out" in
   *"PASS: d3d11 turns"*)
      echo "FAIL: it passed; the test no longer sees a frontend kept waiting"; exit 1 ;;
esac
echo "   fails, as it should"
rm -f d3d11_turns_test.exe d3d11_turns_unfair_test.exe
