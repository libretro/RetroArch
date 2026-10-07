TARGET := device_pins_test

# A port keeps its keyboard by what the keyboard is, not by its place
# in a list: the rule, on its own.
#    make && make check
REPO_ROOT := ../../..

CFLAGS += -Wall -std=gnu89 -Wdeclaration-after-statement -g -O1 \
          -I$(REPO_ROOT)/libretro-common/include

all: $(TARGET)

$(TARGET): device_pins_test.c $(REPO_ROOT)/input/common/input_device_pins.h
	$(CC) $(CFLAGS) -o $@ device_pins_test.c

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
