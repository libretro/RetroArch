# gfx/common/wayland_kwin_output.c against an in-process compositor that
# speaks KWin's output protocols: the modes it lists, the switch it asks
# for, and both forms KWin has published outputs in.

TARGET    := wayland_kwin_output_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
KDE_XML   := $(REPO_ROOT)/deps/wayland-protocols/kde
KDE_HDRS  := $(REPO_ROOT)/gfx/common/wayland/kde-output-device-v2.h \
             $(REPO_ROOT)/gfx/common/wayland/kde-output-management-v2.h
GENERATED := kde-output-device-v2-server-protocol.h \
             kde-output-management-v2-server-protocol.h \
             kde-output-device-v2-protocol.c kde-output-management-v2-protocol.c

SOURCES := wayland_kwin_output_test.c \
           kde-output-device-v2-protocol.c kde-output-management-v2-protocol.c \
           $(REPO_ROOT)/gfx/common/wayland_kwin_output.c \
           $(LRC)/compat/compat_strl.c
CFLAGS  += -Wall -std=gnu99 -g -O0 -DRARCH_INTERNAL -DHAVE_WAYLAND -D_GNU_SOURCE \
           -I. -I$(REPO_ROOT) -I$(LRC)/include \
           $(shell pkg-config --cflags wayland-client wayland-server)
LDFLAGS += $(shell pkg-config --libs wayland-client wayland-server) -lpthread
OBJS    := $(SOURCES:.c=.o)
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(REPO_ROOT)/gfx/common/wayland/kde-%.h: $(KDE_XML)/kde-%.xml
	wayland-scanner client-header $< $@
kde-%-server-protocol.h: $(KDE_XML)/kde-%.xml
	wayland-scanner server-header $< $@
kde-%-protocol.c: $(KDE_XML)/kde-%.xml
	wayland-scanner private-code $< $@
$(OBJS): $(GENERATED) $(KDE_HDRS)
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(OBJS) $(GENERATED)
.PHONY: all check clean
