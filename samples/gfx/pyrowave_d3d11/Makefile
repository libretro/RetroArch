RA := ../../..
COMMON := $(RA)/libretro-common
ifeq ($(OS),Windows_NT)
CC ?= gcc
else
ifeq ($(origin CC),default)
CC := x86_64-w64-mingw32-gcc
endif
endif
TARGET := pyrowave_d3d11_test.exe
CFLAGS += -O2 -std=c89 -pedantic -Wall -Wextra -Werror -I$(COMMON)/include
SRC := pyrowave_d3d11_test.c $(RA)/record/record_pyrowave_d3d11.c \
       $(RA)/record/record_gpu_queue.c $(COMMON)/queues/retro_spsc.c \
       $(COMMON)/formats/pyrowave/rpyrowave.c $(COMMON)/formats/pyrowave/rpyrowave_image.c \
       $(COMMON)/formats/pyrowave/rpyrowave_gpu.c
SHADERS := $(COMMON)/formats/pyrowave/shaders/precompiled
all: $(TARGET)
$(TARGET): $(SRC) $(RA)/record/record_pyrowave_d3d11.h
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -ld3d11 -ldxguid -lm -o $@
check: $(TARGET)
	$(RUNNER) ./$(TARGET) $(SHADERS)/color.dxbc $(SHADERS)/dwt.dxbc $(SHADERS)/packet.dxbc
clean:
	rm -f $(TARGET)
.PHONY: all check clean
