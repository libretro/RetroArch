TARGET := rpng

SOURCES := $(wildcard *.c)
OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -O0 -g -DHAVE_ZLIB -DHAVE_ZLIB_DEFLATE

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -lz -lImlib2

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

