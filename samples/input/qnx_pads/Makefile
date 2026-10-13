# The QNX input and joypad drivers, in their unity-build order, against
# the tree's BlackBerry 10 SDK stand-ins with libscreen's calls defined
# by the test, which includes the drivers' sources itself.
TARGET := qnx_pads_test

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -D__QNX__ -DHAVE_BB10 -DRARCH_INTERNAL \
           -isystem $(RARCH_DIR)/tools/qnx_standin \
           -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include -I$(RARCH_DIR)/deps

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS)
endif

SOURCES := qnx_pads_test.c $(LIBRETRO_COMM_DIR)/compat/compat_strl.c

all: $(TARGET)

$(TARGET): $(SOURCES) $(RARCH_DIR)/input/drivers/qnx_input.c \
           $(RARCH_DIR)/input/drivers_joypad/qnx_joypad.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
