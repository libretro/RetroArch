#!/bin/sh
# Syntax-checks the 3DS arms of libretro-common's threading units against
# real libctru headers, old and new, with the host's identity macros shed
# so every #if ladder takes the 3DS branch.  The legacy buildbot image
# builds against libctru 1.x, where the sync*Address wrappers do not
# exist; libctru 2 is checked with USE_CTRULIB_2 as Makefile.ctr sets it.
# Each unit is checked alone and as one translation unit with
# rthreads.c, the shape griffin builds.
#
# Run from the RetroArch root:
#   tools/ctr-libctru-compat-check.sh [libctru-tag ...]
# Default tags: v1.6.0 v2.4.1.  Needs gcc, git and network access.

set -e

[ $# -gt 0 ] || set -- v1.6.0 v2.4.1

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# newlib's sys/lock.h, which libctru's synchronization.h includes
mkdir -p "$WORK/newlib/sys"
cat > "$WORK/newlib/sys/lock.h" <<'EOF'
#ifndef __SYS_LOCK_H__
#define __SYS_LOCK_H__
#include <stdint.h>
typedef int32_t _LOCK_T;
typedef struct { _LOCK_T lock; uint32_t thread_tag; uint32_t counter; } _LOCK_RECURSIVE_T;
#endif
EOF

printf '#include "libretro-common/rthreads/retro_eventcount.c"\n' \
   > "$WORK/unity.c"
printf '#include "libretro-common/rthreads/rthreads.c"\n' \
   >> "$WORK/unity.c"

UNITS="libretro-common/rthreads/retro_eventcount.c
libretro-common/rthreads/rthreads.c
$WORK/unity.c"

CFLAGS="-fsyntax-only -Wall -Werror=implicit-function-declaration \
   -D_3DS -DARM11 -D__3DS__ -DHAVE_THREADS -Wno-pointer-to-int-cast \
   -U__linux__ -U__gnu_linux__ -Ulinux -U__unix__ -U__unix -Uunix \
   -Ilibretro-common/include -I. -I$WORK/newlib"

status=0
for tag in "$@"; do
   dir="$WORK/libctru-$tag"
   git -c advice.detachedHead=false clone -q --depth 1 --branch "$tag" \
      https://github.com/devkitPro/libctru.git "$dir"
   inc="$dir/libctru/include"
   defs=""
   if grep -q gspPresentBuffer "$inc/3ds/services/gspgpu.h"; then
      defs="-DUSE_CTRULIB_2"
   fi
   for unit in $UNITS; do
      if gcc $CFLAGS $defs -I"$inc" "$unit"; then
         echo "ok    libctru $tag $defs $unit"
      else
         echo "FAIL  libctru $tag $defs $unit"
         status=1
      fi
   done
done
exit $status
