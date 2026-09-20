# The SDL3 audio driver's underruns() accounting, and the callback it
# has to be counted in. Builds audio/drivers/sdl3_audio.c itself.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := sdl3_counters_test
SOURCES_C := sdl3_counters_test.c \
	../latency_contract/stubs_retroarch.c \
	$(CORE_DIR)/audio/audio_upmix.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c
OBJS      := $(SOURCES_C:.c=.o)

SDL_CFLAGS ?= $(shell pkg-config --cflags sdl3)
SDL_LIBS   ?= $(shell pkg-config --libs sdl3)

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) $(SDL_CFLAGS)
CFLAGS   += -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_SDL3 -DHAVE_MICROPHONE -D_GNU_SOURCE -std=gnu99
LDFLAGS  += $(SDL_LIBS) -lm

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
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
