CC ?= cc
RA := ../../..
CFLAGS += -DHAVE_THREADS -O1 -g1 -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
SRC := device_test.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c
CAPTURE := $(RA)/record/record_gpu_capture_d3d12.h $(RA)/record/record_gpu_capture_d3d12.c $(RA)/record/shaders/capture_rgb.h
METADATA := $(RA)/libretro-common/include/formats/video_color.h
TARGET := device_test$(EXE)
SDK_TARGET := device_test_sdk$(EXE)
all: $(TARGET) $(SDK_TARGET)
$(TARGET): $(SRC) $(CAPTURE) $(METADATA) $(RA)/record/record_gpu_queue.h $(RA)/gfx/common/nvenc_d3d12.c $(RA)/gfx/common/nvenc_d3d12.h $(RA)/deps/nvenc/nvEncodeAPI.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS) $(if $(filter .exe,$(EXE)),-ld3d12 -ldxguid,)
$(SDK_TARGET): $(SRC) $(CAPTURE) $(METADATA) $(RA)/record/record_gpu_queue.h $(RA)/gfx/common/nvenc_d3d12.c $(RA)/gfx/common/nvenc_d3d12.h $(RA)/deps/nvenc/nvEncodeAPI.h
	$(CC) $(CFLAGS) -I$(RA)/gfx/include/dxsdk -o $@ $(SRC) $(LDFLAGS) $(if $(filter .exe,$(EXE)),-ld3d12 -ldxguid,)
check: all
	python3 $(RA)/record/shaders/generate_rgb.py --check
	python3 run.py --runner "$(RUNNER)" ./$(TARGET)
	python3 run.py --runner "$(RUNNER)" ./$(SDK_TARGET)
clean:
	rm -f device_test device_test.exe device_test_sdk device_test_sdk.exe
.PHONY: all check clean
