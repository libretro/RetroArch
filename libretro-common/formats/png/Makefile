TARGET := rpng
HAVE_IMLIB2=1

LDFLAGS +=  -lz

ifeq ($(HAVE_IMLIB2),1)
CFLAGS += -DHAVE_IMLIB2
LDFLAGS += -lImlib2
endif

SOURCES := rpng.c rpng_nbio.c rpng_encode.c rpng_test.c ../../file/nbio/nbio_stdio.c
OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -O0 -g -DHAVE_ZLIB -DHAVE_ZLIB_DEFLATE -DRPNG_TEST -I../../include

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

