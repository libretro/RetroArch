CC ?= cc
RA := ../../..
TARGET := vulkan_video_caps_test$(EXE)
PARAMS := hevc_params_test$(EXE)
SESSION := hevc_session_test$(EXE)
DECODERS := $(RA)/libretro-common/formats/h265/rh265.c $(RA)/libretro-common/formats/mp4/rmp4.c
HEVC_DEPS := $(RA)/gfx/common/vulkan_hevc_params.h $(RA)/libretro-common/include/formats/rh265.h \
             ../d3d11_video_decode/hevc_device_data.h
CFLAGS += -O1 -g1 -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement \
          -I$(RA)/libretro-common/include -I$(RA)/gfx/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(PARAMS)

$(PARAMS): hevc_params_test.c $(HEVC_DEPS) $(DECODERS)
	$(CC) $(CFLAGS) -o $@ hevc_params_test.c $(DECODERS) $(LDFLAGS) -lm

$(SESSION): probe.c hevc_session.h hevc_params_test.c $(DECODERS) $(HEVC_DEPS)
	$(CC) $(CFLAGS) -DHEVC_SESSION_TEST -o $@ probe.c $(DECODERS) $(LDFLAGS) -lm $(if $(filter .exe,$(EXE)),,-ldl)

$(TARGET): vulkan_video_caps_test.c $(RA)/gfx/common/vulkan_video_caps.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

probe$(EXE): probe.c $(RA)/gfx/common/vulkan_video_caps.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(if $(filter .exe,$(EXE)),,-ldl)

check: all
	$(RUNNER) ./$(TARGET)
	$(RUNNER) ./$(PARAMS)

session-check: $(SESSION)
	$(RUNNER) ./$(SESSION)

clean:
	rm -f vulkan_video_caps_test vulkan_video_caps_test.exe probe probe.exe
	rm -f hevc_params_test hevc_params_test.exe hevc_session_test hevc_session_test.exe

.PHONY: all check session-check clean
