# The pattern core and the image gate script; see rslc_image_check.sh.
# No check target: the gate needs a display and a Vulkan device.
TARGET    := pattern_core.so
REPO_ROOT := ../../..
CFLAGS    += -Wall -Wextra -pedantic -std=c89 -Wno-long-long -O2 -fPIC \
             -I$(REPO_ROOT)/libretro-common/include

all: $(TARGET)
$(TARGET): pattern_core.c
	$(CC) $(CFLAGS) -shared -o $@ $<
clean:
	rm -f $(TARGET)
.PHONY: all clean
