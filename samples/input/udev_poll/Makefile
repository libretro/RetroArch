TARGET := udev_poll_test
JOYPAD := udev_joypad_poll_test

# The real udev input and joypad drivers, handed a hotplug monitor and
# devices on pipes: what one poll waits for and reads, and a hotplug
# that takes a device away. The drivers are included by the tests; the
# frontend symbols they link against are stood in for.
#    make && make check
REPO_ROOT := ../../..

CFLAGS += -Wall -Wno-unused -std=gnu99 -g1 -O1 \
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
all: $(TARGET) $(JOYPAD)

$(TARGET): udev_poll_test.c $(REPO_ROOT)/input/drivers/udev_input.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ udev_poll_test.c \
		$(REPO_ROOT)/libretro-common/compat/compat_strl.c -ludev

$(JOYPAD): udev_joypad_poll_test.c $(REPO_ROOT)/input/drivers_joypad/udev_joypad.c
	$(CC) $(CFLAGS) -DHAVE_MENU $(LDFLAGS) -o $@ udev_joypad_poll_test.c \
		$(REPO_ROOT)/libretro-common/compat/compat_strl.c \
		$(REPO_ROOT)/input/common/output_writer.c \
		$(REPO_ROOT)/libretro-common/rthreads/rthreads.c -ludev -lpthread

check: all
	./$(TARGET)
	./$(JOYPAD)
else
all check:
	@echo "SKIP udev_poll_test: the udev drivers are Linux only"
endif

clean:
	rm -f $(TARGET) $(JOYPAD)

.PHONY: all check clean
