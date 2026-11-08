include ../../sample_objdir.mk

TARGET := audio_transfer_wma_test

LIBRETRO_COMM_DIR := ../../..


WMA ?= 1
ifeq ($(WMA),1)
DEFINES := -DHAVE_RWMA
WMA_SOURCES := $(LIBRETRO_COMM_DIR)/formats/wma/rwma_pro.c \
 $(LIBRETRO_COMM_DIR)/formats/wma/rwma_standard.c \
 $(LIBRETRO_COMM_DIR)/formats/wma/rwma_stream.c
endif

COMMON_SOURCES := \
	$(LIBRETRO_COMM_DIR)/audio/audio_mix.c \
	$(LIBRETRO_COMM_DIR)/audio/audio_mixer.c \
	$(LIBRETRO_COMM_DIR)/audio/conversion/float_to_s16.c \
	$(LIBRETRO_COMM_DIR)/audio/conversion/s16_to_float.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler.c \
 $(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler_int16.c \
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
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

COMMON_SOURCES += $(WMA_SOURCES)

COMMON_OBJS := $(call sample_objs,$(COMMON_SOURCES:.c=.o))
OBJS        := $(SAMPLE_OBJDIR)/audio_transfer_wma_test.o $(COMMON_OBJS)

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O0 -I$(LIBRETRO_COMM_DIR)/include \
	$(DEFINES)
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(SAMPLE_OBJDIR)/audio_transfer_wma_test.o: wma_fixture.h
$(call sample_objs,$(LIBRETRO_COMM_DIR)/formats/wma/rwma_stream.o): $(LIBRETRO_COMM_DIR)/formats/wma/rwma_xma_stream.h

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
