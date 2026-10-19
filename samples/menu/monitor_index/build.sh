#!/bin/sh
# Builds the Monitor Index dropdown harness.
#
# Like network_settings, this links the SHIPPING objects of a built
# RetroArch with only main() replaced; the display server's output
# list is wrapped at link time (--wrap) so each lane decides how many
# monitors there are.  Works from a Linux shell or MSYS2 MINGW64.
#
# Requires a completed NON-Qt build: with Qt enabled main() lives in
# ui_qt.o, which drags the whole Qt UI in with it.  From the repo
# root:
#
#   ./configure --disable-qt && make
#   samples/menu/monitor_index/build.sh
#
# Both the compile and the link reuse the project's own command lines,
# taken from make -n, so the harness cannot drift away from how the
# program is really built.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=samples/menu/monitor_index

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
    -e "s# retroarch\.c# $out/monitor_index_test.c#" \
   "$cc_line" | sh

rm -f retroarch retroarch.exe
make -n 2>/dev/null | grep -E ' -o retroarch(\.exe)? ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -E -e "s#obj-unix/release/retroarch\.o#$out/retroarch_nomain.o $out/harness_main.o -Wl,--wrap=video_display_server_list_outputs#" \
    -e "s#-o retroarch(\.exe)? #-o $out/monitor_index_test\1 #" \
   "$ld_line" | sh

echo "built $out/monitor_index_test"
