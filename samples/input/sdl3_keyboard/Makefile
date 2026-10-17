# input/drivers/sdl3_input.c's key events through its own poll. SDL3
# is not packaged everywhere yet: the test is built and run where
# pkg-config knows it, or where SDL3_CFLAGS and SDL3_LIBS say where a
# build of it is, and skipped otherwise.
TARGET    := sdl3_keyboard_test
REPO_ROOT := ../../..

SDL3_CFLAGS ?= $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL3_LIBS   ?= $(shell pkg-config --libs sdl3 2>/dev/null)

CFLAGS += -Wall -Wno-unused-function -std=gnu99 -g -O1 -DRARCH_INTERNAL -DHAVE_SDL3 \
   -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)/deps \
   -I$(REPO_ROOT)/gfx/include $(SDL3_CFLAGS)

SOURCES := sdl3_keyboard_test.c \
           $(REPO_ROOT)/input/input_keymaps.c \
           $(REPO_ROOT)/libretro-common/compat/compat_strl.c

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

ifneq ($(strip $(SDL3_LIBS)),)
all: $(TARGET)

$(TARGET): $(SOURCES) $(REPO_ROOT)/input/drivers/sdl3_input.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) $(SDL3_LIBS)

check: all
	SDL_VIDEO_DRIVER=dummy ./$(TARGET)
else
all:
	@echo "[skip] $(TARGET): no SDL3 here (pkg-config sdl3, or SDL3_CFLAGS and SDL3_LIBS)"

check: all
endif

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
