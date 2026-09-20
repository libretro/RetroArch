# The SDL audio driver's producer-side waits, against SDL's own dummy
# device. Builds audio/drivers/sdl2_audio.c itself - the subject is its
# handshake, so a copy of the arithmetic would prove nothing.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := sdl_lost_wakeup_test
SOURCES_C := sdl_lost_wakeup_test.c \
	stubs_retroarch.c \
	$(CORE_DIR)/audio/audio_upmix.c \
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

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) $(shell pkg-config --cflags sdl2)
CFLAGS   += -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_SDL2 -DHAVE_MICROPHONE -D_GNU_SOURCE -std=gnu99
LDFLAGS  += $(shell pkg-config --libs sdl2) -lpthread -lm

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
	./$(TARGET) 2

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
