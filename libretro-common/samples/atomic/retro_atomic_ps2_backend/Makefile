TARGET := retro_atomic_ps2_backend_test

LIBRETRO_COMM_DIR := ../../..

# RETRO_ATOMIC_FORCE_PS2 selects the backend off-target; ps2stub/ then
# supplies the <kernel.h> it includes, so -Ips2stub has to come before
# any system include path that might carry another one.
SOURCES := retro_atomic_ps2_backend_test.c ps2stub/kernel_stub.c

CFLAGS  += -Wall -pedantic -std=gnu99 -g -O0 \
           -I$(LIBRETRO_COMM_DIR)/include -Ips2stub \
           -DRETRO_ATOMIC_FORCE_PS2

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

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
