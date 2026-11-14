#!/bin/sh
# Builds the Security Warning harness.
#
# Links the SHIPPING objects of a built RetroArch with only main()
# replaced, and core_signature.c compiled again with the public test
# key, so the trust store is active in a default build. Requires a
# completed NON-Qt build (with Qt enabled main() lives in ui_qt.o and
# drags the Qt UI in).
#
#   ./configure --disable-qt
#   make
#   samples/menu/core_warning/build.sh
#   samples/menu/core_warning/core_warning_test
#
# MAKE_ARGS selects the tree build to link (for a sanitizer build, the
# same make arguments that produced it). The compile and the link reuse
# the project's own command lines, taken from make -n.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out=samples/menu/core_warning
MAKE_ARGS=${MAKE_ARGS:-}

cd "$root"

cc_line=$(mktemp)
ld_line=$(mktemp)
sig_line=$(mktemp)
trap 'rm -f "$cc_line" "$ld_line" "$sig_line"' EXIT

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

# On macOS main() is Cocoa's (HAVE_MAIN in ui_cocoa.m) and it calls
# rarch_main() with the application up; the harness keeps that main
# and takes rarch_main() over, with threaded_video's harness_cocoa.m.
# Elsewhere main() is retroarch.c's and the harness's own replaces it.
cocoa_objs=""
if grep -q -- '-DHAVE_MAIN' "$cc_line"; then
   sed "s#-o $objdir/retroarch\.o#-Drarch_main=rarch_harness_unused_rarch_main -o $out/retroarch_nomain.o#" \
      "$cc_line" | sh

   sed -e "s#-o $objdir/retroarch\.o#-Dmain=harness_main -o $out/harness_main.o#" \
       -e "s# retroarch\.c# $out/core_warning_test.c#" \
      "$cc_line" | sh

   touch ui/drivers/ui_cocoa.m
   make -n $MAKE_ARGS 2>/dev/null | grep -E '\-o obj-unix/[^/ ]+/ui/drivers/ui_cocoa\.o' | head -1 > "$ld_line"
   if [ ! -s "$ld_line" ]; then
      echo "could not determine the compile command for ui_cocoa.o" >&2
      exit 1
   fi
   sed -e "s#-o $objdir/ui/drivers/ui_cocoa\.o#-o $out/harness_cocoa.o#" \
       -e "s# ui/drivers/ui_cocoa\.m# samples/gfx/threaded_video/harness_cocoa.m#" \
      "$ld_line" | sh
   cocoa_objs="$out/harness_cocoa.o"

   # The Metal driver loads default.metallib from beside the binary
   if [ -f default.metallib ]; then
      cp -f default.metallib $out/default.metallib
   fi
else
   sed "s#-o $objdir/retroarch\.o#-Dmain=rarch_harness_unused_main -o $out/retroarch_nomain.o#" \
      "$cc_line" | sh

   sed -e "s#-o $objdir/retroarch\.o#-o $out/harness_main.o#" \
       -e "s# retroarch\.c# $out/core_warning_test.c#" \
      "$cc_line" | sh
fi

touch core_signature.c
make -n $MAKE_ARGS 2>/dev/null | grep -E "\-o $objdir/core_signature\.o" | head -1 > "$sig_line"
if [ ! -s "$sig_line" ]; then
   echo "could not determine the compile command for core_signature.o" >&2
   exit 1
fi
sed "s#-o $objdir/core_signature\.o#-DHAVE_CORE_SIGNING_TEST_KEY -o $out/core_signature_testkey.o#" \
   "$sig_line" | sh

rm -f retroarch retroarch_debug
make -n $MAKE_ARGS 2>/dev/null | grep -E ' -o retroarch(_debug)? ' | tail -1 > "$ld_line"
if [ ! -s "$ld_line" ]; then
   echo "could not determine the link command" >&2
   exit 1
fi

sed -e "s#$objdir/retroarch\.o#$out/retroarch_nomain.o $out/harness_main.o $cocoa_objs#" \
    -e "s#$objdir/core_signature\.o#$out/core_signature_testkey.o#" \
    -e "s#-o retroarch #-o $out/core_warning_test #" \
    -e "s#-o retroarch_debug #-o $out/core_warning_test #" \
   "$ld_line" | sh

# a Windows link names the binary .exe
exe=$out/core_warning_test
[ -f $exe ] || exe=$exe.exe
for f in $out/retroarch_nomain.o $out/harness_main.o \
      $out/core_signature_testkey.o $exe; do
   if [ ! -f "$f" ]; then
      echo "build.sh: $f was not produced" >&2
      exit 1
   fi
done

echo "built $out/core_warning_test"
