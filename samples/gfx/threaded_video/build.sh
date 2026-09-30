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
   # Usually the tree is not configured, or config.mk has fallen
   # behind configure after a pull: make says which.
   make -n $MAKE_ARGS 2>&1 | grep -iE 'config\.mk|error' | head -3 >&2
   echo "(a configured, built Makefile tree is needed: ./configure --disable-qt && make $MAKE_ARGS)" >&2
   exit 1
fi
objdir=$(grep -oE 'obj-unix/[^/ ]+/retroarch\.o' "$cc_line" | head -1 | sed 's#/retroarch\.o##')
if [ ! -f "$objdir/video_driver.o" ] && [ ! -f "$objdir/gfx/video_driver.o" ]; then
   echo "build RetroArch first: ./configure --disable-qt && make $MAKE_ARGS" >&2
   exit 1
fi

# On macOS main() is Cocoa's (HAVE_MAIN in ui_cocoa.m) and it calls
# rarch_main() with the application up; the harness keeps that main
# and takes rarch_main() over (harness_cocoa.m). Elsewhere main() is
# retroarch.c's and the harness's own replaces it.
cocoa_objs=""
if grep -q -- '-DHAVE_MAIN' "$cc_line"; then
   sed "s#-o $objdir/retroarch\.o#-Drarch_main=rarch_harness_unused_rarch_main -o $out/retroarch_nomain.o#" \
      "$cc_line" | sh

   sed -e "s#-o $objdir/retroarch\.o#-Dmain=harness_main -o $out/harness_main.o#" \
       -e "s# retroarch\.c# $out/threaded_video_test.c#" \
      "$cc_line" | sh

   # The tree's own compile line for an Objective-C file.
   touch ui/drivers/ui_cocoa.m
   make -n $MAKE_ARGS 2>/dev/null | grep -E '\-o obj-unix/[^/ ]+/ui/drivers/ui_cocoa\.o' | head -1 > "$ld_line"
   if [ ! -s "$ld_line" ]; then
      echo "could not determine the compile command for ui_cocoa.o" >&2
      exit 1
   fi
   sed -e "s#-o $objdir/ui/drivers/ui_cocoa\.o#-o $out/harness_cocoa.o#" \
       -e "s# ui/drivers/ui_cocoa\.m# $out/harness_cocoa.m#" \
      "$ld_line" | sh
   cocoa_objs="$out/harness_cocoa.o"

   # The Metal driver loads default.metallib from the executable's
   # own directory (a bare binary has no .app bundle for
   # newDefaultLibrary to look in). The tree builds it next to
   # retroarch; the harness binary needs it next to itself.
   if [ -f default.metallib ]; then
      cp -f default.metallib $out/default.metallib
   else
      echo "build.sh: no default.metallib in the tree - make builds it next to retroarch; the Metal driver cannot come up without it" >&2
      exit 1
   fi
else
   sed "s#-o $objdir/retroarch\.o#-Dmain=rarch_harness_unused_main -o $out/retroarch_nomain.o#" \
      "$cc_line" | sh

   sed -e "s#-o $objdir/retroarch\.o#-o $out/harness_main.o#" \
       -e "s# retroarch\.c# $out/threaded_video_test.c#" \
      "$cc_line" | sh
fi

rm -f retroarch retroarch_debug
make -n $MAKE_ARGS 2>/dev/null | grep -E ' -o retroarch(_debug)? ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -e "s#$objdir/retroarch\.o#$out/retroarch_nomain.o $out/harness_main.o $cocoa_objs#" \
    -e "s#-o retroarch #-o $out/threaded_video_test #" \
    -e "s#-o retroarch_debug #-o $out/threaded_video_test #" \
   "$ld_line" | sh

# Every substitution above is checked by its result. The link line
# used to be rewritten with \?, which BSD sed (macOS) does not take:
# it left the line alone, the link produced retroarch in the tree
# root, and this script said "built". Plain patterns now, and a
# missing object or binary fails here.
for f in $out/retroarch_nomain.o $out/harness_main.o $cocoa_objs $out/threaded_video_test; do
   if [ ! -f "$f" ]; then
      echo "build.sh: $f was not produced - a compile or link line was not rewritten as intended" >&2
      exit 1
   fi
done

# The harness core: a plain shared library, no sanitizer, so that what
# the sanitizer reports is the frontend. Built with the compiler the
# tree's own compile line names, so a cross build (mingw) gets a core
# the frontend can load; the .so name is only a name, every loader
# takes it.
core_cc=$(awk '{print $1}' "$cc_line")
# gfx/include carries the tree's own Vulkan headers, for the core's
# hardware mode (HARNESS_CORE_HW_VULKAN); it calls Vulkan through the
# frontend's interface and links nothing.
$core_cc -O1 -g -shared -fPIC -Ilibretro-common/include -Igfx/include -o $out/harness_core.so $out/harness_core.c
[ -f $out/harness_core.so ] || { echo "build.sh: $out/harness_core.so was not produced" >&2; exit 1; }

echo "built $out/threaded_video_test and $out/harness_core.so"
