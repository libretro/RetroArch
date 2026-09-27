TARGET := dsound_pump_test.exe

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

# A Windows program: built with MinGW, run on Windows or under wine,
# which needs an audio device - on a headless box an ALSA null PCM
# (pcm.!default { type null }) plays at real-time pace.
MINGW_CC := $(shell command -v x86_64-w64-mingw32-gcc 2>/dev/null)
WINE     := $(shell command -v wine 2>/dev/null)

SOURCES := \
	dsound_pump_test.c \
	stubs.c \
	$(RARCH_DIR)/audio/drivers/dsound.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -std=gnu99 -Wall -g -O1 -DHAVE_THREADS -DHAVE_DSOUND -DRARCH_INTERNAL \
           -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include -I$(RARCH_DIR)/deps
LIBS    += -ldsound -ldxguid -lole32 -lwinmm -luser32

ifeq ($(MINGW_CC),)
all check:
	@echo "dsound_pump_test: no x86_64-w64-mingw32-gcc, skipped"
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
