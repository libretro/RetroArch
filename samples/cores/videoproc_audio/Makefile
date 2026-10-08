# The video processor core's audio capture handle between the
# frontend's audio thread and the main thread. Includes the core for
# its statics; links the real libv4l2 and libasound, with the capture
# read and the close wrapped.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := videoproc_audio_test

SOURCES_C := \
	videoproc_audio_test.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

OBJS      := $(SOURCES_C:.c=.o)

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include
CFLAGS   += -Wall $(INCDIRS) -DHAVE_ALSA -D_GNU_SOURCE
LDFLAGS  += -Wl,--wrap=snd_pcm_open -Wl,--wrap=snd_pcm_readi -Wl,--wrap=snd_pcm_close \
            -Wl,--wrap=pthread_mutex_lock -lv4l2 -lasound -lm -lpthread

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
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

check: $(TARGET)
	./$(TARGET)
