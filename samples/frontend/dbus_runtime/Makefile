# gfx/common/dbus_runtime.h mirrors libdbus's public ABI under names of
# its own, so RetroArch can load libdbus at run time without its headers.
# This checks the mirror against the real headers: every struct the
# caller allocates matches in size, alignment and field offsets, and
# every constant matches. It needs the D-Bus development headers; give
# DBUS_CFLAGS when pkg-config is not there to find them.
#
#   make check
#   make check DBUS_CFLAGS="-I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include"

TARGET      := dbus_runtime_abi_test
CORE_DIR    := ../../..
DBUS_CFLAGS ?= $(shell pkg-config --cflags dbus-1 2>/dev/null)

# gnu99: libdbus's own headers use static inline.
CFLAGS += -Wall -Wextra -std=gnu99 -pedantic -DHAVE_DYLIB \
          -I$(CORE_DIR) -I$(CORE_DIR)/libretro-common/include $(DBUS_CFLAGS)

all: $(TARGET)

$(TARGET): dbus_runtime_abi_test.c $(CORE_DIR)/gfx/common/dbus_runtime.h
	$(CC) $(CFLAGS) -o $@ dbus_runtime_abi_test.c

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
