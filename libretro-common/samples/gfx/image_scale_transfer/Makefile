LRC := ../../..
CC ?= cc
EXE ?=
CFLAGS += -O2 -std=c89 -Wall -Wextra -Werror -Wno-unused-parameter -Wdeclaration-after-statement -I$(LRC)/include
ifneq ($(SANITIZER),)
CFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

SCALING_SOURCE ?= $(LRC)/formats/image_texture_scale.c
SOURCES := image_scale_transfer_test.c $(LRC)/gfx/scaler/scaler.c $(LRC)/gfx/scaler/scaler_int.c $(LRC)/gfx/scaler/pixconv.c $(LRC)/gfx/scaler/scaler_filter.c

all: image_scale_transfer_test$(EXE)

image_scale_transfer_test$(EXE): $(SOURCES) $(SCALING_SOURCE)
	$(CC) $(CFLAGS) -DSCALING_SOURCE='"$(abspath $(SCALING_SOURCE))"' $(SOURCES) $(LDFLAGS) -lm -o $@

check: image_scale_transfer_test$(EXE)
	$(RUNNER) ./image_scale_transfer_test$(EXE)

bench: image_scale_transfer_test$(EXE)
	$(RUNNER) ./image_scale_transfer_test$(EXE) --bench

clean:
	rm -f image_scale_transfer_test image_scale_transfer_test.exe

.PHONY: all check bench clean
