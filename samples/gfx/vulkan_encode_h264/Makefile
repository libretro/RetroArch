CC ?= cc
RA := ../../..
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I$(RA)/libretro-common/include -I$(RA)/gfx/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: params_test$(EXE) session_test$(EXE)
params_test$(EXE): params_test.c $(RA)/gfx/common/vulkan_encode_h264_params.h $(RA)/gfx/include/vk_video/vulkan_video_codec_h264std.h
	$(CC) $(CFLAGS) params_test.c -o $@ $(LDFLAGS)
session_test$(EXE): session_test.c $(RA)/gfx/common/vulkan_encode_h264_session.c $(RA)/gfx/common/vulkan_encode_h264_session.h $(RA)/gfx/common/vulkan_encode_h264_params.h
	$(CC) $(CFLAGS) session_test.c -o $@ $(LDFLAGS)
check: all
	$(RUNNER) ./params_test$(EXE)
	$(RUNNER) ./session_test$(EXE)
clean:
	rm -f params_test params_test.exe session_test session_test.exe
.PHONY: all check clean
