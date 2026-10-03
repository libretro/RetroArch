# The FM synth's MIDI event queue: writers and the audio thread at
# once, overflow, and where drops are reported. Includes the driver.

TARGET   := fmsynth_queue_test
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common

SOURCES := fmsynth_queue_test.c \
           $(LRC)/rthreads/rthreads.c
CFLAGS  += -Wall -std=gnu99 -g -O1 -D_GNU_SOURCE -DHAVE_THREADS \
           -I$(CORE_DIR) -I$(CORE_DIR)/gfx -I$(LRC)/include
LDFLAGS += -lpthread -lm
OBJS    := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(OBJS)
.PHONY: all check clean
