# RETRO_ENVIRONMENT_SET_ROTATION before the video driver exists.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := set_rotation_deferred_test
SOURCES_C := set_rotation_deferred_test.c
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

set_rotation_driver.h: $(CORE_DIR)/gfx/video_driver.c extract_set_rotation.awk
	awk -f extract_set_rotation.awk $(CORE_DIR)/gfx/video_driver.c > $@.tmp && mv $@.tmp $@

set_rotation_deferred_test.o: set_rotation_driver.h

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -std=c89 -pedantic -Werror=declaration-after-statement -Wno-long-long -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) set_rotation_driver.h set_rotation_driver.h.tmp

.PHONY: all check clean
