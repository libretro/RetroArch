# WASAPI exclusive-mode pacing: the driver's text, unmodified but for
# its three Windows include lines, against a fake device on a
# real-time clock, under the reporter's settings.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := wasapi_exclusive_pacing_test
GEN       := wasapi_sim.c
SOURCES_C := wasapi_exclusive_pacing_test.c fake_wasapi.c stubs_retroarch.c $(GEN) \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/queues/fifo_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c
OBJS      := $(SOURCES_C:.c=.o)

INCDIRS   := -I. -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -I$(CORE_DIR)/deps
CFLAGS   += -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_WASAPI -D_GNU_SOURCE

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
else
CFLAGS  += -g -O1
endif

all: $(TARGET)

# The driver with its Windows includes swapped for the fake's.
WASAPI_SRC ?= $(CORE_DIR)/audio/drivers/wasapi.c
$(GEN): $(WASAPI_SRC)
	sed -e 's|#include "../common/mmdevice_common.h"|#include "fake_wasapi.h"|' \
	    -e 's|#include "../common/mmdevice_common_inline.h"||' \
	    -e 's|#include "../common/wasapi.h"||' \
	    -e 's|#include "../audio_driver.h"|#include "$(CORE_DIR)/audio/audio_driver.h"|' \
	    -e 's|#include "../../verbosity.h"|#include "$(CORE_DIR)/verbosity.h"|' \
	    -e 's|#include "../../configuration.h"|#include "$(CORE_DIR)/configuration.h"|' \
	    $< > $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -lm -lpthread

%.o: %.c
	$(CC) $(CFLAGS) -std=gnu99 -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) $(GEN)

.PHONY: all clean
