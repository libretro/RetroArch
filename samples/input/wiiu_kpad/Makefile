# The Wii U Wiimote driver, against the tree's Wii U SDK headers, with
# KPADRead and the slot bookkeeping replaced by the test's. The test
# includes the driver's sources itself.
TARGET := wiiu_kpad_test

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -DRARCH_INTERNAL -DWIIU -DHAVE_HID \
           -DWIIU_HID -I$(RARCH_DIR) -I$(RARCH_DIR)/input/include \
           -I$(RARCH_DIR)/wiiu/include -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS)
endif

SOURCES := wiiu_kpad_test.c $(RARCH_DIR)/input/common/input_hid_common.c

all: $(TARGET)

$(TARGET): $(SOURCES) $(RARCH_DIR)/input/drivers_joypad/wiiu/kpad_driver.c \
           $(RARCH_DIR)/input/drivers_joypad/wiiu/pad_functions.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
