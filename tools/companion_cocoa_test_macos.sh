#!/bin/sh
# The Cocoa companion harness on a real Mac: the same
# ui/companion/test/companion_cocoa_test.m as tools/companion_cocoa_test.sh
# runs under GNUstep on Linux, built here with Apple's clang against the
# Cocoa framework and run on the machine's window server (GitHub's macOS
# runners have one). Apple's AppKit is what decides key-window
# arbitration, appearance and the rest; GNUstep only approximates it.
#
#   tools/companion_cocoa_test_macos.sh            (on macOS)
set -eu
cd "$(dirname "$0")/.."

CC=${CC:-clang}
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

LC=libretro-common
INC="-I. -I$LC/include -Ideps -Igfx/include"
DEFS="-DRARCH_INTERNAL -DHAVE_MENU -DHAVE_CONFIGFILE -DHAVE_THREADS -DHAVE_COCOA -DTARGET_OS_OSX=1 -DHAVE_RPNG -DHAVE_RTGA -DHAVE_COMPANION_WIMP -DCOMPANION_TEST_NO_MAIN -DCOMPANION_CORE_TESTING"

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
   $CC -std=gnu99 -O1 -g -w $INC $DEFS -c "$f" -o "$o"
   OBJS="$OBJS $o"
done
# MRC (no -fobjc-arc): the file supports both; the GNUstep run is MRC too.
$CC -x objective-c -O1 -g -w $INC $DEFS -c ui/drivers/ui_cocoa_companion.m -o "$OUT/driver.o"
$CC -x objective-c -O1 -g -w $INC $DEFS -c ui/companion/test/companion_cocoa_test.m -o "$OUT/test.o"
$CC -o "$OUT/companion_cocoa_test" "$OUT/driver.o" "$OUT/test.o" $OBJS \
   -framework Cocoa -framework QuartzCore -lz -lpthread -lobjc

"$OUT/companion_cocoa_test"
