# gfx/common/vulkan_memory_type.h: which memory types a buffer the CPU only
# writes goes to - the device-local host-visible type where the device maps
# all of its memory (resizable BAR, unified memory), the caller's own
# host-visible choice everywhere else. No device needed.

TARGET    := vulkan_memory_type_test
REPO_ROOT := ../../..
SOURCES   := vulkan_memory_type_test.c
CFLAGS    += -Wall -Wextra -std=c89 -pedantic -Wno-long-long -g -O0 \
             -I$(REPO_ROOT)/libretro-common/include
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/common/vulkan_memory_type.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean
