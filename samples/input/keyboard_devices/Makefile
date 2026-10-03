TARGET := keyboard_devices_test

# Which of the devices a system reports are the keyboards on the desk:
# the grouping the udev input driver uses, on its own.
#    make && make check
REPO_ROOT := ../../..

CFLAGS += -Wall -std=gnu89 -Wdeclaration-after-statement -g -O1 \
          -I$(REPO_ROOT)/libretro-common/include

all: $(TARGET)

$(TARGET): keyboard_devices_test.c $(REPO_ROOT)/input/common/input_keyboard_devices.h
	$(CC) $(CFLAGS) -o $@ keyboard_devices_test.c

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
