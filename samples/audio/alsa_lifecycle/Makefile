compiler     := gcc
TARGET       := alsa_lifecycle_test

CORE_DIR = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

CC      := $(compiler)
flags   += -std=gnu99

# The shipping driver and its shared PCM setup against the null PCM;
# the lifecycle entry points - can_pause, pause, drop, prepare - and
# the write loop's wait and writei are scripted via --wrap, all else
# reaches the real library.
SOURCES_C := \
	alsa_lifecycle_test.c \
	../../../audio/audio_upmix.c \
	../../../audio/audio_binaural.c \
	stubs_retroarch.c \
	$(CORE_DIR)/audio/drivers/alsa.c \
	$(CORE_DIR)/audio/common/alsa.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/queues/fifo_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS += -Wall $(INCDIRS)
CFLAGS += -DHAVE_ALSA -DHAVE_THREADS -D_GNU_SOURCE
LDFLAGS += -lasound -lm -lpthread
LDFLAGS += -Wl,--wrap=snd_pcm_hw_params_can_pause \
           -Wl,--wrap=snd_pcm_pause \
           -Wl,--wrap=snd_pcm_drop \
           -Wl,--wrap=snd_pcm_prepare \
           -Wl,--wrap=snd_pcm_writei \
           -Wl,--wrap=snd_pcm_wait \
           -Wl,--wrap=snd_pcm_recover

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

%.o: %.c
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -c -o$@ $<

$(TARGET): $(OBJS)
	$(CC) -o$@ $(OBJS) $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean all check
