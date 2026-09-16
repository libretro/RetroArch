CC = gcc
ROOT = ../../..
CPPFLAGS += -I$(ROOT)/libretro-common/include
CFLAGS += -O2 -std=c89 -Wall -Wextra -Werror
LDLIBS += -lm
WRAP = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: speed_lpf_test
speed_lpf_test: speed_lpf_test.c $(ROOT)/audio/audio_speed_lpf.c $(ROOT)/audio/audio_speed_lpf.h
	$(CC) $(CPPFLAGS) $(CFLAGS) speed_lpf_test.c $(ROOT)/audio/audio_speed_lpf.c $(LDFLAGS) $(WRAP) $(LDLIBS) -o $@
check: speed_lpf_test
	./speed_lpf_test
clean:
	rm -f speed_lpf_test speed_lpf_test.exe
.PHONY: all check clean
