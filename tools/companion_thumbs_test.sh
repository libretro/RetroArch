#!/bin/sh
# Build and run the regression test for the desktop companions' shared
# thumbnail engine (ui/companion/companion_thumbs.c) against the real
# decoder, on Linux. Pass "tsan" or "asan" to run under that sanitizer.
#
#   tools/companion_thumbs_test.sh          # plain
#   tools/companion_thumbs_test.sh tsan     # ThreadSanitizer
#   tools/companion_thumbs_test.sh asan     # Address+UB sanitizer
#
# Also compiles the engine once as C89 (-ansi -pedantic, no threads) to
# keep it honest for the MSVC / 9x builds.
set -eu
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
SAN=""
case "${1:-}" in
   tsan) SAN="-fsanitize=thread" ;;
   asan) SAN="-fsanitize=address,undefined -fno-omit-frame-pointer" ;;
   "")   ;;
   *)    echo "usage: $0 [tsan|asan]" >&2; exit 2 ;;
esac

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

LC=libretro-common
INC="-I. -I$LC/include"
DEFS="-DRARCH_INTERNAL -DLIBRETRO_STRL_CHECK_OVERLAP -DHAVE_THREADS -DHAVE_RTGA -DHAVE_RPNG"
SRCS="ui/companion/companion_thumbs.c \
      gfx/gfx_anim_preview.c \
      $LC/memory/mem_stats.c \
      ui/companion/test/companion_thumbs_test.c \
      $LC/formats/image_texture.c \
      $LC/formats/image_transfer.c \
      $LC/formats/data_transfer.c \
      $LC/memmap/memmap.c \
      $LC/formats/tga/rtga.c \
      $LC/formats/png/rpng.c \
      $LC/formats/png/rpng_apng.c \
      $LC/streams/trans_stream.c \
      $LC/streams/trans_stream_pipe.c \
      $LC/streams/trans_stream_zlib.c \
      $LC/streams/trans_stream_deflate.c \
      $LC/encodings/encoding_deflate.c \
      $LC/encodings/encoding_crc32.c \
      $LC/streams/file_stream.c \
      $LC/streams/interface_stream.c \
      $LC/streams/memory_stream.c \
      $LC/streams/rzip_stream.c \
      $LC/vfs/vfs_implementation.c \
      $LC/file/file_path.c \
      $LC/file/file_path_io.c \
      $LC/string/stdstring.c \
      $LC/string/rstrtod.c \
      $LC/compat/compat_strl.c \
      $LC/compat/compat_strldup.c \
      $LC/compat/compat_posix_string.c \
      $LC/compat/fopen_utf8.c \
      $LC/encodings/encoding_utf.c \
      $LC/time/rtime.c \
      $LC/features/features_cpu.c \
      $LC/rthreads/rthreads.c"

# C89 honesty pass on the engine itself (no threads).
# (_GNU_SOURCE as in the real C89 gate: strict -ansi hides POSIX types
#  such as struct timespec that libretro-common's headers rely on.)
$CC -std=c89 -ansi -pedantic -Werror=pedantic -Wno-long-long -Wall -Wextra \
   -Wno-unused-parameter -Werror=declaration-after-statement -D_GNU_SOURCE \
   $INC -DRARCH_INTERNAL -DHAVE_RPNG -c ui/companion/companion_thumbs.c -o "$OUT/c89.o"

$CC -std=gnu99 -O1 -g $SAN -Wall -Wextra -Wno-unused-parameter \
   $INC $DEFS $SRCS -o "$OUT/companion_thumbs_test" -lpthread -lm -lz

"$OUT/companion_thumbs_test" "$OUT"
