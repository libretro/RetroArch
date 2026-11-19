TARGET := deko3d_test

ROOT   := ../../..
LRC    := $(ROOT)/libretro-common

# The test includes gfx/drivers/deko3d.c as it ships, against the
# deko3d and libnx stand-in headers; the device is a model in the test.
SOURCES := deko3d_test.c

CFLAGS  += -Wall -std=gnu99 -g1 -O1 \
           -I$(ROOT)/tools/platform_stubs/deko3d \
           -I$(ROOT)/tools/platform_stubs/libnx -I$(ROOT) \
           -I$(LRC)/include -I$(ROOT)/deps \
           -DHAVE_LIBNX -DSWITCH -D__SWITCH__ -DHAVE_DEKO3D \
           -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_THREADS -DHAVE_SHADERPIPELINE
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
