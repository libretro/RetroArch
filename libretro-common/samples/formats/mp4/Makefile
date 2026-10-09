TARGET := rmp4_fragment_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rmp4_fragment_test.c \
	$(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c

CFLAGS  += -Wall -std=gnu99 -g -O1 -DHAVE_RMP4 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES:.c=.o)

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -lm

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean
