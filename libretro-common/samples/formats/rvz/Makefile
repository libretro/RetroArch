include ../../sample_objdir.mk

TARGET        := rvz_test
.DEFAULT_GOAL := all

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rvz_test.c \
	$(LIBRETRO_COMM_DIR)/formats/rvz/rvz.c \
	$(LIBRETRO_COMM_DIR)/crypto/crypto.c \
	$(LIBRETRO_COMM_DIR)/hash/lrc_hash.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_rzstd.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_xxh64.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_rbzip2.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma_stream.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma2.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include -DHAVE_RZSTD

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean
