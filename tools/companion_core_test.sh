#!/bin/sh
# Build and run the regression test for the desktop companions' shared
# core (ui/companion/companion_core.c), on Linux. Links the real core
# and playlist.c against libretro-common, with RetroArch's state
# replaced by fixtures. Pass "asan" to run under Address+UB sanitizer.
#
#   tools/companion_core_test.sh
#   tools/companion_core_test.sh asan
set -eu
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
SAN=""
case "${1:-}" in
   asan) SAN="-fsanitize=address,undefined -fno-omit-frame-pointer" ;;
   tsan) SAN="-fsanitize=thread" ;;
   "")   ;;
   *)    echo "usage: $0 [asan|tsan]" >&2; exit 2 ;;
esac

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

LC=libretro-common
INC="-I. -I$LC/include -Ideps -Igfx/include"
DEFS="-DRARCH_INTERNAL -DLIBRETRO_STRL_CHECK_OVERLAP -DHAVE_MENU -DHAVE_CONFIGFILE -DHAVE_THREADS -DHAVE_COMPANION_WIMP -DHAVE_RPNG -DHAVE_RTGA -DCOMPANION_CORE_TESTING"
SRCS="ui/companion/companion_core.c \
      ui/companion/companion_thumbs.c \
      gfx/gfx_anim_preview.c \
      $LC/memory/mem_stats.c \
      $LC/formats/tga/rtga.c \
      ui/companion/test/companion_core_stubs.c \
      ui/companion/test/companion_core_test.c \
      playlist.c \
      core_option_manager.c \
      $LC/lists/nested_list.c \
      $LC/formats/json/rjson.c \
      $LC/formats/image_texture.c \
      $LC/formats/image_transfer.c \
      $LC/formats/data_transfer.c \
      $LC/formats/png/rpng.c \
      $LC/formats/png/rpng_apng.c \
      $LC/formats/png/rpng_encode.c \
      $LC/file/rpng_file.c \
      $LC/memmap/memmap.c \
      $LC/streams/file_stream.c \
      $LC/streams/file_stream_transforms.c \
      $LC/streams/interface_stream.c \
      $LC/streams/memory_stream.c \
      $LC/streams/rzip_stream.c \
      $LC/streams/trans_stream.c \
      $LC/streams/trans_stream_pipe.c \
      $LC/streams/trans_stream_zlib.c \
      $LC/streams/trans_stream_deflate.c \
      $LC/vfs/vfs_implementation.c \
      $LC/file/file_path.c \
      $LC/file/file_path_io.c \
      $LC/file/config_file.c \
      $LC/file/config_file_io.c \
      $LC/file/archive_file.c \
      $LC/file/archive_file_zlib.c \
      $LC/file/retro_dirent.c \
      $LC/lists/dir_list.c \
      $LC/lists/string_list.c \
      $LC/lists/file_list.c \
      $LC/string/stdstring.c \
      $LC/string/rstrtod.c \
      $LC/compat/compat_strl.c \
      $LC/compat/compat_strldup.c \
      $LC/compat/compat_posix_string.c \
      $LC/compat/compat_strcasestr.c \
      $LC/compat/fopen_utf8.c \
      $LC/encodings/encoding_utf.c \
      $LC/encodings/encoding_crc32.c \
      $LC/encodings/encoding_deflate.c \
      $LC/hash/lrc_hash.c \
      $LC/time/rtime.c \
      $LC/features/features_cpu.c \
      $LC/rthreads/rthreads.c"

$CC -std=gnu99 -O1 -g $SAN -Wall -Wno-unused-parameter -Wno-unused-function \
   $INC $DEFS $SRCS -o "$OUT/companion_core_test" -lpthread -lm -lz

"$OUT/companion_core_test"
