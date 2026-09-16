TARGET := overlay_alpha_flag_test

LIBRETRO_COMM_DIR := ../../../libretro-common

# Self-contained: the hazard protocol under test is reproduced in the
# harness (see the file header for why), so the only tree dependency
# is rthreads.c for portable threads and locks.  retro_atomic.h and
# retro_miscellaneous.h are header-only.
SOURCES := \
	overlay_alpha_flag_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lrt

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
