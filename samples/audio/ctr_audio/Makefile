TARGET := ctr_audio_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The stand-in <3ds.h> is found here, ahead of any other.
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_CONFIGFILE -DHAVE_MENU -D_GNU_SOURCE
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := ctr_audio_test.o ctr_mock.o ctr_csnd_audio.o ctr_dsp_audio.o stubs_retroarch.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# The driver hands CSND 32-bit physical addresses back as pointers;
# on a 64-bit host that cast is the mock's business, not a defect.
ctr_csnd_audio.o: ../../../audio/drivers/ctr_csnd_audio.c
	$(CC) $(CFLAGS) -Wno-int-to-pointer-cast -c -o $@ $<

ctr_dsp_audio.o: ../../../audio/drivers/ctr_dsp_audio.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
