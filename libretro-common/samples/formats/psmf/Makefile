TARGET := rpsmf_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rpsmf_test.c \
	$(LIBRETRO_COMM_DIR)/formats/psmf/rpsmf.c \
	$(LIBRETRO_COMM_DIR)/formats/mpeg1/rmpeg1_ps.c

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
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
