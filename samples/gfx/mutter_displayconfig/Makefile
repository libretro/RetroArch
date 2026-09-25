TARGET := mutter_displayconfig_test

# Links the real Mutter DisplayConfig client and the real X11 and
# Wayland display servers against libdbus, libX11/libXrandr and
# libwayland-client.  run.sh supplies the session bus, the mock Mutter
# and the X/Wayland servers each case needs.
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

# KWin's output protocols, which dispserv_wl.c reaches through
# gfx/common/wayland_kwin_output.c
KDE_XML  := $(REPO_ROOT)/deps/wayland-protocols/kde
KDE_HDRS := $(REPO_ROOT)/gfx/common/wayland/kde-output-device-v2.h \
            $(REPO_ROOT)/gfx/common/wayland/kde-output-management-v2.h
KDE_CODE := kde-output-device-v2-protocol.c kde-output-management-v2-protocol.c
# wlroots' output management, reached the same way
WLR_XML  := $(REPO_ROOT)/deps/wayland-protocols/wlr
WLR_HDRS := $(REPO_ROOT)/gfx/common/wayland/wlr-output-management-unstable-v1.h
WLR_CODE := wlr-output-management-unstable-v1-protocol.c
SOURCES := mutter_displayconfig_test.c \
           $(REPO_ROOT)/gfx/common/mutter_displayconfig.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_x11.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_wl.c \
           $(REPO_ROOT)/gfx/common/xinerama_common.c \
           $(REPO_ROOT)/gfx/common/dbus_runtime.c \
           $(LRC)/rthreads/rthreads.c \
           $(LRC)/dynamic/dylib.c \
           $(REPO_ROOT)/gfx/common/wayland_kwin_output.c \
           kde-output-device-v2-protocol.c \
           kde-output-management-v2-protocol.c \
           $(REPO_ROOT)/gfx/common/wayland_wlr_output.c \
           wlr-output-management-unstable-v1-protocol.c \
           $(LRC)/compat/compat_strl.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -D_GNU_SOURCE -DHAVE_THREADS -DHAVE_DYLIB -DHAVE_XRANDR -DHAVE_XINERAMA -DHAVE_WAYLAND \
           -DTEST_X11 -DTEST_WL \
           -I$(REPO_ROOT) -I$(LRC)/include \
           $(shell pkg-config --cflags dbus-1 wayland-client)

LDLIBS  += $(shell pkg-config --libs dbus-1 wayland-client) \
           -lXrandr -lXinerama -lX11 -lm -ldl -lpthread

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

$(REPO_ROOT)/gfx/common/wayland/kde-%.h: $(KDE_XML)/kde-%.xml
	wayland-scanner client-header $< $@
kde-%-protocol.c: $(KDE_XML)/kde-%.xml
	wayland-scanner private-code $< $@
$(REPO_ROOT)/gfx/common/wayland/wlr-%.h: $(WLR_XML)/wlr-%.xml
	wayland-scanner client-header $< $@
wlr-%-protocol.c: $(WLR_XML)/wlr-%.xml
	wayland-scanner private-code $< $@
$(OBJS): $(KDE_HDRS) $(WLR_HDRS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(TARGET) obj $(KDE_CODE) $(WLR_CODE)

.PHONY: clean
