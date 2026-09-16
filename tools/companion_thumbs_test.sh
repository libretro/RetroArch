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
DEFS="-DRARCH_INTERNAL -DLIBRETRO_STRL_CHECK_OVERLAP -DHAVE_THREADS -DHAVE_RTGA -DHAVE_RPNG -DHAVE_RMP4 -DHAVE_RH264"
SRCS="ui/companion/companion_thumbs.c \
      gfx/gfx_anim_preview.c \
      $LC/formats/mp4/rmp4.c \
      $LC/formats/mp4/rmp4_video.c \
      $LC/formats/h264/rh264.c \
      $LC/formats/h265/rh265.c \
      $LC/formats/vp8/rvp8.c \
      $LC/formats/image/image_hdr_blit.c \
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

# The video-hover test needs an MP4 whose frames are distinct solid
# colours (see test_video_hover): 8 frames, Constrained Baseline, 2 KB.
# ffmpeg is a hard requirement - a missing fixture is a test failure,
# not a skip.
command -v ffmpeg >/dev/null 2>&1 || { echo "ffmpeg is required (video fixture)" >&2; exit 2; }
ffmpeg -v error -y -f lavfi \
   -i "color=c=black:s=64x64:r=10,geq=r='255*mod(N\,2)':g='255*mod(floor(N/2)\,2)':b='128+64*mod(floor(N/4)\,2)'" \
   -frames:v 8 -c:v libx264 -preset ultrafast -profile:v baseline -level 3.0 \
   -pix_fmt yuv420p -g 4 -bf 0 "$OUT/hover.mp4"

"$OUT/companion_thumbs_test" "$OUT"
