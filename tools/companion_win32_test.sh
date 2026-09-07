#!/bin/sh
# Regression harness for the Win32 companion, on Linux: the real
# ui/drivers/ui_win32_companion.c compiled with mingw and linked with
# the real companion core, the core test's stubs and fixtures and a stub
# for RetroArch's main window - run under Wine on Xvfb. See
# companion_win32_test.c for what it asserts.
#
# Needs: gcc-mingw-w64-x86-64, wine, xvfb (Debian/Ubuntu:
#   apt-get install gcc-mingw-w64-x86-64 wine xvfb)
#
#   tools/companion_win32_test.sh
set -eu
cd "$(dirname "$0")/.."

CC=${CC:-x86_64-w64-mingw32-gcc}
WINE=${WINE:-wine}
command -v "$CC" >/dev/null 2>&1 || { echo "$CC not found" >&2; exit 2; }
command -v "$WINE" >/dev/null 2>&1 || { echo "$WINE not found" >&2; exit 2; }

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

LC=libretro-common
INC="-I. -I$LC/include -Ideps -Igfx/include"
DEFS="-DRARCH_INTERNAL -DLIBRETRO_STRL_CHECK_OVERLAP -DHAVE_MENU -DHAVE_CONFIGFILE -DHAVE_THREADS -DHAVE_RPNG -DHAVE_RTGA -DHAVE_COMPANION_WIMP -DCOMPANION_TEST_NO_MAIN -DCOMPANION_CORE_TESTING"

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
       $LC/streams/trans_stream_pipe.c $LC/streams/trans_stream_deflate.c \
       $LC/vfs/vfs_implementation.c $LC/file/file_path.c $LC/file/file_path_io.c $LC/file/config_file.c \
       $LC/file/archive_file.c $LC/file/archive_file_zlib.c $LC/file/retro_dirent.c \
       $LC/lists/dir_list.c $LC/lists/string_list.c $LC/lists/file_list.c \
       $LC/string/stdstring.c $LC/string/rstrtod.c $LC/compat/compat_strl.c $LC/compat/compat_strldup.c \
       $LC/compat/compat_posix_string.c $LC/compat/compat_strcasestr.c $LC/compat/fopen_utf8.c \
       $LC/encodings/encoding_utf.c $LC/encodings/encoding_crc32.c $LC/encodings/encoding_deflate.c \
       $LC/hash/lrc_hash.c $LC/time/rtime.c $LC/features/features_cpu.c $LC/rthreads/rthreads.c"

OBJS=""
for f in $CSRCS ui/drivers/ui_win32_companion.c ui/companion/test/companion_win32_test.c; do
   o="$OUT/$(echo "$f" | tr '/' '_').o"
   $CC -std=gnu99 -O1 -g -w $INC $DEFS -c "$f" -o "$o"
   OBJS="$OBJS $o"
done
$CC -o "$OUT/companion_win32_test.exe" $OBJS -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lgdi32 -luser32 -lws2_32 -lpthread

# Headless: Wine needs a display; Xvfb provides one. Wine's own noise
# on stderr is dropped; the harness prints to stdout.
export WINEDEBUG=-all
if command -v xvfb-run >/dev/null 2>&1; then
   set +e
   xvfb-run -a -s "-screen 0 1280x800x24" "$WINE" "$OUT/companion_win32_test.exe" > "$OUT/run.log" 2>"$OUT/err.log"
   st=$?
   set -e
   tr -d '\r' < "$OUT/run.log"
   [ "$st" = 0 ] || { echo "(wine exit $st)"; grep -av "^wine:\|^00[0-9a-f]*:" "$OUT/err.log" | tail -5; }
   exit $st
else
   "$WINE" "$OUT/companion_win32_test.exe"
fi
