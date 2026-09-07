# Latency contract: real drivers on their null backends, each asked for
# a latency setting and checked against what the interface promises of
# buffer_size() and write_avail().

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := latency_contract_test

SOURCES_C := \
	latency_contract_test.c \
	stubs_retroarch.c \
	$(CORE_DIR)/audio/drivers/alsa.c \
	$(CORE_DIR)/audio/common/alsa.c \
	$(CORE_DIR)/audio/drivers/sdl_audio.c \
	$(CORE_DIR)/audio/drivers/openal.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/queues/fifo_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

OBJS      := $(SOURCES_C:.c=.o)

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -I$(CORE_DIR)/deps
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)

CFLAGS  += -Wall $(INCDIRS) $(SDL_CFLAGS)
CFLAGS  += -DRARCH_INTERNAL -DHAVE_ALSA -DHAVE_SDL2 -DHAVE_AL -DHAVE_THREADS -D_GNU_SOURCE
LDFLAGS += -lasound -lopenal $(SDL_LIBS) -lm -lpthread

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
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
