TARGET    := switch_nx_2d_test
TARGET_S2 := switch_nx_2d_s2_test

ROOT   := ../../..
LRC    := $(ROOT)/libretro-common

# The test includes gfx/drivers/switch_nx_gfx.c as it ships, GPU path
# and all, against the libnx stand-in headers; the 2D engine is a model
# in the test. Built twice: for the Switch, and for the Switch 2's GSV
# path.
SOURCES := \
	switch_nx_2d_test.c \
	$(LRC)/gfx/scaler/scaler.c \
	$(LRC)/gfx/scaler/scaler_filter.c \
	$(LRC)/gfx/scaler/scaler_int.c \
	$(LRC)/gfx/scaler/pixconv.c

CFLAGS  += -Wall -std=gnu99 -g1 -O1 \
           -I$(ROOT)/tools/platform_stubs/libnx -I$(ROOT) \
           -I$(LRC)/include -I$(ROOT)/deps \
           -DHAVE_LIBNX -DSWITCH -D__SWITCH__ -DRARCH_INTERNAL \
           -DHAVE_OVERLAY -DHAVE_MENU -DHAVE_THREADS
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_S2)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

$(TARGET_S2): $(SOURCES)
	$(CC) $(CFLAGS) -D__SWITCH2__ -o $@ $(SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)
	./$(TARGET_S2)

clean:
	rm -f $(TARGET) $(TARGET).exe $(TARGET_S2) $(TARGET_S2).exe

.PHONY: all check clean
