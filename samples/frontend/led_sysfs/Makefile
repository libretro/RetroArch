TARGET := led_sysfs_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

# The test includes led/drivers/led_sys_linux.c itself.
SOURCES := \
	led_sysfs_test.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c

CFLAGS  += -Wall -std=gnu89 -g1 -O1 \
           -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -rf led_sysfs_root
	rm -f $(TARGET) $(TARGET).exe *.o

.PHONY: all check clean
