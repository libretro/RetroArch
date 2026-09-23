# The WDM-KS WaveRT loop: silence behind the cursor, and a write that
# lands ahead of it again after the hardware has run past it.
#
# audio/drivers/wdmks.c compiled whole, with no device: the test holds
# the driver's state itself, points the position register at a word
# of its own and moves the "hardware" by hand. Built with MinGW like
# the driver, run under wine or on Windows; nothing here reaches the
# kernel, so wine needs no sound card.

TARGET := wdmks_loop_test.exe

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

MINGW_CC := $(shell command -v x86_64-w64-mingw32-gcc 2>/dev/null)
WINE     := $(shell command -v wine 2>/dev/null)

SOURCES := \
	wdmks_loop_test.c \
	$(RARCH_DIR)/audio/audio_upmix.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_frame.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_decode.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_encode.c \
	$(LIBRETRO_COMM_DIR)/formats/iec61937/iec61937.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

# -Wno-unused-function: the whole driver is included and the test
# calls only its loop bookkeeping.
CFLAGS  += -std=gnu99 -Wall -Wno-unused-function -Wno-unused-const-variable \
           -g -O1 -DHAVE_THREADS -DRARCH_INTERNAL \
           -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include -I$(RARCH_DIR)/deps
LIBS    += -lole32 -lwinmm -lsetupapi -lm

ifeq ($(MINGW_CC),)
all check:
	@echo "wdmks_loop_test: no x86_64-w64-mingw32-gcc, skipped"
else
all: $(TARGET)

$(TARGET): $(SOURCES)
	$(MINGW_CC) $(CFLAGS) -o $@ $(SOURCES) $(LIBS)

check: all
	$(WINE) ./$(TARGET)
endif

clean:
	rm -f $(TARGET)

.PHONY: all check clean
