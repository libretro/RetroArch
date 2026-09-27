CC = gcc
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl3)
SDL_LIBS ?= $(shell pkg-config --libs sdl3)
CFLAGS += -O2 -std=gnu99 -Wall -fwhole-program -ffunction-sections -fdata-sections
CPPFLAGS += -DRARCH_INTERNAL -I../../.. -I../../../libretro-common/include -I../../../deps $(SDL_CFLAGS)
LDFLAGS += -Wl,--gc-sections
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
endif
sdl3_raw_test: sdl3_raw_test.c ../../../audio/drivers/sdl3_audio.c ../../../audio/audio_driver.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) $(SDL_LIBS) -lm -o $@
check: sdl3_raw_test
	./sdl3_raw_test
clean:
	rm -f sdl3_raw_test sdl3_raw_test.exe
.PHONY: check clean
