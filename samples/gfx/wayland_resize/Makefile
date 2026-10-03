# gfx/common/wayland_resize.h: what the Wayland context drivers do once
# their buffers are resized, against an in-process compositor.

TARGET    := wayland_resize_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

SOURCES := wayland_resize_test.c
CFLAGS  += -Wall -std=gnu89 -g -O0 \
           -I$(REPO_ROOT) -I$(LRC)/include \
           $(shell pkg-config --cflags wayland-client wayland-server)
LDFLAGS += $(shell pkg-config --libs wayland-client wayland-server) -lpthread
OBJS    := $(SOURCES:.c=.o)
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(OBJS)
.PHONY: all check clean
