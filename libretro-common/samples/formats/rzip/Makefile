include ../../sample_objdir.mk

TARGET := rzip_archive_test
# a deflated member read at any offset through an index (rzip_seek)
SEEK   := rzip_seek_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/formats/zip/rzip_archive.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS += -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(SEEK)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): rzip_archive_test.o $(OBJS)
	$(CC) rzip_archive_test.o $(OBJS) $(LDFLAGS) -o $@ -lm

$(SEEK): rzip_seek_test.o $(OBJS)
	$(CC) rzip_seek_test.o $(OBJS) $(LDFLAGS) -o $@ -lm

check: $(TARGET) $(SEEK)
	./$(TARGET)
	./$(SEEK)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(SEEK) rzip_archive_test.o rzip_seek_test.o $(OBJS)

.PHONY: all check clean
