# gfx/common/d3d12_upload_heap.h: the heap the D3D12 driver puts the
# buffers the CPU writes in - the GPU upload heap (video memory) where
# the device maps all of it, the upload heap everywhere else - and a
# round trip through a buffer made there on the D3D12 device there is.
#
# Windows only. Elsewhere it is cross-built with mingw-w64 and run
# under Wine, where vkd3d implements D3D12 over Vulkan; with no D3D12
# device the round trip is skipped and the rest still runs. See the
# test for running it against vkd3d-proton, which has a GPU upload heap.
# SANITIZER is taken and ignored: mingw-w64 has none of them.

TARGET    := d3d12_upload_heap_test.exe
REPO_ROOT := ../../..

ifeq ($(OS),Windows_NT)
RUN :=
else
CC  := x86_64-w64-mingw32-gcc
RUN := WINEDEBUG=-all $(or $(WINE),wine)
endif

CFLAGS += -Wall -Wextra -std=c89 -pedantic -Wno-long-long -g -O1 \
          -I$(REPO_ROOT)/libretro-common/include
LDLIBS += -ld3d12 -ldxguid -luuid

all: $(TARGET)

$(TARGET): d3d12_upload_heap_test.c $(REPO_ROOT)/gfx/common/d3d12_upload_heap.h
	$(CC) $(CFLAGS) -o $@ d3d12_upload_heap_test.c $(LDFLAGS) $(LDLIBS)

check: all
	$(RUN) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
