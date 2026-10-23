ROOT := ../../..
CC   ?= gcc
EXE  ?=
CPPFLAGS += -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps -I$(ROOT)/deps/stb \
	-DRARCH_INTERNAL -DHAVE_CONFIGFILE -DHAVE_NETWORKING -D_GNU_SOURCE
CFLAGS   += -O1 -Wall -std=gnu99

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: cli_override_rows_test$(EXE)

cli_override_rows_test$(EXE): cli_override_rows_test.c $(ROOT)/configuration.c $(ROOT)/configuration.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fwhole-program $< $(LDFLAGS) -lm -o $@

check: cli_override_rows_test$(EXE)
	$(RUNNER) ./cli_override_rows_test$(EXE)

clean:
	rm -f cli_override_rows_test cli_override_rows_test.exe

.PHONY: all check clean
