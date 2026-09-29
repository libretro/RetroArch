#!/bin/sh
# Syntax-check C translation units with the include directories and
# defines Makefile.wiiu actually uses, on a PowerPC cross compiler.
#
# Why this exists: the Wii U build does not point at wut. It ships its
# own coreinit headers under wiiu/include/wiiu/os and compiles with
# -Iwiiu -Iwiiu/include, so <coreinit/event.h> does not resolve there
# and a file checked against wut alone passes while the real build
# fails. The spellings differ too (OSMicroseconds, not
# OSMicrosecondsToTicks). This check compiles against the tree's own
# headers, which is the only build that matters.
#
# Usage:
#   tools/wiiu_syntax_check.sh file.c [...]
#       just those files
#   tools/wiiu_syntax_check.sh
#       the rthreads units, which every Wii U build compiles
#
# CC defaults to powerpc-eabi-gcc (devkitPPC). Any PowerPC gcc that
# accepts -mcpu=750 will do for a syntax check; a 64-bit toolchain
# needs -m32 and a gnu/stubs-32.h stand-in on the include path, both
# through CC/EXTRA. A Linux-hosted cross gcc predefines __linux__,
# which would steer every platform switch in the tree down the Linux
# branch and never compile the Wii U code at all, so those macros are
# undefined here the way a bare-metal devkitPPC leaves them.
#
# Prints one line per failing TU and exits non-zero if any failed.

CC="${CC:-powerpc-eabi-gcc}"
command -v "${CC%% *}" >/dev/null 2>&1 || { echo "no $CC" >&2; exit 2; }

FLAGS="-fsyntax-only -mcpu=750 -Werror=implicit-function-declaration \
 -U__linux__ -U__linux -Ulinux -U__gnu_linux__ -U__unix__ -U__unix -Uunix \
 -I. -Ilibretro-common/include -Iwiiu -Iwiiu/include \
 -DWIIU -D__WUT__ -DHW_WUP -D__wiiu__ -DHAVE_MAIN -DRARCH_CONSOLE \
 -DRARCH_INTERNAL -DHAVE_THREADS $EXTRA"

if [ $# -eq 0 ]; then
   set -- libretro-common/rthreads/rthreads.c \
      libretro-common/rthreads/retro_eventcount.c \
      libretro-common/rthreads/tpool.c
fi

FAIL=0
for f in "$@"; do
   if ! $CC $FLAGS "$f" >/dev/null 2>&1; then
      echo "FAIL $f"
      $CC $FLAGS "$f" 2>&1 | grep -m3 "error"
      FAIL=1
   fi
done
exit $FAIL
