TARGET := rpng
TARGET_TEST := rpng_chunk_overflow_test
TARGET_TEST2 := rpng_roundtrip_test
TARGET_TEST3 := rpng_decode_corpus_test

CORE_DIR          := .
LIBRETRO_PNG_DIR  := ../../../formats/png
LIBRETRO_COMM_DIR := ../../..

HAVE_IMLIB2=0


ifeq ($(HAVE_IMLIB2),1)
CFLAGS += -DHAVE_IMLIB2
LDFLAGS += -lImlib2
endif

SOURCES_C := 	\
	$(CORE_DIR)/rpng_test.c \
	$(LIBRETRO_PNG_DIR)/rpng.c \
	$(LIBRETRO_PNG_DIR)/rpng_encode.c \
	$(LIBRETRO_COMM_DIR)/file/rpng_file.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_intf.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_stdio.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_linux.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_unixmmap.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_windowsmmap.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/rzip_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c

# The chunk-overflow regression test exercises only
# rpng_iterate_image, which doesn't invoke the inflate stream,
# but rpng.c itself pulls in trans_stream.h so the
# trans_stream/zlib objects must still be linked in.
TEST_SOURCES_C := \
	$(CORE_DIR)/rpng_chunk_overflow_test.c \
	$(LIBRETRO_PNG_DIR)/rpng.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \

# The round-trip regression test writes with rpng_encode and reads
# back with rpng, so it needs the full file/nbio/stream stack.
# Sources intentionally mirror $(SOURCES_C) minus the demo's
# rpng_test.c entry point, plus the new test entry point.
TEST2_SOURCES_C := \
	$(CORE_DIR)/rpng_roundtrip_test.c \
	$(LIBRETRO_PNG_DIR)/rpng.c \
	$(LIBRETRO_PNG_DIR)/rpng_encode.c \
	$(LIBRETRO_COMM_DIR)/file/rpng_file.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_intf.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_stdio.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_linux.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_unixmmap.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_windowsmmap.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/rzip_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c

# The decode corpus test builds its own PNG streams in memory - with the
# filter type forced per scanline, across every colour type, bit depth
# and a width sweep - and decodes them with rpng, so it needs the
# decoder, the CRC used to seal the chunks it writes, and the deflate
# backend it compresses them with.  No file I/O and no second decoder as
# an oracle: the expected pixels are a function of position, so the
# generator knows them.
TEST3_SOURCES_C := \
	$(CORE_DIR)/rpng_decode_corpus_test.c \
	$(LIBRETRO_PNG_DIR)/rpng.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c

OBJS := $(SOURCES_C:.c=.o)
TEST_OBJS := $(TEST_SOURCES_C:.c=.o)
TEST2_OBJS := $(TEST2_SOURCES_C:.c=.o)
TEST3_OBJS := $(TEST3_SOURCES_C:.c=.corpus.o)

CFLAGS += -Wall -pedantic -std=gnu99 -O0 -g -I$(LIBRETRO_COMM_DIR)/include

# The corpus test decodes ~2000 generated images.  RPNG_TEST turns on
# rpng.c's per-image IHDR trace, which would bury the CI log (and any
# sanitizer report in it), so that target gets its own objects built
# without it.  Everything else keeps the trace it has always had.
CORPUS_CFLAGS := $(CFLAGS)
CFLAGS        += -DRPNG_TEST

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%.corpus.o: %.c
	$(CC) -c -o $@ $< $(CORPUS_CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST): $(TEST_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST2): $(TEST2_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST3): $(TEST3_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3) \
		$(OBJS) $(TEST_OBJS) $(TEST2_OBJS) $(TEST3_OBJS)

.PHONY: clean
