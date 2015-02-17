TARGET := nbio_test

SOURCES := $(wildcard *.c)
OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -O0 -g -I../../include

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

