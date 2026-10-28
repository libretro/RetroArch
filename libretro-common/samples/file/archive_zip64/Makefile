include ../../sample_objdir.mk

TARGET := archive_zip64_test

LIBRETRO_COMM_DIR := ../../..

# ZIP64 through the zip backend, mapped with MMAP=1 (the default) and
# read through the stream with MMAP=0. The archive past 4 GiB is a
# sparse file, removed afterwards; where the filesystem cannot hold it
# that part skips.
SOURCES := \
	archive_zip64_test.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
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
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

override CPPFLAGS += -DHAVE_COMPRESSION -I$(LIBRETRO_COMM_DIR)/include
CFLAGS            += -Wall -pedantic -std=gnu99 -g1

MMAP ?= 1
ifeq ($(MMAP),1)
   override CPPFLAGS += -DHAVE_MMAP
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c Makefile
	$(CC) -c -o $@ $< $(CPPFLAGS) $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c Makefile
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CPPFLAGS) $(CFLAGS)

# The wrap counts reads through the stream: mapped, the directory is
# read in place.
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -Wl,--wrap=filestream_read

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean
