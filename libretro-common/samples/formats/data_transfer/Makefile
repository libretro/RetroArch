TARGET       := data_transfer_source_test
TARGET_TEST2 := data_transfer_prefix_test
TARGET_TEST3 := data_transfer_window_test

LIBRETRO_COMM_DIR := ../../..

COMMON_SOURCES := \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
	$(LIBRETRO_COMM_DIR)/memmap/memmap.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

COMMON_OBJS := $(COMMON_SOURCES:.c=.o)
OBJS        := data_transfer_source_test.o data_transfer_prefix_test.o \
	data_transfer_window_test.o $(COMMON_OBJS)

CFLAGS += -Wall -pedantic -std=gnu99 -g -O0 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_TEST2) $(TARGET_TEST3)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): data_transfer_source_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST2): data_transfer_prefix_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST3): data_transfer_window_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET_TEST2) $(TARGET_TEST3) $(OBJS)

.PHONY: clean
