TARGET := ps3_ring_test

LIBRETRO_COMM_DIR := ../../../libretro-common
STUBS             := ../../../tools/platform_stubs/ps3
INCDIRS := -I. -I$(STUBS) -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS \
           -D__PSL1GHT__ -DHAVE_CONFIGFILE -DHAVE_MENU -D_GNU_SOURCE
LDFLAGS += -lm -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# audio_upmix.c for audio_layout_channels(), which the driver uses to
# size the port; retro_spsc.c is the ring itself.
OBJS := ps3_ring_test.o device_mock.o ps3_audio.o \
	../../../audio/audio_upmix.o \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.o \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

ps3_audio.o: ../../../audio/drivers/ps3_audio.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
