#!/bin/sh
# Builds the cloud sync unload harness.
#
# Like content_closing, this links the SHIPPING objects of a built
# RetroArch with only main() replaced, and one symbol wrapped:
# task_push_cloud_sync, so the harness sees each sync the frontend
# asks for, and what the save file holds at that moment, without a
# server.  The core it loads (sram_core.so) is built here too.
#
# Requires a completed NON-Qt build, from the repo root:
#
#   ./configure --disable-qt && make
#   samples/runloop/cloudsync_unload/build.sh
#
# The compile and link lines are the project's own, taken from make -n.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=samples/runloop/cloudsync_unload

cd "$root"

if [ ! -f obj-unix/release/retroarch.o ]; then
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

sed "s#-o obj-unix/release/retroarch\.o#-Dmain=rarch_harness_unused_main -o $out/retroarch_nomain.o#" \
   "$cc_line" | sh

sed -e "s#-o obj-unix/release/retroarch\.o#-o $out/harness_main.o#" \
    -e "s# retroarch\.c# $out/cloudsync_unload_test.c#" \
   "$cc_line" | sh

rm -f retroarch
make -n 2>/dev/null | grep -E ' -o retroarch ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -e "s#obj-unix/release/retroarch\.o#$out/retroarch_nomain.o $out/harness_main.o#" \
    -e "s#-o retroarch #-o $out/cloudsync_unload_test -Wl,--wrap=task_push_cloud_sync #" \
   "$ld_line" | sh

core_cc=$(sed 's/ .*//' "$cc_line")
$core_cc -O1 -g -shared -fPIC -Ilibretro-common/include \
   -o $out/sram_core.so $out/sram_core.c

echo "built $out/cloudsync_unload_test and $out/sram_core.so"
