ROOT := ../../..
CC   ?= gcc
CPPFLAGS += -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps -I$(ROOT)/deps/stb \
	-DRARCH_INTERNAL -DHAVE_CONFIGFILE -D_GNU_SOURCE
CFLAGS   += -O1 -Wall

all: defaults_target_test

defaults_target_test: defaults_target_test.c $(ROOT)/configuration.c $(ROOT)/configuration.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fwhole-program $< $(LDFLAGS) -lm -o $@

check: defaults_target_test
	./defaults_target_test

clean:
	rm -f defaults_target_test defaults_target_test.exe

.PHONY: all check clean
