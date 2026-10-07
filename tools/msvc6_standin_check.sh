#!/bin/sh
# Builds what the Visual C++ 6 project builds, without Visual C++ 6.
#
# pkg/msvc/msvc-6 is a project for a compiler from 1998 that no CI
# machine can be given: nothing built it, and it stopped compiling
# unseen (it defined the network command interface without networking,
# which command.c cannot compile). This takes the project at its word -
# the two source files it names, the defines on its compiler line, the
# libraries on its linker line, all read from the .dsp - and compiles
# and links them with a 32-bit mingw compiler held to old C:
#
# - GNU C89 with -pedantic an error and no declaration after a
#   statement, which is what that compiler's C comes to for this tree
#   ("long long" and variadic macros aside: the tree spells those for
#   it under _MSC_VER, which a stand-in cannot be);
# - Windows NT 4.0's API (_WIN32_WINNT 0x0400). The project says NT
#   3.51; the stand-in's headers do not hold together below 4.0;
# - a real link against the project's own libraries, and the ones the
#   sources ask Microsoft's linker for by #pragma comment(lib), which
#   only that linker reads.
#
# So it catches what does not depend on the compiler being the real
# one: a define the project lacks that some code now needs, newer C in
# a file the project compiles, a call to something newer than NT 4.0,
# a function that is called and was compiled out. It does not catch
# what only Visual C++ 6 objects to, and it never reads the code that
# is written for it alone (#if _MSC_VER <= 1200).
#
# Needs: gcc-mingw-w64-i686 g++-mingw-w64-i686.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
dsp=pkg/msvc/msvc-6/RetroArch/RetroArch.dsp
CC=${CC:-i686-w64-mingw32-gcc}
CXX=${CXX:-i686-w64-mingw32-g++}
command -v "$CC"  >/dev/null 2>&1 || { echo "no $CC"  >&2; exit 2; }
command -v "$CXX" >/dev/null 2>&1 || { echo "no $CXX" >&2; exit 2; }

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

# the release configuration's compiler line: its defines, less the
# Windows version (see above)
defs=$(tr -d '\r' < "$dsp" | grep -m1 '^# ADD CPP ' \
   | grep -oE '/D "?[A-Za-z_0-9=x]+"?' | sed 's#^/D #-D#; s/"//g' \
   | grep -v '^-D_WIN32_WINNT=' | tr '\n' ' ')
# ... and its linker line's libraries
libs=$(tr -d '\r' < "$dsp" | grep -m1 '^# ADD LINK32 ' \
   | grep -oE '[A-Za-z0-9_]+\.lib' | sed 's/\.lib$//; s/^/-l/' | tr '\n' ' ')
# #pragma comment(lib, ...) in ui_win32.c and win32_common.c
libs="$libs -lcomctl32 -limm32"
[ -n "$defs" ] && [ -n "$libs" ] || { echo "could not read $dsp" >&2; exit 2; }

inc="-I libretro-common/include -I deps -I deps/stb"
win="-D_WIN32_WINNT=0x0400 -DWINVER=0x0400"
fail=0

if ! $CC -c -O0 -std=gnu89 -pedantic -Werror=pedantic \
      -Wno-long-long -Wno-variadic-macros \
      -Werror=declaration-after-statement \
      -Werror=implicit-function-declaration \
      $inc $defs $win griffin/griffin.c -o "$out/griffin.o" \
      > "$out/c.log" 2>&1
then
   grep -E ' error' "$out/c.log" | head -30
   echo "FAIL msvc6_standin_check: griffin.c does not compile as the project has it"
   fail=1
fi

if ! $CXX -c -O0 -std=gnu++98 -w \
      $inc $defs $win griffin/griffin_cpp.cpp -o "$out/griffin_cpp.o" \
      > "$out/cpp.log" 2>&1
then
   grep -E ' error' "$out/cpp.log" | head -30
   echo "FAIL msvc6_standin_check: griffin_cpp.cpp does not compile as the project has it"
   fail=1
fi

[ "$fail" = 0 ] || exit 1

if ! $CXX -o "$out/retroarch.exe" "$out/griffin.o" "$out/griffin_cpp.o" $libs \
      > "$out/link.log" 2>&1
then
   grep -E 'undefined reference|error' "$out/link.log" \
      | sed -E "s/.*undefined reference to/undefined:/" | sort | uniq -c | head -30
   echo "FAIL msvc6_standin_check: the project's two objects do not link with its libraries"
   exit 1
fi
echo "PASS msvc6_standin_check (griffin.c, griffin_cpp.cpp; the project's defines and libraries; old C, NT 4.0, linked)"
