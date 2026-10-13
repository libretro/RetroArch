# The Switch joypad driver (the libnx build), against the tree's libnx
# stubs, with the libnx calls defined by the test, which includes the
# driver's source itself.
TARGET := switch_joypad_test

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -DRARCH_INTERNAL -DSWITCH -DHAVE_LIBNX \
           -I$(RARCH_DIR)/tools/platform_stubs/libnx -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): switch_joypad_test.c $(RARCH_DIR)/input/drivers_joypad/switch_joypad.c
	$(CC) $(CFLAGS) -o $@ switch_joypad_test.c $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
