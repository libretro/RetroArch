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

all: cheat_user_path_test$(EXE)

cheat_user_path_test$(EXE): cheat_user_path_test.c $(ROOT)/configuration.c $(ROOT)/configuration.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fwhole-program $< $(LDFLAGS) -lm -o $@

check: cheat_user_path_test$(EXE)
	$(RUNNER) ./cheat_user_path_test$(EXE)

clean:
	rm -f cheat_user_path_test cheat_user_path_test.exe cheat_user_path_test.cfg \
	      cheat_user_path_override.cfg

.PHONY: all check clean
