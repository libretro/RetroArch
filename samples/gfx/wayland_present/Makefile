TARGET := wayland_present_test

# gfx/common/wayland_present.c against a compositor built here on
# libwayland-server.  Both protocol halves are generated from the
# bundled XML.
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
PROTO_XML := $(REPO_ROOT)/deps/wayland-protocols/stable/presentation-time/presentation-time.xml
CLIENT_H  := $(REPO_ROOT)/gfx/common/wayland/presentation-time.h
WAYLAND_SCANNER ?= wayland-scanner

GENERATED := presentation-time-server-protocol.h \
             presentation-time-protocol.c

SOURCES := wayland_present_test.c \
           presentation-time-protocol.c \
           $(REPO_ROOT)/gfx/common/wayland_present.c \
           $(LRC)/features/features_cpu.c \
           $(LRC)/compat/compat_strl.c

CFLAGS  += -Wall -std=gnu89 -g -O1 \
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
presentation-time-server-protocol.h: $(PROTO_XML)
	$(WAYLAND_SCANNER) server-header $< $@
presentation-time-protocol.c: $(PROTO_XML)
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
