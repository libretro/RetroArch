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

# A run with the core rarely beside the frontend tells nothing either
# way (INCONCLUSIVE): run again, a few times.
run() {
   n=0
   while :; do
      out=$(timeout 120 "$WINE" "./$1" | tr -d '\r') || true
      echo "$out"
      case "$out" in
         *INCONCLUSIVE*) ;;
         *) return 0 ;;
      esac
      n=$((n + 1))
      if [ $n -ge 5 ]; then
         echo "FAIL: the core was never running beside the frontend"
         exit 1
      fi
   done
}

echo "== the turns as the driver takes them"
run d3d11_turns_test.exe
case "$out" in
   *"PASS: d3d11 turns"*) ;;
   *) echo "FAIL"; exit 1 ;;
esac

# Each run of the unfair core either catches the frontend kept waiting
# or, when the host had the core off the CPU at every counted ask, does
# not; one catch in a few runs is the test telling them apart.
echo "== a core that does not give way (must fail)"
caught=0
for attempt in 1 2 3 4 5; do
   run d3d11_turns_unfair_test.exe
   case "$out" in
      *"PASS: d3d11 turns"*) ;;
      *) caught=1; break ;;
   esac
done
if [ $caught -eq 0 ]; then
   echo "FAIL: it passed; the test no longer sees a frontend kept waiting"; exit 1
fi
echo "   fails, as it should"
rm -f d3d11_turns_test.exe d3d11_turns_unfair_test.exe
