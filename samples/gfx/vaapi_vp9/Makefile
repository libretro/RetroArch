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
all: backend_nothreads_test$(EXE) backend_volatile_test$(EXE) backend_test$(EXE) owner_test$(EXE) decoder_test$(EXE) runtime_test$(EXE) picture_test$(EXE) session_test$(EXE) submit_test$(EXE) pool_test$(EXE) export_test$(EXE)
picture_test$(EXE): picture_test.c ../../../gfx/common/vaapi_vp9_picture.h ../../../libretro-common/include/formats/rvp9_hw.h ../../../libretro-common/formats/vp9/rvp9.c ../../../libretro-common/formats/vp9/rvp9_recon.inc
	$(CC) $(CFLAGS) picture_test.c ../../../libretro-common/formats/vp9/rvp9.c -o $@ $(LDFLAGS)
session_test$(EXE): session_test.c ../vaapi_hevc/session_test.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_session.h ../../../gfx/common/vaapi_vp9_session.h
	$(CC) $(CFLAGS) session_test.c -o $@ $(LDFLAGS)
submit_test$(EXE): submit_test.c ../vaapi_hevc/submit_test.c ../../../gfx/common/vaapi_vp9_submit.c ../../../gfx/common/vaapi_vp9_submit.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_vp9_picture.h
	$(CC) $(CFLAGS) submit_test.c -o $@ $(LDFLAGS)
POOL_SOURCES := ../../../gfx/common/vaapi_vp9_pool.c ../../../gfx/common/vaapi_vp9_submit.c ../../../gfx/common/vaapi_hevc_submit.c ../../../libretro-common/formats/vp9/rvp9.c
pool_test$(EXE): pool_test.c $(POOL_SOURCES) ../../../gfx/common/vaapi_vp9_pool.h ../../../gfx/common/vaapi_vp9_submit.h ../../../gfx/common/vaapi_vp9_picture.h
	$(CC) $(CFLAGS) pool_test.c $(POOL_SOURCES) -o $@ $(LDFLAGS)
rvp9_test.o: ../../../libretro-common/formats/vp9/rvp9.c ../../../libretro-common/formats/vp9/rvp9_recon.inc ../../../libretro-common/include/formats/rvp9_hw.h
	$(CC) $(CFLAGS) -c $< -o $@
EXPORT_SOURCES := $(filter-out ../../../libretro-common/formats/vp9/rvp9.c,$(POOL_SOURCES)) ../../../gfx/common/vaapi_vp9_export.c ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_frame.c
export_test$(EXE): export_test.c pool_test.c $(EXPORT_SOURCES) rvp9_test.o ../../../gfx/common/vaapi_vp9_export.h ../../../gfx/common/vaapi_hevc_export.h
	$(CC) $(CFLAGS) -DHAVE_THREADS export_test.c $(EXPORT_SOURCES) rvp9_test.o -o $@ $(LDFLAGS)
check: all
	$(RUNNER) ./backend_nothreads_test$(EXE)
	$(RUNNER) ./backend_volatile_test$(EXE)
	$(RUNNER) ./backend_test$(EXE)
	$(RUNNER) ./owner_test$(EXE)
	$(RUNNER) ./decoder_test$(EXE)
	$(RUNNER) ./runtime_test$(EXE)
	$(RUNNER) ./picture_test$(EXE)
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./submit_test$(EXE)
	$(RUNNER) ./pool_test$(EXE)
	$(RUNNER) ./export_test$(EXE)
clean:
	rm -f backend_tsan_test backend_tsan_test.exe backend_nothreads_test backend_nothreads_test.exe backend_volatile_test backend_volatile_test.exe backend_test backend_test.exe owner_test owner_test.exe decoder_test decoder_test.exe runtime_test runtime_test.exe picture_test picture_test.exe session_test session_test.exe submit_test submit_test.exe pool_test pool_test.exe export_test export_test.exe rvp9_test.o
.PHONY: all check clean

runtime_test$(EXE): runtime_test.c ../vaapi_hevc/runtime_test.c ../vaapi_hevc/session_test.c ../vaapi_hevc/submit_test.c ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_vp9_runtime.h ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_submit.c
	$(CC) $(CFLAGS) runtime_test.c -o $@ $(LDFLAGS)

decoder_test$(EXE): decoder_test.c export_test.c pool_test.c $(EXPORT_SOURCES) rvp9_test.o ../../../gfx/common/vaapi_vp9_decoder.c ../../../gfx/common/vaapi_vp9_decoder.h
	$(CC) $(CFLAGS) -DHAVE_THREADS decoder_test.c ../../../gfx/common/vaapi_vp9_decoder.c $(EXPORT_SOURCES) rvp9_test.o -o $@ $(LDFLAGS)

OWNER_SOURCES := ../../../gfx/common/vaapi_vp9_decoder.c ../../../gfx/common/vaapi_vp9_pool.c ../../../gfx/common/vaapi_vp9_submit.c ../../../gfx/common/vaapi_vp9_export.c ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_frame.c
owner_test$(EXE): owner_test.c ../vaapi_hevc/runtime_test.c ../vaapi_hevc/session_test.c ../vaapi_hevc/submit_test.c ../../../gfx/common/vaapi_vp9_owner.c ../../../gfx/common/vaapi_vp9_owner.h ../../../gfx/common/vaapi_hevc_runtime.c $(OWNER_SOURCES) rvp9_test.o
	$(CC) $(CFLAGS) -DHAVE_THREADS owner_test.c $(OWNER_SOURCES) rvp9_test.o -o $@ $(LDFLAGS)

BACKEND_SOURCES := ../../../libretro-common/rthreads/retro_eventcount.c ../../../libretro-common/rthreads/rthreads.c
backend_test$(EXE): backend_test.c ../../../gfx/common/vaapi_vp9_backend.c ../../../gfx/common/vaapi_vp9_backend.h ../../../gfx/drivers/vulkan_vaapi_vp9_backend.h $(BACKEND_SOURCES)
	$(CC) $(CFLAGS) -DHAVE_THREADS -D_DEFAULT_SOURCE -pthread backend_test.c $(BACKEND_SOURCES) -o $@ $(LDFLAGS)

backend_nothreads_test$(EXE): backend_disabled_test.c ../../../gfx/common/vaapi_vp9_backend.c ../../../gfx/common/vaapi_vp9_backend.h
	$(CC) $(CFLAGS) backend_disabled_test.c ../../../gfx/common/vaapi_vp9_backend.c -o $@ $(LDFLAGS)
backend_volatile_test$(EXE): backend_disabled_test.c ../../../gfx/common/vaapi_vp9_backend.c ../../../gfx/common/vaapi_vp9_backend.h
	$(CC) $(CFLAGS) -DHAVE_THREADS -DRETRO_ATOMIC_FORCE_VOLATILE -DRETRO_ATOMIC_SUPPRESS_WARNING backend_disabled_test.c ../../../gfx/common/vaapi_vp9_backend.c -o $@ $(LDFLAGS)

backend_tsan_test$(EXE): backend_test.c ../../../gfx/common/vaapi_vp9_backend.c ../../../gfx/common/vaapi_vp9_backend.h ../../../gfx/drivers/vulkan_vaapi_vp9_backend.h $(BACKEND_SOURCES)
	$(CC) $(filter-out -fsanitize=%,$(CFLAGS)) -DHAVE_THREADS -D_DEFAULT_SOURCE -pthread -fsanitize=thread -fno-omit-frame-pointer backend_test.c $(BACKEND_SOURCES) -o $@ $(filter-out -fsanitize=%,$(LDFLAGS)) -fsanitize=thread
