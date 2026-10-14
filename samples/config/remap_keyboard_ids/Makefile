ROOT := ../../..
CC   ?= gcc
CPPFLAGS += -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps -I$(ROOT)/deps/stb \
	-DRARCH_INTERNAL -DHAVE_CONFIGFILE -D_GNU_SOURCE
CFLAGS   += -O1 -Wall

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: remap_keyboard_ids_test

remap_keyboard_ids_test: remap_keyboard_ids_test.c $(ROOT)/configuration.c $(ROOT)/configuration.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fwhole-program $< $(LDFLAGS) -lm -o $@

check: remap_keyboard_ids_test
	./remap_keyboard_ids_test

clean:
	rm -f remap_keyboard_ids_test remap_keyboard_ids_test.exe

.PHONY: all check clean
