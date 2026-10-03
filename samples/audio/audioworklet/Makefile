TARGET := audioworklet_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The driver is #included by the test and built against the tree's
# compile-only emscripten stand-ins.
INCDIRS := -I../../../tools/platform_stubs/emscripten -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -D_GNU_SOURCE
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := audioworklet_test.o $(LIBRETRO_COMM_DIR)/queues/retro_spsc.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# A callback only EM_ASM refers to is unused once the stand-in swallows
# the JavaScript.
audioworklet_test.o: audioworklet_test.c ../../../audio/drivers/audioworklet.c
	$(CC) $(CFLAGS) -Wno-unused-function -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
