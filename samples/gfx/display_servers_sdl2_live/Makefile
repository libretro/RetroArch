TARGET := display_servers_sdl2_live_test

# Links the real SDL2 display server against SDL2 and switches a real
# SDL window among the display modes SDL lists on $DISPLAY.  Meant for
# the same Xorg 'dummy' driver server the X11 live sample uses: SDL's
# x11 backend lists that server's RandR modes and changes them with
# XRRSetCrtcConfig, so the switch is a real one.
REPO_ROOT := ../../..

SDL_CFLAGS ?= $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2)
SDL_LIBS   ?= $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2)

SOURCES := display_servers_sdl2_live_test.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_sdl2.c \
           $(REPO_ROOT)/libretro-common/compat/compat_strl.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -DHAVE_SDL2 \
           $(SDL_CFLAGS) \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include

LDLIBS  += $(SDL_LIBS) -lm

# Objects live under obj/ here, never next to the sources: the other
# display server samples compile some of the same files with other
# flags, and a stale object in the tree would be picked up.
OBJS := $(addprefix obj/,$(notdir $(SOURCES:.c=.o)))
VPATH := $(sort $(dir $(SOURCES)))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

obj/%.o: %.c
	@mkdir -p obj
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(TARGET) obj

.PHONY: clean
