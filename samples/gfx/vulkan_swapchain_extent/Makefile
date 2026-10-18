TARGET    := vulkan_swapchain_extent_test
REPO_ROOT := ../../..
SOURCES   := vulkan_swapchain_extent_test.c
CFLAGS    += -Wall -Wextra -std=c89 -pedantic -Wno-long-long -g1 -O0 \
             -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)/gfx/include
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/common/vulkan_swapchain_extent.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean
