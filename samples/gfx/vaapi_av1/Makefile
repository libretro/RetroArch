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
all: backend_test$(EXE) backend_nothreads_test$(EXE) backend_volatile_test$(EXE) owner_test$(EXE) decoder_trace_test$(EXE) busy_test$(EXE) decoder_test$(EXE) export_test$(EXE) pool_test$(EXE) submit_test$(EXE) picture_test$(EXE) session_test$(EXE) runtime_test$(EXE) old_sdk_test$(EXE) tiles_test$(EXE) tiles_trace_test$(EXE)
session_test$(EXE): session_test.c ../vaapi_hevc/session_test.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_av1_session.h
	$(CC) $(CFLAGS) session_test.c -o $@ $(LDFLAGS)
runtime_test$(EXE): runtime_test.c ../vaapi_hevc/runtime_test.c ../vaapi_hevc/session_test.c ../vaapi_hevc/submit_test.c ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_av1_runtime.h ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_submit.c
	$(CC) $(CFLAGS) runtime_test.c -o $@ $(LDFLAGS)
check: all
	$(RUNNER) ./backend_test$(EXE)
	$(RUNNER) ./backend_nothreads_test$(EXE)
	$(RUNNER) ./backend_volatile_test$(EXE)
	$(RUNNER) ./owner_test$(EXE)
	$(RUNNER) ./busy_test$(EXE)
	$(RUNNER) ./decoder_test$(EXE)
	$(RUNNER) ./export_test$(EXE)
	$(RUNNER) ./pool_test$(EXE)
	$(RUNNER) ./submit_test$(EXE)
	$(RUNNER) ./picture_test$(EXE)
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./runtime_test$(EXE)
	$(RUNNER) ./old_sdk_test$(EXE)
	$(RUNNER) ./tiles_test$(EXE)
clean:
	rm -f backend_tsan_test backend_tsan_test.exe backend_test backend_test.exe backend_nothreads_test backend_nothreads_test.exe backend_volatile_test backend_volatile_test.exe owner_test owner_test.exe decoder_trace_test decoder_trace_test.exe busy_test busy_test.exe decoder_test decoder_test.exe export_test export_test.exe pool_test pool_test.exe submit_test submit_test.exe picture_test picture_test.exe tiles_test tiles_test.exe tiles_trace_test tiles_trace_test.exe old_sdk_test old_sdk_test.exe session_test session_test.exe runtime_test runtime_test.exe
.PHONY: all check clean

old_sdk_test$(EXE): old_sdk_test.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_av1_session.h ../../../gfx/common/vaapi_av1_runtime.h
	$(CC) $(CFLAGS) old_sdk_test.c -o $@ $(LDFLAGS)

tiles_test$(EXE): tiles_test.c ../../../gfx/common/vaapi_av1_tiles.h ../../../libretro-common/include/formats/rav1_tiles.h
	$(CC) $(CFLAGS) tiles_test.c -o $@ $(LDFLAGS)
tiles_trace_test$(EXE): tiles_trace_test.c ../../../gfx/common/vaapi_av1_picture.h ../vulkan_video_caps/av1_params_test.c ../vulkan_video_caps/av1_session_data.h ../../../gfx/common/vulkan_av1_params.h ../../../gfx/common/vaapi_av1_tiles.h
	$(CC) $(CFLAGS) -Wno-unused-function tiles_trace_test.c -o $@ $(LDFLAGS)
trace-check: tiles_trace_test$(EXE) decoder_trace_test$(EXE)
	AV1_VULKAN_TEST=./tiles_trace_test$(EXE) python3 ../vulkan_video_caps/av1_fixtures.py
	AV1_VULKAN_TEST=./decoder_trace_test$(EXE) python3 ../vulkan_video_caps/av1_fixtures.py
.PHONY: trace-check

picture_test$(EXE): picture_test.c ../../../gfx/common/vaapi_av1_picture.h ../../../gfx/common/vaapi_av1_tiles.h
	$(CC) $(CFLAGS) picture_test.c -o $@ $(LDFLAGS)

submit_test$(EXE): submit_test.c ../../../gfx/common/vaapi_av1_submit.c ../../../gfx/common/vaapi_av1_submit.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_av1_picture.h
	$(CC) $(CFLAGS) submit_test.c -o $@ $(LDFLAGS)

pool_test$(EXE): pool_test.c ../../../gfx/common/vaapi_av1_pool.c ../../../gfx/common/vaapi_av1_pool.h ../../../gfx/common/vaapi_av1_submit.c ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_av1_picture.h
	$(CC) $(CFLAGS) pool_test.c -o $@ $(LDFLAGS)

EXPORT_SOURCES := ../../../gfx/common/vaapi_av1_export.c ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_frame.c
export_test$(EXE): export_test.c pool_test.c $(EXPORT_SOURCES) ../../../gfx/common/vaapi_av1_export.h ../../../gfx/common/vaapi_av1_pool.c ../../../gfx/common/vaapi_av1_submit.c
	$(CC) $(CFLAGS) -DHAVE_THREADS export_test.c $(EXPORT_SOURCES) -o $@ $(LDFLAGS)

decoder_test$(EXE): decoder_test.c export_test.c pool_test.c $(EXPORT_SOURCES) ../../../gfx/common/vaapi_av1_decoder.c ../../../gfx/common/vaapi_av1_decoder.h ../../../gfx/common/vaapi_av1_pool.c ../../../gfx/common/vaapi_av1_submit.c
	$(CC) $(CFLAGS) -DHAVE_THREADS decoder_test.c $(EXPORT_SOURCES) -o $@ $(LDFLAGS)

busy_test$(EXE): busy_test.c decoder_test.c export_test.c pool_test.c $(EXPORT_SOURCES) ../../../gfx/common/vaapi_av1_decoder.c ../../../gfx/common/vaapi_av1_decoder.h ../../../gfx/common/vaapi_av1_pool.c ../../../gfx/common/vaapi_av1_submit.c
	$(CC) $(CFLAGS) -DHAVE_THREADS busy_test.c $(EXPORT_SOURCES) -o $@ $(LDFLAGS)

DECODER_TRACE_SOURCES := ../../../gfx/common/vaapi_av1_decoder.c ../../../gfx/common/vaapi_av1_pool.c ../../../gfx/common/vaapi_av1_submit.c ../../../gfx/common/vaapi_hevc_submit.c $(EXPORT_SOURCES)
decoder_trace_test$(EXE): decoder_trace_test.c tiles_trace_test.c $(DECODER_TRACE_SOURCES) ../vulkan_video_caps/av1_params_test.c
	$(CC) $(CFLAGS) -DHAVE_THREADS -Wno-unused-function decoder_trace_test.c $(DECODER_TRACE_SOURCES) -o $@ $(LDFLAGS)

OWNER_SOURCES := ../../../gfx/common/vaapi_av1_decoder.c ../../../gfx/common/vaapi_av1_pool.c ../../../gfx/common/vaapi_av1_submit.c $(EXPORT_SOURCES)
owner_test$(EXE): owner_test.c ../vaapi_hevc/runtime_test.c ../vaapi_hevc/session_test.c ../vaapi_hevc/submit_test.c ../../../gfx/common/vaapi_av1_owner.c ../../../gfx/common/vaapi_av1_owner.h $(OWNER_SOURCES)
	$(CC) $(CFLAGS) -DHAVE_THREADS owner_test.c $(OWNER_SOURCES) -o $@ $(LDFLAGS)

BACKEND_SOURCES := ../../../libretro-common/rthreads/retro_eventcount.c ../../../libretro-common/rthreads/rthreads.c $(DECODER_TRACE_SOURCES)
backend_test$(EXE): backend_test.c ../../../gfx/common/vaapi_av1_backend.c ../../../gfx/common/vaapi_av1_backend.h ../../../gfx/drivers/vulkan_vaapi_av1_backend.h $(BACKEND_SOURCES)
	$(CC) $(CFLAGS) -DHAVE_THREADS -D_DEFAULT_SOURCE -pthread backend_test.c $(BACKEND_SOURCES) -o $@ $(LDFLAGS)
backend_nothreads_test$(EXE): backend_disabled_test.c ../../../gfx/common/vaapi_av1_backend.c ../../../gfx/common/vaapi_av1_backend.h
	$(CC) $(CFLAGS) backend_disabled_test.c ../../../gfx/common/vaapi_av1_backend.c -o $@ $(LDFLAGS)
backend_volatile_test$(EXE): backend_disabled_test.c ../../../gfx/common/vaapi_av1_backend.c ../../../gfx/common/vaapi_av1_backend.h
	$(CC) $(CFLAGS) -DHAVE_THREADS -DRETRO_ATOMIC_FORCE_VOLATILE -DRETRO_ATOMIC_SUPPRESS_WARNING backend_disabled_test.c ../../../gfx/common/vaapi_av1_backend.c -o $@ $(LDFLAGS)

backend_tsan_test$(EXE): backend_test.c ../../../gfx/common/vaapi_av1_backend.c ../../../gfx/common/vaapi_av1_backend.h ../../../gfx/drivers/vulkan_vaapi_av1_backend.h $(BACKEND_SOURCES)
	$(CC) $(filter-out -fsanitize=%,$(CFLAGS)) -DHAVE_THREADS -D_DEFAULT_SOURCE -pthread -fsanitize=thread -fno-omit-frame-pointer backend_test.c $(BACKEND_SOURCES) -o $@ $(filter-out -fsanitize=%,$(LDFLAGS)) -fsanitize=thread
