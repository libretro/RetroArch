CC ?= cc
VAAPI_CFLAGS ?= $(shell pkg-config --cflags libva)
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long \
 -Wno-unused-parameter -Wframe-larger-than=2048 \
 -I../../../libretro-common/include -I../../../gfx/include $(VAAPI_CFLAGS)
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: submit_test$(EXE) picture_test$(EXE) session_test$(EXE) runtime_test$(EXE) old_sdk_test$(EXE) tiles_test$(EXE) tiles_trace_test$(EXE)
session_test$(EXE): session_test.c ../vaapi_hevc/session_test.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_av1_session.h
	$(CC) $(CFLAGS) session_test.c -o $@ $(LDFLAGS)
runtime_test$(EXE): runtime_test.c ../vaapi_hevc/runtime_test.c ../vaapi_hevc/session_test.c ../vaapi_hevc/submit_test.c ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_av1_runtime.h ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_submit.c
	$(CC) $(CFLAGS) runtime_test.c -o $@ $(LDFLAGS)
check: all
	$(RUNNER) ./submit_test$(EXE)
	$(RUNNER) ./picture_test$(EXE)
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./runtime_test$(EXE)
	$(RUNNER) ./old_sdk_test$(EXE)
	$(RUNNER) ./tiles_test$(EXE)
clean:
	rm -f submit_test submit_test.exe picture_test picture_test.exe tiles_test tiles_test.exe tiles_trace_test tiles_trace_test.exe old_sdk_test old_sdk_test.exe session_test session_test.exe runtime_test runtime_test.exe
.PHONY: all check clean

old_sdk_test$(EXE): old_sdk_test.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_av1_session.h ../../../gfx/common/vaapi_av1_runtime.h
	$(CC) $(CFLAGS) old_sdk_test.c -o $@ $(LDFLAGS)

tiles_test$(EXE): tiles_test.c ../../../gfx/common/vaapi_av1_tiles.h ../../../libretro-common/include/formats/rav1_tiles.h
	$(CC) $(CFLAGS) tiles_test.c -o $@ $(LDFLAGS)
tiles_trace_test$(EXE): tiles_trace_test.c ../../../gfx/common/vaapi_av1_picture.h ../vulkan_video_caps/av1_params_test.c ../vulkan_video_caps/av1_session_data.h ../../../gfx/common/vulkan_av1_params.h ../../../gfx/common/vaapi_av1_tiles.h
	$(CC) $(CFLAGS) -Wno-unused-function tiles_trace_test.c -o $@ $(LDFLAGS)
trace-check: tiles_trace_test$(EXE)
	AV1_VULKAN_TEST=./tiles_trace_test$(EXE) python3 ../vulkan_video_caps/av1_fixtures.py
.PHONY: trace-check

picture_test$(EXE): picture_test.c ../../../gfx/common/vaapi_av1_picture.h ../../../gfx/common/vaapi_av1_tiles.h
	$(CC) $(CFLAGS) picture_test.c -o $@ $(LDFLAGS)

submit_test$(EXE): submit_test.c ../../../gfx/common/vaapi_av1_submit.c ../../../gfx/common/vaapi_av1_submit.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_av1_picture.h
	$(CC) $(CFLAGS) submit_test.c -o $@ $(LDFLAGS)
