TARGET := rjpeg_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rjpeg_test.c \
	$(LIBRETRO_COMM_DIR)/formats/jpeg/rjpeg.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
