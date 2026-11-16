#!/bin/sh
# Builds the message box layout harness.
#
# Like menu_icon_ownership, this links the SHIPPING objects of a built
# RetroArch with only main() replaced - nothing is stubbed.  Only the
# display driver and its font backend are the harness's, so every
# piece of text the menu drivers draw can be recorded.
#
# Requires a completed NON-Qt build: with Qt enabled main() lives in
# ui_qt.o, which drags the whole Qt UI in with it.  From the repo
# root:
#
#   ./configure --disable-qt && make
#   samples/menu/msgbox_layout/build.sh
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
sed 's#-o obj-unix/release/retroarch\.o#-Dmain=rarch_harness_unused_main -o samples/menu/msgbox_layout/retroarch_nomain.o#' \
   "$cc_line" | sh

sed -e 's#-o obj-unix/release/retroarch\.o#-o samples/menu/msgbox_layout/harness_main.o#' \
    -e 's# retroarch\.c# samples/menu/msgbox_layout/msgbox_layout_test.c#' \
   "$cc_line" | sh

# The project's own link line, with retroarch.o swapped for the pair
# above.  Removing the binary first is what makes make -n emit it.
rm -f retroarch
make -n 2>/dev/null | grep -E ' -o retroarch ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -e 's#obj-unix/release/retroarch\.o#samples/menu/msgbox_layout/retroarch_nomain.o samples/menu/msgbox_layout/harness_main.o#' \
    -e 's#-o retroarch #-o samples/menu/msgbox_layout/msgbox_layout_test -Wl,--wrap=gfx_display_draw_quad #' \
   "$ld_line" | sh

echo "built samples/menu/msgbox_layout/msgbox_layout_test"
