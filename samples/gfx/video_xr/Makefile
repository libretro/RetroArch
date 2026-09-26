TARGET := video_xr_test

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

# gfx/video_xr.c is pure maths over gfx/video_views.c, so both build alone.
SOURCES := \
	video_xr_test.c \
	$(RARCH_DIR)/gfx/video_xr.c \
	$(RARCH_DIR)/gfx/video_views.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=c89 -pedantic -Werror=declaration-after-statement -g -O1 \
           -I$(LIBRETRO_COMM_DIR)/include -I$(RARCH_DIR)
LDFLAGS += -lm

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
