# input/common/wayland_cursor.h: the size and scale the Wayland context
# loads its cursor theme at when the compositor draws none.

TARGET    := wayland_cursor_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

SOURCES := wayland_cursor_test.c
CFLAGS  += -Wall -std=c89 -pedantic -g -O0 \
           -I$(REPO_ROOT) -I$(LRC)/include
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
