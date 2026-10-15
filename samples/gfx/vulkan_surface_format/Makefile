# gfx/common/vulkan_surface_format.h: which 8-bit surface format the
# swapchain is created with - B8G8R8A8_UNORM in the sRGB colour space
# wherever the surface lists it, whatever order the driver lists it in.
# No device needed.

TARGET    := vulkan_surface_format_test
REPO_ROOT := ../../..
SOURCES   := vulkan_surface_format_test.c
CFLAGS    += -Wall -Wextra -std=c89 -pedantic -Wno-long-long -g1 -O0 \
             -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)/gfx/include
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/common/vulkan_surface_format.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean
