#!/bin/sh
# Builds the video filter menu harness.
#
# Links the SHIPPING objects of a built RetroArch with only main()
# replaced - nothing is stubbed. Requires a completed NON-Qt build
# (with Qt enabled main() lives in ui_qt.o and drags the Qt UI in).
#
#   ./configure --disable-qt
#   make
#   samples/gfx/video_filter_menu/build.sh
#   samples/gfx/video_filter_menu/video_filter_menu_test
#
# MAKE_ARGS selects the tree build to link (for a sanitizer build, the
# same make arguments that produced it). The compile and the link reuse
# the project's own command lines, taken from make -n.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=samples/gfx/video_filter_menu
MAKE_ARGS=${MAKE_ARGS:-}

cd "$root"

cc_line=$(mktemp)
ld_line=$(mktemp)
trap 'rm -f "$cc_line" "$ld_line"' EXIT

touch retroarch.c
make -n $MAKE_ARGS 2>/dev/null | grep -E '\-o obj-unix/[^/ ]+/retroarch\.o' | head -1 > "$cc_line"
if [ ! -s "$cc_line" ]; then
   echo "could not determine the compile command for retroarch.o" >&2
   make -n $MAKE_ARGS 2>&1 | grep -iE 'config\.mk|error' | head -3 >&2
   echo "(a configured, built Makefile tree is needed: ./configure --disable-qt && make $MAKE_ARGS)" >&2
   exit 1
fi
objdir=$(grep -oE 'obj-unix/[^/ ]+/retroarch\.o' "$cc_line" | head -1 | sed 's#/retroarch\.o##')
if [ ! -f "$objdir/gfx/video_driver.o" ]; then
   echo "build RetroArch first: ./configure --disable-qt && make $MAKE_ARGS" >&2
   exit 1
fi

sed "s#-o $objdir/retroarch\.o#-Dmain=rarch_harness_unused_main -o $out/retroarch_nomain.o#" \
   "$cc_line" | sh

sed -e "s#-o $objdir/retroarch\.o#-o $out/harness_main.o#" \
    -e "s# retroarch\.c# $out/video_filter_menu_test.c#" \
   "$cc_line" | sh

rm -f retroarch retroarch_debug
make -n $MAKE_ARGS 2>/dev/null | grep -E ' -o retroarch(_debug)? ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -e "s#$objdir/retroarch\.o#$out/retroarch_nomain.o $out/harness_main.o#" \
    -e "s#-o retroarch #-o $out/video_filter_menu_test #" \
    -e "s#-o retroarch_debug #-o $out/video_filter_menu_test #" \
   "$ld_line" | sh

# The core and the filter plugins (Normal2x, Normal4x, and widen32, the test
# filter giving XRGB8888): plain shared libraries, no sanitizer, built
# with the compiler the tree's compile line names. Plugins are found
# next to the .filt, under the frontend's library extension.
core_cc=$(awk '{print $1}' "$cc_line")
case "$($core_cc -dumpmachine)" in
   *mingw*|*cygwin*|*windows*) ext=dll ;;
   *darwin*)                   ext=dylib ;;
   *)                          ext=so ;;
esac
$core_cc -O1 -g -shared -fPIC -Ilibretro-common/include -o $out/filter_core.so $out/filter_core.c
mkdir -p $out/filters
for f in normal2x normal4x; do
   $core_cc -O2 -shared -fPIC -Ilibretro-common/include \
      -o $out/filters/$f.$ext gfx/video_filters/$f.c
done
$core_cc -O2 -shared -fPIC -Ilibretro-common/include -Igfx/video_filters \
   -o $out/filters/widen32.$ext $out/widen32.c
cp gfx/video_filters/Normal2x.filt gfx/video_filters/Normal4x.filt $out/widen32.filt $out/filters/

for f in $out/retroarch_nomain.o $out/harness_main.o $out/video_filter_menu_test \
         $out/filter_core.so $out/filters/normal2x.$ext $out/filters/normal4x.$ext \
         $out/filters/widen32.$ext; do
   if [ ! -f "$f" ]; then
      echo "build.sh: $f was not produced" >&2
      exit 1
   fi
done

echo "built $out/video_filter_menu_test, $out/filter_core.so and $out/filters"
