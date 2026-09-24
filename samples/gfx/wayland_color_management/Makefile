# gfx/common/wayland_color.c against an in-process compositor that
# implements wp_color_manager_v1: what the client asks for, in what
# order, and that it never breaks the protocol's fatal rules.

TARGET := wayland_color_management_test

REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
PROTO_XML := $(REPO_ROOT)/deps/wayland-protocols/staging/color-management/color-management-v1.xml
CLIENT_H  := $(REPO_ROOT)/gfx/common/wayland/color-management-v1.h
WAYLAND_SCANNER ?= wayland-scanner

GENERATED := color-management-v1-server-protocol.h \
             color-management-v1-protocol.c

SOURCES := wayland_color_management_test.c \
           color-management-v1-protocol.c \
           $(REPO_ROOT)/gfx/common/wayland_color.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -DRARCH_INTERNAL -DHAVE_WAYLAND -D_GNU_SOURCE \
           -I. -I$(REPO_ROOT) -I$(LRC)/include \
           $(shell pkg-config --cflags wayland-client wayland-server)
LDFLAGS += $(shell pkg-config --libs wayland-client wayland-server) -lpthread

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(CLIENT_H): $(PROTO_XML)
	$(WAYLAND_SCANNER) client-header $< $@
color-management-v1-server-protocol.h: $(PROTO_XML)
	$(WAYLAND_SCANNER) server-header $< $@
color-management-v1-protocol.c: $(PROTO_XML)
	$(WAYLAND_SCANNER) private-code $< $@

$(OBJS): $(GENERATED) $(CLIENT_H)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) $(GENERATED)

.PHONY: all check clean
