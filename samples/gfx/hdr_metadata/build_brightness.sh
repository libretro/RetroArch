#!/bin/sh
# Links the shipping frontend objects with a test main.
# Requires a built non-Qt tree; MAKE_ARGS selects its build flags.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=samples/gfx/hdr_metadata
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
    -e "s# retroarch\.c# $out/brightness_test.c#" \
   "$cc_line" | sh

rm -f retroarch retroarch_debug retroarch.exe retroarch_debug.exe
make -n $MAKE_ARGS 2>/dev/null | grep -E ' -o retroarch(_debug)?(\.exe)? ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -e "s#$objdir/retroarch\.o#$out/retroarch_nomain.o $out/harness_main.o#" \
    -e "s#-o retroarch\(_debug\)\?\(\.exe\)\? #-o $out/brightness_test\2 #" \
   "$ld_line" | sh


echo "built native HDR brightness harness"
