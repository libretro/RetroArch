TARGET := image_loader_test

# The still loader (formats/image_texture.c) and the scale unit
# (formats/image_texture_scale.c), driven as a frontend task or a core
# would: whole buffer, timed steps against a fake clock, a prefix that
# grows, an abort, the APNG verdict, the request's answers.
LIBRETRO_COMM_DIR := ../../..

SOURCES := image_loader_test.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/image_texture.c \
           $(LIBRETRO_COMM_DIR)/formats/image_texture_scale.c \
           $(LIBRETRO_COMM_DIR)/formats/image_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/png/rpng.c \
           $(LIBRETRO_COMM_DIR)/formats/png/rpng_apng.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/pixconv.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/scaler.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_filter.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_int.c \
           $(LIBRETRO_COMM_DIR)/memmap/memmap.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -O0 -DHAVE_RPNG \
          -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -lm

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all clean
