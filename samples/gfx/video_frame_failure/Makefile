ROOT := ../../..
LRC := $(ROOT)/libretro-common
CC ?= cc
CFLAGS += -std=gnu89 -Wall -Wextra -Werror -Wno-unused-parameter -Wdeclaration-after-statement -I$(LRC)/include
ifneq ($(SANITIZER),)
CFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

video_frame_failure_test$(EXE): video_frame_failure_test.c $(LRC)/gfx/scaler/scaler.c $(LRC)/gfx/scaler/scaler_int.c $(LRC)/gfx/scaler/pixconv.c $(LRC)/gfx/scaler/scaler_filter.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -lm -o $@

check: video_frame_failure_test$(EXE)
	$(RUNNER) ./video_frame_failure_test$(EXE)

clean:
	rm -f video_frame_failure_test video_frame_failure_test.exe

.PHONY: check clean
