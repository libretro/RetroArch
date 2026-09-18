TARGET := vblank_clock_test
PUB     := vblank_clock_pub_test

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

# gfx/common/vblank_clock.h is header-only arithmetic; no platform code.
SOURCES := vblank_clock_test.c

CFLAGS  += -Wall -std=c89 -pedantic -Wno-long-long -g -O1 \
           -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS)
endif

all: $(TARGET) $(PUB)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

# The seqlock between two threads: gnu99 for pthreads, run under
# SANITIZER=thread in CI.
$(PUB): vblank_clock_pub_test.c $(RARCH_DIR)/gfx/common/vblank_clock.h
	$(CC) $(subst -std=c89 -pedantic,-std=gnu99,$(CFLAGS)) -o $@ vblank_clock_pub_test.c $(LDFLAGS) -lpthread

check: all
	./$(TARGET)
	./$(PUB)

clean:
	rm -f $(TARGET) $(PUB)

.PHONY: all check clean
