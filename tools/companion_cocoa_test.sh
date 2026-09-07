#!/bin/sh
# Regression harness for the Cocoa companion, on Linux: the real
# ui/drivers/ui_cocoa_companion.m compiled and linked against GNUstep's
# Foundation + AppKit, with the real companion core, the core test's
# stubs and fixtures, and a stub platform (a window with a render view)
# - run headless under Xvfb. See companion_cocoa_test.m for what it
# asserts.
#
# Needs: gobjc, gnustep-devel, gnustep-back-common, xvfb (Debian/Ubuntu:
#   apt-get install gobjc gnustep-devel gnustep-back-common xvfb)
#
#   tools/companion_cocoa_test.sh
set -eu
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
if ! command -v gnustep-config >/dev/null 2>&1; then
   echo "gnustep-config not found: install gnustep-devel" >&2
   exit 2
fi

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

# Apple-only headers and macros the driver includes, shimmed for GNUstep.
SHIM="$OUT/shim"
mkdir -p "$SHIM/objc" "$SHIM/QuartzCore"
printf '#include <objc/runtime.h>\n#include <objc/message.h>\n' > "$SHIM/objc/objc-runtime.h"
printf '#include <Foundation/Foundation.h>\n' > "$SHIM/QuartzCore/QuartzCore.h"
cat > "$SHIM/TargetConditionals.h" <<'EOF'
#define TARGET_OS_MAC 1
#define TARGET_OS_OSX 1
#define TARGET_OS_IPHONE 0
#define TARGET_OS_IOS 0
#define TARGET_OS_TV 0
#define TARGET_OS_SIMULATOR 0
EOF
cat > "$SHIM/AvailabilityMacros.h" <<'EOF'
/* GNUstep defines the MAC_OS_X_VERSION_10_x constants itself (its own
 * scale: 10.12 is 1120); take its newest API level. */
#include <GNUstepBase/GSVersionMacros.h>
#ifndef MAC_OS_X_VERSION_MAX_ALLOWED
#define MAC_OS_X_VERSION_MAX_ALLOWED MAC_OS_X_VERSION_10_14
#endif
#ifndef MAC_OS_X_VERSION_MIN_REQUIRED
#define MAC_OS_X_VERSION_MIN_REQUIRED MAC_OS_X_VERSION_10_14
#endif
#define API_DEPRECATED(...)
#define API_AVAILABLE(...)
#include <AppKit/NSGraphics.h>
#ifndef NSCompositingOperationSourceOver
#define NSCompositingOperationSourceOver NSCompositeSourceOver
#endif
EOF

LC=libretro-common
INC="-I$SHIM -I. -I$LC/include -Ideps -Igfx/include"
DEFS="-DRARCH_INTERNAL -DLIBRETRO_STRL_CHECK_OVERLAP -DHAVE_MENU -DHAVE_CONFIGFILE -DHAVE_THREADS -DHAVE_COCOA -DTARGET_OS_OSX=1 -DHAVE_RPNG -DHAVE_RTGA -DHAVE_COMPANION_WIMP -DCOMPANION_TEST_NO_MAIN -DCOMPANION_CORE_TESTING"
SAN=""
case "${1:-}" in
   asan) SAN="-fsanitize=address -fno-omit-frame-pointer" ;;
   "")   ;;
   *)    echo "usage: $0 [asan]" >&2; exit 2 ;;
esac
OBJCFLAGS="$(gnustep-config --objc-flags) -Wall -Wno-unused-parameter -Wno-multichar $SAN"
LDFLAGS="$(gnustep-config --gui-libs) -lpthread -lm -lz"

# C sources shared with the core test (its stubs and fixtures included)
CSRCS="ui/companion/companion_core.c \
       ui/companion/companion_thumbs.c \
       gfx/gfx_anim_preview.c \
       ui/companion/test/companion_core_stubs.c \
       ui/companion/test/companion_core_test.c \
       playlist.c core_option_manager.c $LC/lists/nested_list.c $LC/file/config_file_io.c \
       $LC/formats/json/rjson.c $LC/formats/image_texture.c $LC/formats/image_transfer.c \
       $LC/formats/data_transfer.c $LC/formats/png/rpng.c $LC/formats/png/rpng_apng.c \
       $LC/formats/png/rpng_encode.c $LC/file/rpng_file.c $LC/formats/tga/rtga.c $LC/memmap/memmap.c $LC/memory/mem_stats.c \
       $LC/streams/file_stream.c $LC/streams/file_stream_transforms.c $LC/streams/interface_stream.c \
       $LC/streams/memory_stream.c $LC/streams/rzip_stream.c $LC/streams/trans_stream.c \
       $LC/streams/trans_stream_pipe.c $LC/streams/trans_stream_zlib.c $LC/streams/trans_stream_deflate.c \
       $LC/vfs/vfs_implementation.c $LC/file/file_path.c $LC/file/file_path_io.c $LC/file/config_file.c \
       $LC/file/archive_file.c $LC/file/archive_file_zlib.c $LC/file/retro_dirent.c \
       $LC/lists/dir_list.c $LC/lists/string_list.c $LC/lists/file_list.c \
       $LC/string/stdstring.c $LC/string/rstrtod.c $LC/compat/compat_strl.c $LC/compat/compat_strldup.c \
       $LC/compat/compat_posix_string.c $LC/compat/compat_strcasestr.c $LC/compat/fopen_utf8.c \
       $LC/encodings/encoding_utf.c $LC/encodings/encoding_crc32.c $LC/encodings/encoding_deflate.c \
       $LC/hash/lrc_hash.c $LC/time/rtime.c $LC/features/features_cpu.c $LC/rthreads/rthreads.c"

OBJS=""
for f in $CSRCS; do
   o="$OUT/$(echo "$f" | tr '/' '_').o"
   $CC -std=gnu99 -O1 -g -w $SAN $INC $DEFS -c "$f" -o "$o"
   OBJS="$OBJS $o"
done
$CC -x objective-c -O1 -g $OBJCFLAGS $INC $DEFS -c ui/drivers/ui_cocoa_companion.m -o "$OUT/driver.o"
$CC -x objective-c -O1 -g $OBJCFLAGS $INC $DEFS -c ui/companion/test/companion_cocoa_test.m -o "$OUT/test.o"
$CC $SAN -o "$OUT/companion_cocoa_test" "$OUT/driver.o" "$OUT/test.o" $OBJS $LDFLAGS -lobjc -lgnustep-base -lgnustep-gui

# Headless: GNUstep needs a display for AppKit; Xvfb provides one.
# GDB=1 runs under gdb and prints a backtrace on a fault.
RUN="$OUT/companion_cocoa_test"
if [ "${GDB:-0}" = 1 ]; then
   RUN="gdb -q -batch -ex run -ex bt -ex quit --args $OUT/companion_cocoa_test"
fi
if command -v xvfb-run >/dev/null 2>&1; then
   # The status must survive the filter, or a sanitizer abort would
   # look like a pass (it did: the run that first caught the macOS
   # heap-buffer-overflow still exited 0 through the pipe).
   set +e
   ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
      xvfb-run -a -s "-screen 0 1280x800x24" $RUN > "$OUT/run.log" 2>&1
   st=$?
   set -e
   grep -av "autorelease called without pool" "$OUT/run.log"
   [ "$st" = 0 ] || echo "(harness exited $st)"
   exit $st

else
   $RUN
fi
