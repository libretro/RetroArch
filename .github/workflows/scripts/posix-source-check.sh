#!/bin/sh
# Compile every libretro-common file that includes retro_posix_source.h
# with $CC and no feature macros of its own, as a core's Makefile does.
# webOS.yml runs it with the webOS SDK, whose glibc 2.12 predates
# _DEFAULT_SOURCE: a name glibc hides behind __USE_MISC fails here
# instead of in a core's webOS build.
#
#   CC=arm-webos-linux-gnueabi-gcc sh posix-source-check.sh

set -e
cd "$(dirname "$0")/../../.."

CC=${CC:-cc}
fail=0
n=0
for tu in $(grep -rl --include='*.c' '#include <retro_posix_source.h>' \
      libretro-common | grep -v '/samples/' | sort); do
   n=$((n + 1))
   if ! out=$($CC -I. -Ilibretro-common/include -Wall \
         -Werror=implicit-function-declaration -Werror=implicit-int \
         -DHAVE_THREADS -DHAVE_NETWORKING -fsyntax-only "$tu" 2>&1); then
      echo "FAIL  $tu"
      echo "$out" | grep ": error: " | sed 's/^/      /' | head -8
      fail=1
   fi
done

if [ "$n" -eq 0 ]; then
   echo "no file includes retro_posix_source.h"
   exit 1
fi
[ "$fail" = 0 ] && echo "ok    $n files include retro_posix_source.h"
exit $fail
