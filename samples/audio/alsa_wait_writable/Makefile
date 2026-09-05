compiler     := gcc
TARGET       := alsa_wait_writable_test

CORE_DIR = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

CC      := $(compiler)
flags   += -std=gnu99

# The shipping driver, its shared PCM setup, and the libretro-common
# pieces they lean on. The harness scripts the five libasound calls
# alsa_wait_writable() makes via --wrap; everything else reaches the
# real library.
SOURCES_C := \
	alsa_wait_writable_test.c \
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
LDFLAGS += -Wl,--wrap=snd_pcm_avail \
           -Wl,--wrap=snd_pcm_state \
           -Wl,--wrap=snd_pcm_start \
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
