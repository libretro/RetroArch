TARGET := vblank_clock_test

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

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
