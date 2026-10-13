# The PSP joypad driver (the PSP build), against the tree's PSP stubs,
# with the controller calls defined by the test, which includes the
# driver's source itself.
TARGET := psp_pad_read_test

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -DRARCH_INTERNAL -DPSP \
           -I$(RARCH_DIR)/tools/platform_stubs/psp -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): psp_pad_read_test.c $(RARCH_DIR)/input/drivers_joypad/psp_joypad.c
	$(CC) $(CFLAGS) -o $@ psp_pad_read_test.c $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
