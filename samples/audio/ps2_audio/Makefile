# audio/drivers/ps2_audio.c against a scripted audsrv and EE kernel
# (kernel.h, audsrv.h and ps2_mock.c here); see ps2_audio_test.c.

TARGET := ps2_audio_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The stand-in <kernel.h> and <audsrv.h> are found here, ahead of any other.
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -D_GNU_SOURCE
LDFLAGS += -lm -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

SOURCES := ps2_audio_test.c ps2_mock.c stubs_retroarch.c ../../../audio/drivers/ps2_audio.c

all: $(TARGET)

$(TARGET): $(SOURCES) kernel.h audsrv.h ps2_mock.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
