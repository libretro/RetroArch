#!/bin/sh
# The tree's own headers must be found before any header of the same name
# on the system, whichever way the system path reached the compile line:
# an absolute include dir configure wrote to config.mk (/usr/local/include
# on the BSDs), or a pkg-config *_CFLAGS in use.
#
# Plants a libretro.h, boolean.h, retro_inline.h and compat/strl.h that
# #error when included in two system-style dirs, puts one dir in each
# place, and compiles retroarch.c with the project's own compile line.
#
# Needs a configured tree (./configure has run).
#
# Usage: tools/include_order_check.sh
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

if [ ! -f config.mk ]; then
   echo "no config.mk: run ./configure first" >&2
   exit 1
fi

work=$(mktemp -d)
cp config.mk "$work/config.mk.orig"
trap 'cp "$work/config.mk.orig" config.mk; rm -rf "$work"' EXIT INT TERM

for d in configured pkgconfig; do
   mkdir -p "$work/$d/compat"
   for h in libretro.h boolean.h retro_inline.h compat/strl.h; do
      printf '#error "%s from the %s system dir was picked over the tree'"'"'s"\n' \
         "$h" "$d" > "$work/$d/$h"
   done
done

# A pkg-config flag the build is really using: the first of these that
# configure enabled.
pkg=''
for p in FREETYPE X11 PULSE ALSA UDEV DBUS WAYLAND; do
   if grep -q "^HAVE_$p = 1" config.mk; then
      pkg=$p
      break
   fi
done

{
   echo "INCLUDE_DIRS += -I$work/configured"
   [ -n "$pkg" ] && echo "${pkg}_CFLAGS += -I$work/pkgconfig"
} >> config.mk

line=$(make -n -W retroarch.c 2>/dev/null \
   | grep -E -- '-o obj-unix/[^ ]+/retroarch\.o' | head -1)
if [ -z "$line" ]; then
   echo "FAIL  could not determine the compile line for retroarch.o" >&2
   exit 1
fi
case "$line" in
   *"$work/configured"*) ;;
   *) echo "FAIL  the configured include dir did not reach the compile line" >&2
      exit 1 ;;
esac
if [ -n "$pkg" ]; then
   case "$line" in
      *"$work/pkgconfig"*) ;;
      *) echo "FAIL  ${pkg}_CFLAGS did not reach the compile line" >&2
         exit 1 ;;
   esac
fi

cmd=$(printf '%s\n' "$line" \
   | sed -e 's# -o obj-unix/[^ ]*/retroarch\.o# -fsyntax-only#' \
         -e 's# -MMD##' -e 's# -MT [^ ]*##')
if ! out=$(eval "$cmd" 2>&1); then
   echo "FAIL  a system header was picked over the tree's:"
   printf '%s\n' "$out" | grep -E 'error' | head -8 | sed 's/^/      /'
   exit 1
fi

echo "ok    the tree's headers come first (configured dir${pkg:+, ${pkg}_CFLAGS})"
