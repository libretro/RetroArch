TARGET := retro_atomic_wiiu_backend_test

LIBRETRO_COMM_DIR := ../../..

# RETRO_ATOMIC_FORCE_WIIU selects the backend off-target; coreinit_stub.c
# supplies the OS*Atomic exports it calls. For the word-sized size_t and
# pointer paths too, build for 32-bit PowerPC:
#   make CC=powerpc-linux-gnu-gcc LDFLAGS=-static check RUNNER=qemu-ppc-static
SOURCES := retro_atomic_wiiu_backend_test.c coreinit_stub.c

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O2 \
           -I$(LIBRETRO_COMM_DIR)/include \
           -DRETRO_ATOMIC_FORCE_WIIU
LDFLAGS += -lpthread

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean
