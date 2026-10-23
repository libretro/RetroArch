CC ?= cc
VAAPI_CFLAGS ?= $(shell pkg-config --cflags libva)
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long \
 -Wno-unused-parameter -Wframe-larger-than=2048 \
 -I../../../libretro-common/include $(VAAPI_CFLAGS)
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: session_test$(EXE) runtime_test$(EXE) old_sdk_test$(EXE)
session_test$(EXE): session_test.c ../vaapi_hevc/session_test.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_av1_session.h
	$(CC) $(CFLAGS) session_test.c -o $@ $(LDFLAGS)
runtime_test$(EXE): runtime_test.c ../vaapi_hevc/runtime_test.c ../vaapi_hevc/session_test.c ../vaapi_hevc/submit_test.c ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_av1_runtime.h ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_submit.c
	$(CC) $(CFLAGS) runtime_test.c -o $@ $(LDFLAGS)
check: all
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./runtime_test$(EXE)
	$(RUNNER) ./old_sdk_test$(EXE)
clean:
	rm -f old_sdk_test old_sdk_test.exe session_test session_test.exe runtime_test runtime_test.exe
.PHONY: all check clean

old_sdk_test$(EXE): old_sdk_test.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_av1_session.h ../../../gfx/common/vaapi_av1_runtime.h
	$(CC) $(CFLAGS) old_sdk_test.c -o $@ $(LDFLAGS)
