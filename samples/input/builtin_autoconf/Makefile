# input/input_autodetect_builtin.c's profiles, which a pad with no
# profile file gets: the SDL2 and SDL3 joypad drivers' in every build,
# and one build more for each platform below with the -D that adds its
# own.
CC     ?= cc
ROOT   := ../../..
SOURCES := builtin_autoconf_test.c $(ROOT)/input/input_autodetect_builtin.c
CFLAGS += -std=c89 -Wall -Wextra -g1 -O1 \
   -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/input/include \
   -DHAVE_SDL2 -DHAVE_SDL3

PLATFORMS  := webos
DEFS_webos := -DWEBOS

PLATFORMS    += android
DEFS_android := -DANDROID

DEFS_ps3  := -D__PS3__
DEFS_ps4  := -DORBIS

DEFS_gamecube := -DGEKKO
DEFS_wii      := -DGEKKO -DHW_RVL

DEFS_qnx  := -D__QNX__ -isystem $(ROOT)/tools/qnx_standin

TARGETS := builtin_autoconf_test$(EXE) \
   $(PLATFORMS:%=builtin_autoconf_%_test$(EXE))

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGETS)

builtin_autoconf_test$(EXE): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

builtin_autoconf_%_test$(EXE): $(SOURCES)
	$(CC) $(CFLAGS) $(DEFS_$*) $(SOURCES) -o $@ $(LDFLAGS)

check: all
	set -e; for t in $(TARGETS); do echo $$t; $(RUNNER) ./$$t; done

clean:
	rm -f builtin_autoconf_*test builtin_autoconf_*test.exe

.PHONY: all check clean
