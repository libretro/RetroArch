TARGET := win32_screensaver_test
GEN    := win32_screensaver_sim.c

LIBRETRO_COMM_DIR := ../../../libretro-common
COMMON_SRC        ?= ../../../gfx/common/win32_common.c

# win32_suspend_screensaver() as it ships, lifted out of win32_common.c
# and compiled against a fake kernel32 instead of <windows.h>.
SOURCES := win32_screensaver_test.c $(GEN)

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -Wno-unused-parameter -std=c89 -pedantic -g -O1 \
          -I. -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(GEN): $(COMMON_SRC)
	{ echo '#include "fake_kernel32.h"'; \
	  sed -n '/^bool win32_suspend_screensaver(void \*data, bool enable)$$/,/^}$$/p' $<; } > $@
	grep -q 'win32_suspend_screensaver' $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): fake_kernel32.h

check: all
	./$(TARGET) power
	./$(TARGET) thread_state
	./$(TARGET) none

clean:
	rm -f $(OBJS) $(TARGET) $(GEN)

.PHONY: all check clean
