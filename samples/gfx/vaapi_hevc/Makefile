CC ?= cc
VAAPI_CFLAGS ?= $(shell pkg-config --cflags libva)
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long \
 -Wframe-larger-than=2048 -I../../../libretro-common/include $(VAAPI_CFLAGS)
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: test$(EXE) session_test$(EXE) abi-check
session_test$(EXE): session_test.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_session.h
	$(CC) $(CFLAGS) session_test.c -o $@ $(LDFLAGS)
abi-check: abi.c ../../../gfx/common/vaapi_hevc_session.h
	$(CC) $(CFLAGS) -fsyntax-only abi.c
test$(EXE): test.c ../../../gfx/common/vaapi_hevc.h \
 ../../../libretro-common/include/formats/rh265.h \
 ../../../libretro-common/formats/h265/rh265.c \
 ../videotoolbox_hevc/sequence_data.h ../videotoolbox_hevc/reorder_data.h
	$(CC) $(CFLAGS) -Wno-unused-parameter test.c ../../../libretro-common/formats/h265/rh265.c -o $@ $(LDFLAGS) -lm
check: all
	$(RUNNER) ./test$(EXE)
	$(RUNNER) ./session_test$(EXE)
clean:
	rm -f test test.exe session_test session_test.exe
.PHONY: all check clean abi-check
