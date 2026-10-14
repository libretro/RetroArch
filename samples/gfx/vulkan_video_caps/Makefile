CC ?= cc
RA := ../../..
TARGET := vulkan_video_caps_test$(EXE)
CFLAGS += -O1 -g1 -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement \
          -I$(RA)/libretro-common/include -I$(RA)/gfx/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): vulkan_video_caps_test.c $(RA)/gfx/common/vulkan_video_caps.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

probe$(EXE): probe.c $(RA)/gfx/common/vulkan_video_caps.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(if $(filter .exe,$(EXE)),,-ldl)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -f vulkan_video_caps_test vulkan_video_caps_test.exe probe probe.exe

.PHONY: all check clean
