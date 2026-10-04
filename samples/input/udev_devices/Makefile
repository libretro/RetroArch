TARGET := udev_devices_test

# The real udev input driver, handed a desk of devices in memory: its
# listing of keyboards and mice, its numbering, and what a port reads.
# udev_input.c is included by the test itself; the frontend symbols it
# links against are stood in for in the test.
#    make && make check
REPO_ROOT := ../../..

CFLAGS += -Wall -Wno-unused -std=gnu99 -g -O1 \
          -DRARCH_INTERNAL -DHAVE_UDEV -DHAVE_EPOLL -DHAVE_THREADS \
          -I$(REPO_ROOT) -I$(REPO_ROOT)/input/include \
          -I$(REPO_ROOT)/gfx/include \
          -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)/deps

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

# udev is Linux's: elsewhere there is no driver to test, and the
# targets say so and succeed.
ifeq ($(shell uname -s 2>/dev/null),Linux)
all: $(TARGET)

$(TARGET): udev_devices_test.c $(REPO_ROOT)/input/drivers/udev_input.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ udev_devices_test.c \
		$(REPO_ROOT)/libretro-common/compat/compat_strl.c -ludev

check: all
	./$(TARGET)
else
all check:
	@echo "SKIP udev_devices_test: the udev input driver is Linux only"
endif

clean:
	rm -f $(TARGET)

.PHONY: all check clean
