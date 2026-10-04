TARGET := window_driver_test

ROOT              := ../../..
LIBRETRO_COMM_DIR := $(ROOT)/libretro-common

# The choice functions are the frontend's own file; the drivers, the
# settings and input_driver_init_wrap() are stood in for by the test.
SOURCES := window_driver_test.c $(ROOT)/input/input_driver_choice.c

INCLUDES := -I$(ROOT) -I$(LIBRETRO_COMM_DIR)/include -I$(ROOT)/input/include
DEFINES  := -DRARCH_INTERNAL

CFLAGS += -Wall -std=gnu99 -g -O1 $(INCLUDES) $(DEFINES)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

MINGW_CC ?= x86_64-w64-mingw32-gcc

all: $(TARGET) $(TARGET)_sdl1

# host: the X11, SDL 3 and Wayland windows and the no-window-system choices
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -DHAVE_X11 -DHAVE_UDEV -DHAVE_SDL3 -DHAVE_WAYLAND -o $@ $(SOURCES) $(LDFLAGS)

# the same with SDL 1.2 in SDL 3's place: a build has one or the other,
# and the SDL 1.2 window's choice is only compiled with it
$(TARGET)_sdl1: $(SOURCES)
	$(CC) $(CFLAGS) -DHAVE_X11 -DHAVE_UDEV -DHAVE_SDL -DHAVE_WAYLAND -o $@ $(SOURCES) $(LDFLAGS)

# Windows, run under Wine: the Windows and SDL 3 window choices
$(TARGET).exe: $(SOURCES)
	$(MINGW_CC) -Wall -std=gnu99 -O1 $(INCLUDES) $(DEFINES) \
	   -D_WIN32_WINNT=0x0601 -DHAVE_WINRAWINPUT -DHAVE_DINPUT -DHAVE_SDL3 \
	   -o $@ $(SOURCES)

check: all
	./$(TARGET)
	./$(TARGET)_sdl1

# wine goes by several names; the first one found is used
WINE ?= $(shell for w in wine wine64 /usr/lib/wine/wine64; do \
           command -v $$w >/dev/null 2>&1 && { echo $$w; break; }; done)

check-windows: $(TARGET).exe
	@test -n "$(WINE)" || { echo "no wine" >&2; exit 1; }
	WINEDEBUG=-all $(WINE) ./$(TARGET).exe

clean:
	rm -f $(TARGET) $(TARGET)_sdl1 $(TARGET).exe

.PHONY: all check check-windows clean
