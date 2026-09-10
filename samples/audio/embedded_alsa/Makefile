TARGET  := embedded_alsa_test
TARGET2 := abi_check

LIBRETRO_COMM_DIR := ../../../libretro-common
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

SOURCES_C := \
	embedded_alsa_test.c \
	stubs_retroarch.c \
	../../../audio/audio_upmix.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_TINYALSA -DHAVE_CONFIGFILE -DHAVE_MENU -D_GNU_SOURCE
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET) $(TARGET2)

ealsa_abi.h: ../../../audio/drivers/alsa.c extract_abi.sh
	./extract_abi.sh

$(TARGET2): abi_check.c ealsa_abi.h
	$(CC) $(CFLAGS) -o $@ abi_check.c

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: all
	./$(TARGET2)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET2) ealsa_abi.h

.PHONY: all check clean
