TARGET := dingux_rumble_test

# The real Dingux joypad driver, given a stand-in /dev/input and SDL:
# its vibration motor is found on the node that plays a sine and
# written from the driver's own thread. Built with threads and without.
#    make && make check
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

CFLAGS += -Wall -Wno-unused -std=gnu99 -g1 -O1 \
          -DRARCH_INTERNAL -DDINGUX -DHAVE_SDL_DINGUX \
          -I. -I$(REPO_ROOT) -I$(REPO_ROOT)/input/include \
          -I$(REPO_ROOT)/gfx/include \
          -I$(LRC)/include -I$(REPO_ROOT)/deps

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

COMMON := $(REPO_ROOT)/input/common/output_writer.c

# The motor is evdev's, so Linux's: elsewhere there is nothing to
# test, and the targets say so and succeed.
ifeq ($(shell uname -s 2>/dev/null),Linux)
all: $(TARGET) $(TARGET)_nothreads

$(TARGET): dingux_rumble_test.c $(REPO_ROOT)/input/drivers_joypad/sdl_dingux_joypad.c $(REPO_ROOT)/input/common/linux_ff.h
	$(CC) $(CFLAGS) -DHAVE_THREADS $(LDFLAGS) -o $@ dingux_rumble_test.c \
		$(COMMON) $(LRC)/rthreads/rthreads.c -lpthread

$(TARGET)_nothreads: dingux_rumble_test.c $(REPO_ROOT)/input/drivers_joypad/sdl_dingux_joypad.c $(REPO_ROOT)/input/common/linux_ff.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ dingux_rumble_test.c $(COMMON)

check: all
	./$(TARGET)
	./$(TARGET)_nothreads
else
all check:
	@echo "SKIP dingux_rumble_test: the Dingux motor is Linux's"
endif

clean:
	rm -f $(TARGET) $(TARGET)_nothreads

.PHONY: all check clean
