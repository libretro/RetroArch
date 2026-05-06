TARGET := unbase64_test

CORE_DIR          := .
LIBRETRO_COMM_DIR := ../../..

SOURCES_C := \
	$(CORE_DIR)/unbase64_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_base64.c

OBJS := $(SOURCES_C:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -I$(LIBRETRO_COMM_DIR)/include

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
