TARGET := mutter_displayconfig_test

# Links the real Mutter DisplayConfig client and the real X11 and
# Wayland display servers against libdbus, libX11/libXrandr and
# libwayland-client.  run.sh supplies the session bus, the mock Mutter
# and the X/Wayland servers each case needs.
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

SOURCES := mutter_displayconfig_test.c \
           $(REPO_ROOT)/gfx/common/mutter_displayconfig.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_x11.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_wl.c \
           $(REPO_ROOT)/gfx/common/xinerama_common.c \
           $(LRC)/compat/compat_strl.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -DHAVE_DBUS -DHAVE_XRANDR -DHAVE_XINERAMA -DHAVE_WAYLAND \
           -DTEST_X11 -DTEST_WL \
           -I$(REPO_ROOT) -I$(LRC)/include \
           $(shell pkg-config --cflags dbus-1 wayland-client)

LDLIBS  += $(shell pkg-config --libs dbus-1 wayland-client) \
           -lXrandr -lXinerama -lX11 -lm

# Objects under obj/, never next to the sources: other samples build
# some of the same files with other flags.
OBJS  := $(addprefix obj/,$(notdir $(SOURCES:.c=.o)))
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
