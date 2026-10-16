CC ?= cc
RA := ../../..
CFLAGS += -O1 -g1 -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
TARGET := device_test$(EXE)
SDK_TARGET := device_test_sdk$(EXE)
all: $(TARGET) $(SDK_TARGET)
$(TARGET): device_test.c $(RA)/gfx/common/nvenc_d3d12.c $(RA)/gfx/common/nvenc_d3d12.h $(RA)/deps/nvenc/nvEncodeAPI.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(if $(filter .exe,$(EXE)),-ld3d12 -ldxguid,)
$(SDK_TARGET): device_test.c $(RA)/gfx/common/nvenc_d3d12.c $(RA)/gfx/common/nvenc_d3d12.h $(RA)/deps/nvenc/nvEncodeAPI.h
	$(CC) $(CFLAGS) -I$(RA)/gfx/include/dxsdk -o $@ $< $(LDFLAGS) $(if $(filter .exe,$(EXE)),-ld3d12 -ldxguid,)
check: all
	python3 run.py --runner "$(RUNNER)" ./$(TARGET)
	python3 run.py --runner "$(RUNNER)" ./$(SDK_TARGET)
clean:
	rm -f device_test device_test.exe device_test_sdk device_test_sdk.exe
.PHONY: all check clean
