compiler     := gcc
TARGET       := discrete_multichannel_test

CORE_DIR = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) \
	-I$(CORE_DIR)/deps -I$(CORE_DIR)/deps/stb

CC      := $(compiler)
flags   += -std=gnu99

# The WASAPI driver's text, its three Windows include lines swapped
# for the fake device's, so the AC-3 bitstream path runs here.
GEN := wasapi_sim.c

SOURCES_C := \
	discrete_multichannel_test.c \
	$(GEN) \
	../wasapi_exclusive_pacing/fake_wasapi.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_frame.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_decode.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_encode.c \
	$(LIBRETRO_COMM_DIR)/formats/iec61937/iec61937.c \
	$(LIBRETRO_COMM_DIR)/queues/fifo_queue.c \
	stubs_retroarch.c \
	../../../audio/audio_upmix.c \
	../../../audio/audio_binaural.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/lists/dir_list.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/audio/conversion/float_to_s16.c \
	$(LIBRETRO_COMM_DIR)/audio/conversion/s16_to_float.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler_int16.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/nearest_resampler_int16.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/file/config_file.c \
	$(LIBRETRO_COMM_DIR)/file/config_file_userdata.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/nearest_resampler.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS += -Wall $(INCDIRS)
CFLAGS += -I../wasapi_exclusive_pacing -DHAVE_WASAPI -DHAVE_RAC3 -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_AUDIOMIXER -DHAVE_MENU \
	-DHAVE_CONFIGFILE -D_GNU_SOURCE
LDFLAGS += -lm -lpthread

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

WASAPI_SRC ?= ../../../audio/drivers/wasapi.c
$(GEN): $(WASAPI_SRC)
	sed -e 's|#include "../common/mmdevice_common.h"|#include "fake_wasapi.h"|' \
	    -e 's|#include "../common/mmdevice_common_inline.h"||' \
	    -e 's|#include "../common/wasapi.h"||' \
	    -e 's|#include "../audio_driver.h"|#include "../../../audio/audio_driver.h"|' \
	    -e 's|#include "../../verbosity.h"|#include "../../../verbosity.h"|' \
	    -e 's|#include "../../configuration.h"|#include "../../../configuration.h"|' \
	    $< > $@


%.o: %.c
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -c -o$@ $<

$(TARGET): $(OBJS)
	$(CC) -o$@ $(OBJS) $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean all check
