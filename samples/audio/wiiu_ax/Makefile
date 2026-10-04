TARGET := wiiu_ax_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The Wii U headers are the tree's own (wiiu/include); the functions
# behind them are the test's.
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../wiiu \
	-I../../../wiiu/include

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS \
	-D_GNU_SOURCE
LDFLAGS += -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := wiiu_ax_test.o rthreads.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# The test includes the driver, to read its counters.
wiiu_ax_test.o: wiiu_ax_test.c ../../../audio/drivers/wiiu_audio.c
	$(CC) $(CFLAGS) -c -o $@ $<

rthreads.o: $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
