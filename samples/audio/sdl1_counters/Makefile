# The SDL 1.2 audio driver's frames_consumed()/underruns() accounting,
# and the ordering its setup needs before the device thread runs.
# Builds audio/drivers/sdl1_audio.c itself.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := sdl1_counters_test
SOURCES_C := sdl1_counters_test.c \
	../latency_contract/stubs_retroarch.c \
	$(CORE_DIR)/audio/audio_upmix.c \
	$(CORE_DIR)/audio/drivers/sdl1_audio.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c
OBJS      := $(SOURCES_C:.c=.o)

# SDL 1.2 ships sdl-config; pkg-config carries it on the distributions
# that have retired sdl-config.
SDL_CFLAGS := $(shell pkg-config --cflags sdl 2>/dev/null || sdl-config --cflags 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl 2>/dev/null || sdl-config --libs 2>/dev/null)

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS   += -Wall $(INCDIRS) $(SDL_CFLAGS)
CFLAGS   += -DRARCH_INTERNAL -DHAVE_SDL -DHAVE_THREADS -D_GNU_SOURCE
LDFLAGS  += $(SDL_LIBS) -lpthread -lm

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
else
   CFLAGS += -g -O1
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -std=gnu99 -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
