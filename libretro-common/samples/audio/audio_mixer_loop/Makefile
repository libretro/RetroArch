TARGET := audio_mixer_loop_test

LIBRETRO_COMM_DIR := ../../..

# Vorbis is the codec the reported regression was in, and the WAV arm
# gives the same test a second decoder to hold the seek contract
# against, so both are on.  Nothing else is needed: the point is the
# repeat path, not format coverage.
DEFINES := -DHAVE_RVORBIS -DHAVE_RWAV

COMMON_SOURCES := \
	$(LIBRETRO_COMM_DIR)/audio/audio_mix.c \
	$(LIBRETRO_COMM_DIR)/audio/audio_mixer.c \
	$(LIBRETRO_COMM_DIR)/audio/conversion/float_to_s16.c \
	$(LIBRETRO_COMM_DIR)/audio/conversion/s16_to_float.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/nearest_resampler.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/nearest_resampler_int16.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler_int16.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/file/config_file.c \
	$(LIBRETRO_COMM_DIR)/file/config_file_io.c \
	$(LIBRETRO_COMM_DIR)/file/config_file_userdata.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/formats/audio_transfer.c \
	$(LIBRETRO_COMM_DIR)/formats/vorbis/rvorbis.c \
	$(LIBRETRO_COMM_DIR)/formats/wav/rwav.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

COMMON_OBJS := $(COMMON_SOURCES:.c=.o)
OBJS        := audio_mixer_loop_test.o $(COMMON_OBJS)

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
