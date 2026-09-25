# gfx/common/wayland_wlr_output.c against an in-process compositor that
# speaks wlroots output management: the modes it lists, the switch it asks
# for, and every head configured exactly once.

TARGET    := wayland_wlr_output_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
WLR_XML   := $(REPO_ROOT)/deps/wayland-protocols/wlr
WLR_HDRS  := $(REPO_ROOT)/gfx/common/wayland/wlr-output-management-unstable-v1.h
GENERATED := wlr-output-management-unstable-v1-server-protocol.h \
             wlr-output-management-unstable-v1-protocol.c

SOURCES := wayland_wlr_output_test.c \
           wlr-output-management-unstable-v1-protocol.c \
           $(REPO_ROOT)/gfx/common/wayland_wlr_output.c \
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
$(REPO_ROOT)/gfx/common/wayland/wlr-%.h: $(WLR_XML)/wlr-%.xml
	wayland-scanner client-header $< $@
wlr-%-server-protocol.h: $(WLR_XML)/wlr-%.xml
	wayland-scanner server-header $< $@
wlr-%-protocol.c: $(WLR_XML)/wlr-%.xml
	wayland-scanner private-code $< $@
$(OBJS): $(GENERATED) $(WLR_HDRS)
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(OBJS) $(GENERATED)
.PHONY: all check clean
