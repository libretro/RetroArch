# configuration.c's float rows in a build without the menu.
#
#   make check

ROOT := ../../..
CC   ?= gcc
EXE  ?=
CPPFLAGS += -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps -I$(ROOT)/deps/stb \
	-DRARCH_INTERNAL -DHAVE_CONFIGFILE -D_GNU_SOURCE
CFLAGS   += -O1 -Wall -std=gnu99

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: float_rows_test$(EXE)

float_rows_test$(EXE): float_rows_test.c $(ROOT)/configuration.c $(ROOT)/configuration.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fwhole-program $< $(LDFLAGS) -lm -o $@

check: float_rows_test$(EXE)
	$(RUNNER) ./float_rows_test$(EXE)

clean:
	rm -f float_rows_test float_rows_test.exe

.PHONY: all check clean
