# The SPIR-V cache: gfx/drivers_shader/slang_cache.c and the cache store
# in gfx/video_shader_parse.c, against a real directory through the
# real VFS. video_shader_parse.c is linked whole and only its cache
# store is called, so the frontend the rest of it names is left
# unresolved (GNU ld, non-PIE).
RA      := ../../..
SAN     ?= address,undefined
CFLAGS  := -std=gnu99 -g -O1 -fsanitize=$(SAN) -DRARCH_INTERNAL \
           -DHAVE_SLANG -DHAVE_GLSLANG \
           -I$(RA) -I$(RA)/libretro-common/include -I$(RA)/deps
SRC     := slang_cache_test.c \
           $(RA)/gfx/drivers_shader/slang_cache.c \
           $(RA)/gfx/video_shader_parse.c \
           $(RA)/libretro-common/streams/file_stream.c \
           $(RA)/libretro-common/vfs/vfs_implementation.c \
           $(RA)/libretro-common/file/file_path.c \
           $(RA)/libretro-common/file/file_path_io.c \
           $(RA)/libretro-common/compat/compat_strl.c \
           $(RA)/libretro-common/compat/fopen_utf8.c \
           $(RA)/libretro-common/string/stdstring.c \
           $(RA)/libretro-common/encodings/encoding_crc32.c \
           $(RA)/libretro-common/encodings/encoding_utf.c \
           $(RA)/libretro-common/time/rtime.c
LDFLAGS := -no-pie -Wl,--unresolved-symbols=ignore-all -lm

all: mapped read

mapped: $(SRC)
	$(CC) $(CFLAGS) -DHAVE_MMAP -no-pie -o $@ $(SRC) $(LDFLAGS)

read: $(SRC)
	$(CC) $(CFLAGS) -no-pie -o $@ $(SRC) $(LDFLAGS)

check: all
	ASAN_OPTIONS=detect_leaks=1 ./mapped
	ASAN_OPTIONS=detect_leaks=1 ./read

clean:
	rm -f mapped read

.PHONY: all check clean
