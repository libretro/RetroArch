# gfx/common/dbus_common.c's screensaver inhibit against a private
# session bus and a fake org.freedesktop.ScreenSaver. The library under
# test loads libdbus at run time; the fake service links it.
#
#   make check

TARGET   := dbus_screensaver_test
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common

DBUS_CFLAGS ?= $(shell pkg-config --cflags dbus-1 2>/dev/null)
DBUS_LIBS   ?= $(shell pkg-config --libs dbus-1 2>/dev/null || echo -ldbus-1)

SOURCES := dbus_screensaver_test.c \
           $(CORE_DIR)/gfx/common/dbus_common.c \
           $(CORE_DIR)/gfx/common/dbus_runtime.c \
           $(LRC)/dynamic/dylib.c \
           $(LRC)/rthreads/rthreads.c \
           $(LRC)/rthreads/retro_eventcount.c \
           $(LRC)/features/features_cpu.c \
           $(LRC)/compat/compat_strl.c \
           $(LRC)/string/stdstring.c \
           $(LRC)/encodings/encoding_utf.c
CFLAGS  += -Wall -std=gnu99 -g -O1 -D_GNU_SOURCE -DHAVE_THREADS -DHAVE_DYLIB \
           -DHAVE_DBUS -I$(CORE_DIR) -I$(LRC)/include $(DBUS_CFLAGS)
LDFLAGS += -lpthread -ldl $(DBUS_LIBS)
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
	./run.sh
clean:
	rm -f $(TARGET) $(OBJS)
.PHONY: all check clean
