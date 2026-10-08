TARGET := switch_audren_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The stand-in <switch.h> is found here, ahead of any other.
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS -D_GNU_SOURCE
LDFLAGS += -lpthread -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := switch_audren_test.o audren_mock.o stubs_retroarch.o \
        switch_libnx_audren_audio.o rthreads.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# The libnx build of the driver.
switch_libnx_audren_audio.o: ../../../audio/drivers/switch_libnx_audren_audio.c
	$(CC) $(CFLAGS) -DSWITCH -DHAVE_LIBNX -c -o $@ $<

rthreads.o: $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
