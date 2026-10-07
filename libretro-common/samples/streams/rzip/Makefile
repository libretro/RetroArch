TARGET       := rzip
TARGET_TEST  := rzip_chunk_size_test
TARGET_TEST2 := rzip_matches_buf_test
TARGET_TEST3 := rzip_codec_test
TARGET_TEST4 := rzip_parallel_test

LIBRETRO_COMM_DIR := ../../..
LIBRETRO_DEPS_DIR := ../../../../deps

# Attempt to detect target platform
ifeq '$(findstring ;,$(PATH))' ';'
	UNAME := Windows
else
	UNAME := $(shell uname 2>/dev/null || echo Unknown)
	UNAME := $(patsubst CYGWIN%,Cygwin,$(UNAME))
	UNAME := $(patsubst MSYS%,MSYS,$(UNAME))
	UNAME := $(patsubst MINGW%,MSYS,$(UNAME))
endif

# Add '.exe' extension on Windows platforms
ifeq ($(UNAME), Windows)
	TARGET := rzip.exe
endif
ifeq ($(UNAME), MSYS)
	TARGET := rzip.exe
endif

# rzip_stream.c parks its chunk-writer pool on retro_eventcount, so a
# HAVE_THREADS build links it alongside rthreads.
THREAD_SOURCES := \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c

COMMON_SOURCES := \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream_transforms.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/rzip_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/stdin_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_rzstd.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_rzstd.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(THREAD_SOURCES)


SOURCES      := rzip.c $(COMMON_SOURCES)
SOURCES_TEST := rzip_chunk_size_test.c $(COMMON_SOURCES)
SOURCES_TEST2 := rzip_matches_buf_test.c $(COMMON_SOURCES)
SOURCES_TEST3 := rzip_codec_test.c $(COMMON_SOURCES)
SOURCES_TEST4 := rzip_parallel_test.c $(COMMON_SOURCES)

OBJS      := $(SOURCES:.c=.o)
OBJS_TEST := $(SOURCES_TEST:.c=.o)
OBJS_TEST2 := $(SOURCES_TEST2:.c=.o)
OBJS_TEST3 := $(SOURCES_TEST3:.c=.o)
OBJS_TEST4 := $(SOURCES_TEST4:.c=.o)

INCLUDE_DIRS += -I$(LIBRETRO_COMM_DIR)/include
CFLAGS += -DHAVE_COMPRESSION -DHAVE_RZSTD -DHAVE_THREADS -Wall -pedantic -std=gnu99 $(INCLUDE_DIRS)
LDFLAGS += -lpthread

# Silence "ISO C does not support the 'I64' ms_printf length modifier"
# warnings when using MinGW
ifeq ($(UNAME), Windows)
	CFLAGS += -Wno-format
endif
ifeq ($(UNAME), MSYS)
	CFLAGS += -Wno-format
endif

ifneq ($(SANITIZER),)
	CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
	LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -g -DDEBUG -D_DEBUG
else
	CFLAGS += -O2 -DNDEBUG
endif

all: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET_TEST4)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST): $(OBJS_TEST)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST2): $(OBJS_TEST2)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST3): $(OBJS_TEST3)
	$(CC) -o $@ $^ $(LDFLAGS)

# The pool sizes itself from cpu_features_get_core_amount(); the test
# picks the width, so that one symbol is wrapped for this target only.
$(TARGET_TEST4): $(OBJS_TEST4)
	$(CC) -o $@ $^ $(LDFLAGS) -Wl,--wrap=cpu_features_get_core_amount

clean:
	rm -f $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET_TEST4) $(OBJS) $(OBJS_TEST) $(OBJS_TEST2) $(OBJS_TEST3) $(OBJS_TEST4)

.PHONY: clean
