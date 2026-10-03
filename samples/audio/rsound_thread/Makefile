# librsound's playback thread: ending on its own, stopped by the
# frontend, and both at once. Includes librsound for its statics.

TARGET   := rsound_thread_test
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common

SOURCES := rsound_thread_test.c \
           $(LRC)/rthreads/rthreads.c \
           $(LRC)/compat/compat_strl.c
CFLAGS  += -Wall -std=gnu99 -g -O1 -D_GNU_SOURCE -DHAVE_THREADS \
           -I$(CORE_DIR) -I$(LRC)/include
LDFLAGS += -lpthread
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
