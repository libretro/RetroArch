TARGET := udev_ds3_pressure_test

# The real udev joypad driver reading a DualShock 3's button pressures
# from its hidraw node, which is stood in for by a socket that keeps
# each report whole, as hidraw does.
#    make && make check
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

CFLAGS += -Wall -Wno-unused -std=gnu99 -g -O1 \
          -DRARCH_INTERNAL -DHAVE_UDEV -DHAVE_MENU \
          -I$(REPO_ROOT) -I$(REPO_ROOT)/input/include \
          -I$(REPO_ROOT)/gfx/include \
          -I$(LRC)/include -I$(REPO_ROOT)/deps

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

COMMON := $(LRC)/compat/compat_strl.c $(REPO_ROOT)/input/common/output_writer.c

# udev is Linux's: elsewhere there is no driver to test, and the
# targets say so and succeed.
ifeq ($(shell uname -s 2>/dev/null),Linux)
all: $(TARGET)

$(TARGET): udev_ds3_pressure_test.c $(REPO_ROOT)/input/drivers_joypad/udev_joypad.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ udev_ds3_pressure_test.c $(COMMON) -ludev

check: all
	./$(TARGET)
else
all check:
	@echo "SKIP udev_ds3_pressure_test: the udev joypad driver is Linux only"
endif

clean:
	rm -f $(TARGET)

.PHONY: all check clean
