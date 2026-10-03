# The WASAPI microphone's capture fetch: the driver's text, unmodified
# but for its Windows include lines, against the shared fake WASAPI
# (samples/audio/wasapi_exclusive_pacing) with a capture client added.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common
FAKE_DIR          := ../wasapi_exclusive_pacing

TARGET    := wasapi_mic_test
GEN       := wasapi_mic_sim.c
SOURCES_C := wasapi_mic_test.c $(FAKE_DIR)/fake_wasapi.c $(FAKE_DIR)/stubs_retroarch.c \
	../../../audio/audio_upmix.c \
	../../../audio/audio_binaural.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_frame.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_decode.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_encode.c \
	$(LIBRETRO_COMM_DIR)/formats/iec61937/iec61937.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/queues/fifo_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c
OBJS      := $(SOURCES_C:.c=.o)

INCDIRS   := -I. -I$(FAKE_DIR) -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -I$(CORE_DIR)/deps
CFLAGS   += -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_WASAPI -DHAVE_MICROPHONE -D_GNU_SOURCE

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
else
CFLAGS  += -g -O1
endif

all: $(TARGET)

# The driver with its Windows includes swapped for the fake's, and the
# capture client's types from fake_wasapi_mic.h.
WASAPI_SRC ?= $(CORE_DIR)/audio/drivers/wasapi.c
$(GEN): $(WASAPI_SRC)
	sed -e 's|#include "../common/mmdevice_common.h"|#include "fake_wasapi_mic.h"|' \
	    -e 's|#include "../common/mmdevice_common_inline.h"||' \
	    -e 's|#include "../common/wasapi.h"||' \
	    -e 's|#include "../audio_driver.h"|#include "$(CORE_DIR)/audio/audio_driver.h"|' \
	    -e 's|#include "../microphone_driver.h"|#include "$(CORE_DIR)/audio/microphone_driver.h"|' \
	    -e 's|#include "../../verbosity.h"|#include "$(CORE_DIR)/verbosity.h"|' \
	    -e 's|#include "../../configuration.h"|#include "$(CORE_DIR)/configuration.h"|' \
	    $< > $@

wasapi_mic_test.o: wasapi_mic_test.c $(GEN)
	$(CC) $(CFLAGS) -std=gnu99 -Wno-unused-function -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -lm -lpthread

%.o: %.c
	$(CC) $(CFLAGS) -std=gnu99 -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(GEN)

.PHONY: all check clean
