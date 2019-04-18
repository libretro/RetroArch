TARGET := nbio_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	nbio_test.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_intf.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_linux.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_unixmmap.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_windowsmmap.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_stdio.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
