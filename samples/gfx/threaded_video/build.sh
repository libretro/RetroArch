#!/bin/sh
# Builds the threaded video harness.
#
# Links the SHIPPING objects of a built RetroArch with only main()
# replaced - nothing is stubbed. Requires a completed NON-Qt build
# (with Qt enabled main() lives in ui_qt.o and drags the Qt UI in).
#
# The harness is meant to run under a sanitizer, so the build it links
# is selected with the same make arguments used to produce it, passed
# through MAKE_ARGS:
#
#   ./configure --disable-qt
#   make -j8 SANITIZER=address,undefined DEBUG=1
#   MAKE_ARGS="SANITIZER=address,undefined DEBUG=1" \
#      samples/gfx/threaded_video/build.sh
#   samples/gfx/threaded_video/threaded_video_test [cycles]
#
# With MAKE_ARGS empty it links the plain release build. Both the
# compile and the link reuse the project's own command lines, taken
# from make -n, so the harness cannot drift from how the program is
# really built.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=samples/gfx/threaded_video
MAKE_ARGS=${MAKE_ARGS:-}

cd "$root"

cc_line=$(mktemp)
ld_line=$(mktemp)
trap 'rm -f "$cc_line" "$ld_line"' EXIT

# The project's own compile line for retroarch.o, which also tells us
# which object directory this configuration builds into.
touch retroarch.c
make -n $MAKE_ARGS 2>/dev/null | grep -E '\-o obj-unix/[^/ ]+/retroarch\.o' | head -1 > "$cc_line"
if [ ! -s "$cc_line" ]; then
   echo "could not determine the compile command for retroarch.o" >&2
   exit 1
fi
objdir=$(grep -oE 'obj-unix/[^/ ]+/retroarch\.o' "$cc_line" | head -1 | sed 's#/retroarch\.o##')
if [ ! -f "$objdir/video_driver.o" ] && [ ! -f "$objdir/gfx/video_driver.o" ]; then
   echo "build RetroArch first: ./configure --disable-qt && make $MAKE_ARGS" >&2
   exit 1
fi

sed "s#-o $objdir/retroarch\.o#-Dmain=rarch_harness_unused_main -o $out/retroarch_nomain.o#" \
   "$cc_line" | sh

sed -e "s#-o $objdir/retroarch\.o#-o $out/harness_main.o#" \
    -e "s# retroarch\.c# $out/threaded_video_test.c#" \
   "$cc_line" | sh

rm -f retroarch retroarch_debug
make -n $MAKE_ARGS 2>/dev/null | grep -E ' -o retroarch(_debug)? ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -e "s#$objdir/retroarch\.o#$out/retroarch_nomain.o $out/harness_main.o#" \
    -e "s#-o retroarch\(_debug\)\? #-o $out/threaded_video_test #" \
   "$ld_line" | sh

# The harness core: a plain shared library, no sanitizer, so that what
# the sanitizer reports is the frontend.
cc -O1 -g -shared -fPIC -Ilibretro-common/include -o $out/harness_core.so $out/harness_core.c

echo "built $out/threaded_video_test and $out/harness_core.so"
