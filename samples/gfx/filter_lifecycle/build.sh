#!/bin/sh
# Builds the softfilter lifecycle harness: shipping video_filter.c,
# the builtin filter set, and the harness main, threads on, ASan.
set -e
cd "$(dirname "$0")"
R=../../..
FILTERS="$(ls $R/gfx/video_filters/*.c)"
SRCS="$R/gfx/video_filter.c \
   $R/libretro-common/features/features_cpu.c \
   $R/libretro-common/file/config_file.c \
   $R/libretro-common/file/config_file_io.c \
   $R/libretro-common/file/config_file_userdata.c \
   $R/libretro-common/file/file_path.c \
   $R/libretro-common/file/file_path_io.c \
   $R/libretro-common/lists/string_list.c \
   $R/libretro-common/string/stdstring.c \
   $R/libretro-common/streams/file_stream.c \
   $R/libretro-common/vfs/vfs_implementation.c \
   $R/libretro-common/encodings/encoding_utf.c \
   $R/libretro-common/file/retro_dirent.c \
   $R/libretro-common/compat/compat_strl.c \
   $R/libretro-common/string/rstrtod.c \
   $R/libretro-common/compat/compat_strcasestr.c \
   $R/libretro-common/time/rtime.c \
   $R/libretro-common/rthreads/rthreads.c \
   $R/libretro-common/rthreads/retro_eventcount.c \
   $R/verbosity.c"
SRCS="$SRCS $FILTERS"
gcc -O1 -g -fsanitize=address -std=gnu99 \
   -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_FILTERS_BUILTIN \
   -D_GNU_SOURCE \
   -I$R -I$R/libretro-common/include -I$R/deps \
   filter_lifecycle_test.c frontend_stubs.c $SRCS \
   -o filter_lifecycle_test -lpthread -lm
echo "built samples/gfx/filter_lifecycle/filter_lifecycle_test"
