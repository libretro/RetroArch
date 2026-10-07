TARGET       := r7z_lzma_props_test
TARGET_TEST2 := r7z_archive_detach_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	r7z_lzma_props_test.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma.c

ARCHIVE_SOURCES := \
	r7z_archive_detach_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_archive.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_bcj2.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_filters.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma2.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma_stream.c

OBJS         := $(SOURCES:.c=.o)
ARCHIVE_OBJS := $(ARCHIVE_SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_TEST2)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET_TEST2): $(ARCHIVE_OBJS)
	$(CC) $(ARCHIVE_OBJS) $(LDFLAGS) -o $@

check: $(TARGET) $(TARGET_TEST2)
	./$(TARGET)
	./$(TARGET_TEST2)

clean:
	rm -f $(TARGET) $(TARGET_TEST2) $(OBJS) $(ARCHIVE_OBJS)

.PHONY: all check clean
