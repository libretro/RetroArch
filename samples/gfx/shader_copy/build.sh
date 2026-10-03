#!/bin/sh
# Builds the shader copy-for-menu parity harness.
#
# Like playlist_nav, this links the SHIPPING objects of a built
# RetroArch with only main() replaced - nothing is stubbed.  The seam
# under test runs through rgui_frame's aspect-change detection, the staged config on the
# rgui handle, and the application rgui_render performs on the main
# thread before CMD_EVENT_VIDEO_SET_ASPECT_RATIO fires.
# real frame and render code can see whether the write lands in the
# configuration or dies with the frame's snapshot copy.
#
# Requires a completed NON-Qt build: with Qt enabled main() lives in
# ui_qt.o, which drags the whole Qt UI in with it.  From the repo
# root:
#
#   ./configure --disable-qt && make
#   samples/gfx/shader_copy/build.sh
#
# retroarch.c is recompiled here with main renamed, so the harness can
# supply its own without having to exclude the object that holds most
# of the frontend.  Both the compile and the link reuse the project's
# own command lines, taken from make -n, so the harness cannot drift
# away from how the program is really built.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)

cd "$root"

if [ ! -f obj-unix/release/retroarch.o ]; then
   echo "build RetroArch first: ./configure --disable-qt && make" >&2
   exit 1
fi

cc_line=$(mktemp)
ld_line=$(mktemp)
trap 'rm -f "$cc_line" "$ld_line"' EXIT

# The project's own compile line for retroarch.o ...
touch retroarch.c
make -n 2>/dev/null | grep -E '\-o obj-unix/release/retroarch\.o' | head -1 > "$cc_line"
if [ ! -s "$cc_line" ]; then
   echo "could not determine the compile command for retroarch.o" >&2
   exit 1
fi

# ... reused twice: once for retroarch.c with main renamed out of the
# way, once for the harness itself, so both are built exactly the way
# the frontend is.
sed 's#-o obj-unix/release/retroarch\.o#-Dmain=rarch_harness_unused_main -o samples/gfx/shader_copy/retroarch_nomain.o#' \
   "$cc_line" | sh

sed -e 's#-o obj-unix/release/retroarch\.o#-o samples/gfx/shader_copy/harness_main.o#' \
    -e 's# retroarch\.c# samples/gfx/shader_copy/shader_copy_test.c#' \
   "$cc_line" | sh

# The project's own link line, with retroarch.o swapped for the pair
# above.  Removing the binary first is what makes make -n emit it.
rm -f retroarch
make -n 2>/dev/null | grep -E ' -o retroarch ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -e 's#obj-unix/release/retroarch\.o#samples/gfx/shader_copy/retroarch_nomain.o samples/gfx/shader_copy/harness_main.o#' \
    -e 's#-o retroarch #-o samples/gfx/shader_copy/shader_copy_test #' \
   "$ld_line" | sh

echo "built samples/gfx/shader_copy/shader_copy_test"
