# input/input_autodetect_builtin.c's profiles for the SDL2 and SDL3
# joypad drivers, which a pad with no profile file gets; built again
# for webOS, which adds profiles of its own.
CC     ?= cc
ROOT   := ../../..
TARGET := builtin_autoconf_test$(EXE)
WEBOS_TARGET := builtin_autoconf_webos_test$(EXE)
SOURCES := builtin_autoconf_test.c $(ROOT)/input/input_autodetect_builtin.c
CFLAGS += -std=c89 -Wall -Wextra -g1 -O1 \
   -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/input/include \
   -DHAVE_SDL2 -DHAVE_SDL3

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(WEBOS_TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

$(WEBOS_TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -DWEBOS $(SOURCES) -o $@ $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)
	$(RUNNER) ./$(WEBOS_TARGET)

clean:
	rm -f builtin_autoconf_test builtin_autoconf_test.exe
	rm -f builtin_autoconf_webos_test builtin_autoconf_webos_test.exe

.PHONY: all check clean
