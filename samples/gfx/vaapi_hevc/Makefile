CC ?= cc
VAAPI_CFLAGS ?= $(shell pkg-config --cflags libva libdrm)
CFLAGS += -DHAVE_DYLIB -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long \
 -Wframe-larger-than=2048 -I../../../libretro-common/include $(VAAPI_CFLAGS)
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifeq ($(shell uname -s),Linux)
NATIVE_PROBE = native_probe
endif
all: test$(EXE) session_test$(EXE) runtime_test$(EXE) submit_test$(EXE) pool_test$(EXE) abi-check $(NATIVE_PROBE)
runtime_test$(EXE): runtime_test.c session_test.c submit_test.c ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_hevc_runtime.h ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_session.h
	$(CC) $(CFLAGS) runtime_test.c -o $@ $(LDFLAGS)
submit_test$(EXE): submit_test.c ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h
	$(CC) $(CFLAGS) submit_test.c -o $@ $(LDFLAGS)
pool_test$(EXE): pool_test.c test.c ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_pool.h ../../../gfx/common/vaapi_hevc_packet.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) -Wno-unused-parameter pool_test.c ../../../libretro-common/formats/h265/rh265.c -o $@ $(LDFLAGS) -lm
native_probe: native_probe.c ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_hevc_session.c ../../../libretro-common/dynamic/dylib.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -ldl
session_test$(EXE): session_test.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_session.h
	$(CC) $(CFLAGS) session_test.c -o $@ $(LDFLAGS)
abi-check: abi.c ../../../gfx/common/vaapi_hevc_session.h
	$(CC) $(CFLAGS) -fsyntax-only abi.c
test$(EXE): test.c ../../../gfx/common/vaapi_hevc_packet.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../gfx/common/vaapi_hevc.h \
 ../../../libretro-common/include/formats/rh265.h \
 ../../../libretro-common/formats/h265/rh265.c \
 ../videotoolbox_hevc/sequence_data.h ../videotoolbox_hevc/reorder_data.h
	$(CC) $(CFLAGS) -Wno-unused-parameter test.c ../../../libretro-common/formats/h265/rh265.c -o $@ $(LDFLAGS) -lm
check: all
	$(RUNNER) ./test$(EXE)
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./runtime_test$(EXE)
	$(RUNNER) ./submit_test$(EXE)
	$(RUNNER) ./pool_test$(EXE)
ifneq ($(NATIVE_PROBE),)
	./native_probe
endif
clean:
	rm -f test test.exe session_test session_test.exe runtime_test runtime_test.exe submit_test submit_test.exe pool_test pool_test.exe native_probe
.PHONY: all check clean abi-check
