# The PS3 joypad driver (the PSL1GHT build), against the tree's PS3 stubs,
# with the pad calls defined by the test, which includes the driver's
# source itself.
TARGET := ps3_joypad_test

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -DRARCH_INTERNAL -D__PS3__ -D__PSL1GHT__ \
           -I$(RARCH_DIR)/tools/platform_stubs/ps3 \
           -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): ps3_joypad_test.c $(RARCH_DIR)/input/drivers_joypad/ps3_joypad.c
	$(CC) $(CFLAGS) -o $@ ps3_joypad_test.c $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
