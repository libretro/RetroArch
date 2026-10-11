TARGET := ratrac3_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	ratrac3_test.c \
	$(LIBRETRO_COMM_DIR)/formats/atrac3/ratrac3.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=c89 -g -O1 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -lm

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
