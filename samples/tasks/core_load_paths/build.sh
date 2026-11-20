#!/bin/sh
# Builds the load paths harness: real loads through the load check on
# the shipping objects of a build with the load check (the default),
# core_signature.c compiled again with the public test key, main()
# replaced. Requires a completed NON-Qt build (with Qt enabled main()
# lives in ui_qt.o and drags the Qt UI in).
#
#   ./configure --disable-qt
#   make
#   samples/tasks/core_load_paths/build.sh
#   make -C samples/tasks/core_load_paths check
#
# MAKE_ARGS selects the tree build to link (for a sanitizer build, the
# same make arguments that produced it). The compile and the link reuse
# the project's own command lines, taken from make -n.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=samples/tasks/core_load_paths
MAKE_ARGS=${MAKE_ARGS:-}

cd "$root"

cc_line=$(mktemp)
ld_line=$(mktemp)
sig_line=$(mktemp)
cocoa_line=$(mktemp)
trap 'rm -f "$cc_line" "$ld_line" "$sig_line" "$cocoa_line"' EXIT

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
if [ ! -f "$objdir/core_load_check.o" ]; then
   echo "no core_load_check.o: configure without --disable-core_load_check" >&2
   exit 1
fi
if grep -qE '^HAVE_QT[[:space:]]*=[[:space:]]*1' config.mk 2>/dev/null; then
   echo "a Qt build: main() lives in ui_qt.o; build a tree configured with --disable-qt (a separate worktree keeps this one)" >&2
   exit 1
fi

# SDL2's cflags may rename main already (-Dmain=SDL_main)
sed -e 's# -Dmain=[^ ]*##g' \
    -e "s#-o $objdir/retroarch\.o#-Dmain=rarch_harness_unused_main -o $out/retroarch_nomain.o#" \
   "$cc_line" | sh

# A forced quick check for the null-load-core mode; Darwin's ld has
# no --wrap
wrap_cflags=
wrap_ldflags=
if [ "$(uname -s)" != Darwin ]; then
   wrap_cflags=-DCORE_LOAD_PATHS_WRAP
   wrap_ldflags=-Wl,--wrap=core_check_quick,--wrap=libretro_get_system_info,--wrap=core_info_find
fi

sed -e "s#-o $objdir/retroarch\.o#$wrap_cflags -o $out/harness_main.o#" \
    -e "s# retroarch\.c# $out/core_load_paths_test.c#" \
   "$cc_line" | sh

touch core_signature.c
make -n $MAKE_ARGS 2>/dev/null | grep -E "\-o $objdir/core_signature\.o" | head -1 > "$sig_line"
if [ ! -s "$sig_line" ]; then
   echo "could not determine the compile command for core_signature.o" >&2
   exit 1
fi
sed "s#-o $objdir/core_signature\.o#-DHAVE_CORE_SIGNING_TEST_KEY -o $out/core_signature_testkey.o#" \
   "$sig_line" | sh

# macOS: main() is ui_cocoa.m's (HAVE_MAIN)
if [ -f "$objdir/ui/drivers/ui_cocoa.o" ]; then
   touch ui/drivers/ui_cocoa.m
   make -n $MAKE_ARGS 2>/dev/null | grep -E "\-o $objdir/ui/drivers/ui_cocoa\.o" | head -1 > "$cocoa_line"
   if [ ! -s "$cocoa_line" ]; then
      echo "could not determine the compile command for ui_cocoa.o" >&2
      exit 1
   fi
   sed "s#-o $objdir/ui/drivers/ui_cocoa\.o#-Dmain=rarch_harness_unused_cocoa_main -o $out/ui_cocoa_nomain.o#" \
      "$cocoa_line" | sh
fi

rm -f retroarch retroarch_debug
make -n $MAKE_ARGS 2>/dev/null | grep -E ' -o retroarch(_debug)? ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -e "s#$objdir/retroarch\.o#$out/retroarch_nomain.o $out/harness_main.o#" \
    -e "s#$objdir/core_signature\.o#$out/core_signature_testkey.o#" \
    -e "s#$objdir/ui/drivers/ui_cocoa\.o#$out/ui_cocoa_nomain.o#" \
    -e "s#-o retroarch #$wrap_ldflags -o $out/core_load_paths_test #" \
    -e "s#-o retroarch_debug #$wrap_ldflags -o $out/core_load_paths_test #" \
   "$ld_line" | sh

# a Windows link names the binary .exe
exe=$out/core_load_paths_test
[ -f $exe ] || exe=$exe.exe
for f in $out/retroarch_nomain.o $out/harness_main.o \
      $out/core_signature_testkey.o $exe; do
   if [ ! -f "$f" ]; then
      echo "build.sh: $f was not produced" >&2
      exit 1
   fi
done

core_cc=$(awk '{print $1}' "$cc_line")
$core_cc -O1 -g -shared -fPIC -Ilibretro-common/include \
   -o $out/harness_core.so samples/tasks/content_load/harness_core.c
$core_cc -O1 -g -shared -fPIC -Ilibretro-common/include -DHARNESS_CORE_NO_HW \
   -o $out/harness_core_b.so samples/tasks/content_load/harness_core.c
$core_cc -O1 -g -shared -fPIC -Ilibretro-common/include -DHARNESS_CORE_NO_HW \
   -DHARNESS_CORE_SUBSYSTEM='"harness_sub_a"' \
   -o $out/harness_core_sub_a.so samples/tasks/content_load/harness_core.c
$core_cc -O1 -g -shared -fPIC -Ilibretro-common/include -DHARNESS_CORE_NO_HW \
   -DHARNESS_CORE_SUBSYSTEM='"harness_sub_b"' \
   -o $out/harness_core_sub_b.so samples/tasks/content_load/harness_core.c
$core_cc -O1 -g -shared -fPIC -Ilibretro-common/include \
   -o $out/needs_content_core.so $out/needs_content_core.c

echo "built $out/core_load_paths_test and its cores"
