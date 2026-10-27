include ../../sample_objdir.mk

TARGET := archive_zip_methods_test

LIBRETRO_COMM_DIR := ../../..

# Deflate64, LZMA, bzip2 and Zstandard zip members, mapped with MMAP=1 (the
# default) and read through the stream with MMAP=0. HAVE_7ZIP brings
# the LZMA decoder.
SOURCES := \
	archive_zip_methods_test.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_7z.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_bz2.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_zstd.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_archive.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma_stream.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma2.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_bcj2.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_filters.c \
	$(LIBRETRO_COMM_DIR)/formats/rar/rrar_ppmd7.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_rbzip2.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_rzstd.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_xxh64.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

override CPPFLAGS += -DHAVE_COMPRESSION -DHAVE_7ZIP -DHAVE_RBZIP2 -DHAVE_RZSTD -I$(LIBRETRO_COMM_DIR)/include
CFLAGS            += -Wall -pedantic -std=gnu99 -g1

# A bzip2 member read with MMAP=0 goes through a ring that wraps round
# within it
override CPPFLAGS += -DZIP_BZIP2_AHEAD=1500

MMAP ?= 1
ifeq ($(MMAP),1)
   override CPPFLAGS += -DHAVE_MMAP
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c Makefile archive_zip_methods_fixtures.h
	$(CC) -c -o $@ $< $(CPPFLAGS) $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c Makefile
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CPPFLAGS) $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean
