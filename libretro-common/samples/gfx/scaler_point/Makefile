LRC := ../../..
CC ?= cc
EXE ?=
CFLAGS += -O2 -std=c89 -Wall -Wextra -Werror -Wno-unused-parameter -Wdeclaration-after-statement -I$(LRC)/include
ifneq ($(SANITIZER),)
CFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

SCALER_SOURCE ?= $(LRC)/gfx/scaler/scaler.c
SOURCES := scaler_point_test.c $(LRC)/gfx/scaler/scaler_int.c $(LRC)/gfx/scaler/pixconv.c $(LRC)/gfx/scaler/scaler_filter.c

all: scaler_point_test$(EXE)

scaler_point_test$(EXE): $(SOURCES) $(SCALER_SOURCE)
	$(CC) $(CFLAGS) -DSCALER_SOURCE='"$(abspath $(SCALER_SOURCE))"' $(SOURCES) $(LDFLAGS) -lm -o $@

check: scaler_point_test$(EXE)
	$(RUNNER) ./scaler_point_test$(EXE)

bench: scaler_point_test$(EXE)
	$(RUNNER) ./scaler_point_test$(EXE) --bench

clean:
	rm -f scaler_point_test scaler_point_test.exe

.PHONY: all check bench clean
