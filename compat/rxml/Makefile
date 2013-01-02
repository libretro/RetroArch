TARGET := rxml

SOURCES := $(wildcard *.c)
OBJS := $(SOURCES:.c=.o)

CFLAGS += -DRXML_TEST -Wall -pedantic -std=gnu99 -O0 -g

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

