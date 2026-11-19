# The OpenAL driver against a scripted library offering Apple's source
# notification and no AL_SOFT_events; see openal_notify_test.c.

TARGET := openal_notify_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The stub AL headers first: the scripted library implements them.
INCDIRS := -I../../../tools/platform_stubs/openal -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_AL -DHAVE_THREADS -DHAVE_CONFIGFILE -D_GNU_SOURCE
LDFLAGS += -lm -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

SOURCES := openal_notify_test.c stubs_retroarch.c \
	../../../audio/audio_upmix.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

all: $(TARGET)

$(TARGET): $(SOURCES) ../../../audio/drivers/openal.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
