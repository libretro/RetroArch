TARGET := pipewire_write_test

LIBRETRO_COMM_DIR := ../../../libretro-common
PW_CFLAGS := $(shell pkg-config --cflags libpipewire-0.3)
INCDIRS := $(PW_CFLAGS) -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_PIPEWIRE -DHAVE_MICROPHONE -DHAVE_THREADS -D_GNU_SOURCE
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := pipewire_write_test.o $(LIBRETRO_COMM_DIR)/lists/string_list.o $(LIBRETRO_COMM_DIR)/compat/compat_strl.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# The driver is #included by the test; its stand-ins for libpipewire
# live there too, so nothing links against the library.
pipewire_write_test.o: pipewire_write_test.c ../../../audio/drivers/pipewire.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
