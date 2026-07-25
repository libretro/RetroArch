TARGET := audio_transfer_window_test

LIBRETRO_COMM_DIR := ../../..

# The window test's whole point is that pages outside the window are
# not merely absent but unreadable, so strictness is on by default
# here; the test says so itself if it is ever built without it.
DEFINES := -DDT_STRICT -DHAVE_RWEBM -DHAVE_RVORBIS -DHAVE_ROPUS

COMMON_SOURCES := \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/formats/audio_transfer.c \
	$(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
	$(LIBRETRO_COMM_DIR)/formats/opus/ropus.c \
	$(LIBRETRO_COMM_DIR)/formats/vorbis/rvorbis.c \
	$(LIBRETRO_COMM_DIR)/formats/webm/rwebm.c \
	$(LIBRETRO_COMM_DIR)/memmap/memmap.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

COMMON_OBJS := $(COMMON_SOURCES:.c=.o)
OBJS        := audio_transfer_window_test.o $(COMMON_OBJS)

CFLAGS  += -Wall -pedantic -std=gnu99 -g -O0 -I$(LIBRETRO_COMM_DIR)/include \
	$(DEFINES)
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
