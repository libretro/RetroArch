# Display pacing: where the hold ends when the handover took time.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

EXE       :=
TARGET    := pace_hold_start_test$(EXE)
SOURCES_C := pace_hold_start_test.c
OBJS      := $(SOURCES_C:.c=.o)

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS   += -Wall $(INCDIRS)

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1
LDFLAGS += -fsanitize=$(SANITIZER)
else
CFLAGS  += -g1 -O1
endif

all: $(TARGET)

# The wait, taken from the wrapper.
pace_hold_start_driver.h: $(CORE_DIR)/gfx/video_thread_wrapper.c extract_hold.awk
	awk -f extract_hold.awk $(CORE_DIR)/gfx/video_thread_wrapper.c > $@.tmp && mv $@.tmp $@

pace_hold_start_test.o: pace_hold_start_driver.h

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -std=gnu89 -pedantic -D_GNU_SOURCE -Wdeclaration-after-statement -Wno-long-long -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) pace_hold_start_driver.h pace_hold_start_driver.h.tmp

.PHONY: all check clean
