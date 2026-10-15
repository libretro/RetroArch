# The libogc GameCube/Wii joypad driver (the Wii build), with libogc
# replaced by the test's. The test includes the driver's source itself.
TARGET := gx_pad_sticks_test

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -DRARCH_INTERNAL -DGEKKO -DHW_RVL \
           -Ilibogc -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-sanitize-recover=all -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS) -lm
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-sanitize-recover=all -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS) -lm
endif

all: $(TARGET)

$(TARGET): gx_pad_sticks_test.c $(RARCH_DIR)/input/drivers_joypad/gx_joypad_libogc.c
	$(CC) $(CFLAGS) -o $@ gx_pad_sticks_test.c $(LDFLAGS) -lm

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
