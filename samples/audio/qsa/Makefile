TARGET := qsa_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The mock <sys/asoundlib.h> is found here, ahead of any system one.
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

SOURCES_C := \
	qsa_test.c \
	qsa_mock.c \
	stubs_retroarch.c \
	../../../audio/drivers/alsa_qsa.c

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_ALSA -DHAVE_CONFIGFILE -DHAVE_MENU -D_GNU_SOURCE
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
