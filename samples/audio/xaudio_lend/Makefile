TARGET := xaudio_lend_test.exe

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

MINGW_CC := $(shell command -v x86_64-w64-mingw32-gcc 2>/dev/null)
WINE     := $(shell command -v wine 2>/dev/null)

# The shipping driver, against wine's XAudio2 (or Windows' own)
SOURCES := \
	xaudio_lend_test.c \
	stubs.c \
	$(RARCH_DIR)/audio/drivers/xaudio.c \
	$(RARCH_DIR)/audio/common/mmdevice_common.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -std=gnu99 -Wall -g1 -O1 -DHAVE_THREADS -DHAVE_XAUDIO -DRARCH_INTERNAL \
           -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include -I$(RARCH_DIR)/deps \
           -I$(RARCH_DIR)/gfx/include/dxsdk
# mingw-w64 ships the XAudio2 2.8 import library, as the cross build
# in Makefile.common links
LIBS    += -lxaudio2_8 -lole32 -lksuser -lwinmm -luser32 -luuid

ifeq ($(MINGW_CC),)
all check:
	@echo "xaudio_lend_test: no x86_64-w64-mingw32-gcc, skipped"
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
