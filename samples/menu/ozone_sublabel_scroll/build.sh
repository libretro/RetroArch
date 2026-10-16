#!/bin/sh
# Builds the ozone sublabel scroll harness.
#
# Like ozone_color_theme_defer, this links the SHIPPING objects of a
# built RetroArch with only main() replaced.  The harness also
# includes menu/drivers/ozone.c, so it can read the ozone handle; the
# shipping ozone.o is left out of the link in its favour.
#
# Requires a completed NON-Qt build.  From the repo root:
#
#   ./configure --disable-qt && make
#   samples/menu/ozone_sublabel_scroll/build.sh
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
dir=samples/menu/ozone_sublabel_scroll

cd "$root"

if [ ! -f obj-unix/release/retroarch.o ] \
      || [ ! -f obj-unix/release/menu/drivers/ozone.o ]; then
   echo "build RetroArch first: ./configure --disable-qt && make" >&2
   exit 1
fi

cc_line=$(mktemp)
ld_line=$(mktemp)
trap 'rm -f "$cc_line" "$ld_line"' EXIT

touch retroarch.c
make -n 2>/dev/null | grep -E '\-o obj-unix/release/retroarch\.o' | head -1 > "$cc_line"
if [ ! -s "$cc_line" ]; then
   echo "could not determine the compile command for retroarch.o" >&2
   exit 1
fi

sed "s#-o obj-unix/release/retroarch\.o#-Dmain=rarch_harness_unused_main -o $dir/retroarch_nomain.o#" \
   "$cc_line" | sh

sed -e "s#-o obj-unix/release/retroarch\.o#-o $dir/harness_main.o#" \
    -e "s# retroarch\.c# $dir/ozone_sublabel_scroll_test.c#" \
   "$cc_line" | sh

rm -f retroarch
make -n 2>/dev/null | grep -E ' -o retroarch ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -e "s#obj-unix/release/retroarch\.o#$dir/retroarch_nomain.o $dir/harness_main.o#" \
    -e "s#obj-unix/release/menu/drivers/ozone\.o##" \
    -e "s#-o retroarch #-o $dir/ozone_sublabel_scroll_test #" \
   "$ld_line" | sh

echo "built $dir/ozone_sublabel_scroll_test"
