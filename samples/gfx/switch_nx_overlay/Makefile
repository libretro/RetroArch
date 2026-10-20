TARGET := switch_nx_overlay_test

ROOT   := ../../..
LRC    := $(ROOT)/libretro-common

# The test includes gfx/drivers/switch_nx_gfx.c as it ships, against
# the libnx stand-in headers the compile matrix builds it with.
SOURCES := \
	switch_nx_overlay_test.c \
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

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
