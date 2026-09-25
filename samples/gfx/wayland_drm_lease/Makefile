TARGET := wayland_drm_lease_test

# Path back to the repo root from this sample dir.  Like
# samples/gfx/display_servers this links the real display server
# rather than mirroring its shape; the compositor is what the test
# supplies, on libwayland-server.
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
PROTO_XML := $(REPO_ROOT)/deps/wayland-protocols/staging/drm-lease/drm-lease-v1.xml

WAYLAND_SCANNER ?= wayland-scanner

# The client side is what a build generates into gfx/common/wayland;
# the server side exists only here, for the test compositor.  Both are
# generated from the same bundled XML, so a protocol change cannot
# leave the two halves disagreeing.
GENERATED := drm-lease-v1-client-protocol.h \
             drm-lease-v1-server-protocol.h \
             drm-lease-v1-protocol.c

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
SOURCES := wayland_drm_lease_test.c \
           drm-lease-v1-protocol.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_wl.c \
           $(REPO_ROOT)/gfx/common/wayland_drm_lease.c \
           $(REPO_ROOT)/gfx/common/wayland_kwin_output.c \
           kde-output-device-v2-protocol.c \
           kde-output-management-v2-protocol.c \
           $(REPO_ROOT)/gfx/common/wayland_wlr_output.c \
           wlr-output-management-unstable-v1-protocol.c \
           $(LRC)/rthreads/rthreads.c \
           $(LRC)/compat/compat_strl.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -DRARCH_INTERNAL -DHAVE_WAYLAND -DHAVE_THREADS -D_GNU_SOURCE \
           -I. \
           -I$(REPO_ROOT) \
           -I$(LRC)/include \
           $(shell pkg-config --cflags wayland-client wayland-server)

LDFLAGS += $(shell pkg-config --libs wayland-client wayland-server) -lpthread

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

# dispserv_wl.c includes the client header by the path a real build
# puts it at, so the generated copy goes there too. clean leaves it:
# the script that generates it runs at configure time, so removing it
# would break an already configured tree until the next configure.
$(REPO_ROOT)/gfx/common/wayland/drm-lease-v1.h: $(PROTO_XML)
	$(WAYLAND_SCANNER) client-header $< $@

drm-lease-v1-client-protocol.h: $(PROTO_XML)
	$(WAYLAND_SCANNER) client-header $< $@

drm-lease-v1-server-protocol.h: $(PROTO_XML)
	$(WAYLAND_SCANNER) server-header $< $@

drm-lease-v1-protocol.c: $(PROTO_XML)
	$(WAYLAND_SCANNER) private-code $< $@

$(OBJS): $(GENERATED) $(REPO_ROOT)/gfx/common/wayland/drm-lease-v1.h

%.o: %.c
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
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) $(GENERATED) $(KDE_CODE) $(WLR_CODE)

.PHONY: all check clean
