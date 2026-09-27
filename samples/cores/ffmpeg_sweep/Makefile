# The ffmpeg core across the media it has to take and the abuse a
# as a libretro host: steady state, seeks both ways, teardown.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common
FF_CORE_DIR       := $(CORE_DIR)/cores/libretro-ffmpeg

TARGET    := ffmpeg_sweep_test

SOURCES_C := \
	ffmpeg_sweep_test.c \
	$(FF_CORE_DIR)/ffmpeg_core.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/tpool.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

OBJS      := $(SOURCES_C:.c=.o)

FF_CFLAGS := $(shell pkg-config --cflags libavcodec libavformat libavutil libswscale libswresample 2>/dev/null)
FF_LIBS   := $(shell pkg-config --libs   libavcodec libavformat libavutil libswscale libswresample 2>/dev/null)

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include -I$(LIBRETRO_COMM_DIR)/include/compat -I$(FF_CORE_DIR)
CFLAGS   += -Wall $(INCDIRS) $(FF_CFLAGS)
CFLAGS   += -DHAVE_THREADS -DHAVE_SWRESAMPLE -D_GNU_SOURCE
LDFLAGS  += $(FF_LIBS) -lm -lpthread

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
else
CFLAGS  += -g -O1
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -std=gnu99 -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS) ffmpeg_sweep_test.mkv

.PHONY: clean
