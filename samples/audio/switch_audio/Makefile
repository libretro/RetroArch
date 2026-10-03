TARGET := switch_audio_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The stand-in <switch.h> is found here, ahead of any other.
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_CONFIGFILE -DHAVE_MENU -D_GNU_SOURCE
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := switch_audio_test.o audout_mock.o stubs_retroarch.o switch_audio.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# The libnx build of the driver.
switch_audio.o: ../../../audio/drivers/switch_audio.c
	$(CC) $(CFLAGS) -DSWITCH -DHAVE_LIBNX -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
