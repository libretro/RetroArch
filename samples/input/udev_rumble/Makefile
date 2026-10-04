TARGET := udev_rumble_test

# The real udev joypad driver, given a pad whose descriptor is a pipe:
# rumble is written by the driver's own thread, so a pad that is slow
# to answer does not hold the frame. Built with threads and without.
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
all: $(TARGET) $(TARGET)_nothreads

$(TARGET): udev_rumble_test.c $(REPO_ROOT)/input/drivers_joypad/udev_joypad.c
	$(CC) $(CFLAGS) -DHAVE_THREADS $(LDFLAGS) -o $@ udev_rumble_test.c \
		$(COMMON) $(LRC)/rthreads/rthreads.c -ludev -lpthread

$(TARGET)_nothreads: udev_rumble_test.c $(REPO_ROOT)/input/drivers_joypad/udev_joypad.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ udev_rumble_test.c $(COMMON) -ludev

check: all
	./$(TARGET)
	./$(TARGET)_nothreads
else
all check:
	@echo "SKIP udev_rumble_test: the udev joypad driver is Linux only"
endif

clean:
	rm -f $(TARGET) $(TARGET)_nothreads

.PHONY: all check clean
