TARGET := psp_ring_test

LIBRETRO_COMM_DIR := ../../../libretro-common
STUBS             := ../../../tools/platform_stubs
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS \
           -DHAVE_CONFIGFILE -DHAVE_MENU -D_GNU_SOURCE
LDFLAGS += -lm -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# Each driver is built for its own platform, against that platform's
# compile-only SDK stubs; device_mock.c supplies the bodies.
OBJS := psp_ring_test.o device_mock.o psp_audio.o psp2_audio.o ps4_audio.o \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.o \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# The drivers take their sthread_create from device_mock, which can be
# told to refuse one; this unit is compiled without the rename, so the
# real one is still there to delegate to.
DRVFLAGS := -Dsthread_create=mock_sthread_create

psp_audio.o: ../../../audio/drivers/psp_audio.c
	$(CC) $(CFLAGS) $(DRVFLAGS) -I$(STUBS)/psp -DPSP -c -o $@ $<

psp2_audio.o: ../../../audio/drivers/psp2_audio.c
	$(CC) $(CFLAGS) $(DRVFLAGS) -I$(STUBS)/vita -DVITA -c -o $@ $<

ps4_audio.o: ../../../audio/drivers/ps4_audio.c
	$(CC) $(CFLAGS) $(DRVFLAGS) -I$(STUBS)/orbis -DORBIS -c -o $@ $<

device_mock.o: device_mock.c
	$(CC) $(CFLAGS) -I$(STUBS)/vita -I$(STUBS)/orbis -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
