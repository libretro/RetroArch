#!/bin/sh
# Syntax-check the Android-only translation units without an NDK.
#
# gfx/display_servers/dispserv_android.c and the other ANDROID-gated
# sources are not built by the desktop build, so a change to them
# compiles here and fails on a real Android build - which is a slow
# and irritating way to find a missing declaration.  The stubs beside
# this script declare just enough of the NDK surface (jni.h and the
# android/ headers platform_unix.h pulls in) to let the compiler
# parse them.
#
# This checks SYNTAX AND DECLARATIONS ONLY.  The stubs are not the
# real NDK: signatures are approximated and nothing is linked or run.
# It catches use-before-declaration, typos and missing includes - the
# class of mistake that has no business reaching a device build - and
# nothing beyond that.
#
# Usage, from the repo root:
#   samples/android/compile_check/check.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)

cd "$root"

FLAGS="-fsyntax-only -std=gnu99 -DANDROID -D__ANDROID_API__=30
       -D_GNU_SOURCE -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_THREADS
       -DDEFAULT_MAX_PADS=16 -DPROP_VALUE_MAX=92
       -I$here/ndkstub -Ilibretro-common/include -I. -Ideps"

status=0

for src in gfx/display_servers/dispserv_android.c; do
   printf '%-48s ' "$src"
   if ${CC:-gcc} $FLAGS "$src" 2>/tmp/android_check.$$; then
      echo "ok"
   else
      echo "FAILED"
      cat /tmp/android_check.$$ >&2
      status=1
   fi
   rm -f /tmp/android_check.$$
done

exit $status
