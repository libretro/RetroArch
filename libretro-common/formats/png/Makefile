TARGET := rpng
HAVE_IMLIB2=1

LDFLAGS +=  -lz

ifeq ($(HAVE_IMLIB2),1)
CFLAGS += -DHAVE_IMLIB2
LDFLAGS += -lImlib2
endif

SOURCES_C := 	rpng.c \
					rpng_encode.c \
					rpng_test.c \
					../../compat/compat.c \
					../../file/nbio/nbio_stdio.c \
					../../file/file_extract.c \
					../../file/file_path.c \
					../../file/retro_file.c \
					../../string/string_list.c

OBJS := $(SOURCES_C:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -O0 -g -DHAVE_ZLIB -DHAVE_ZLIB_DEFLATE -DRPNG_TEST -I../../include

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

