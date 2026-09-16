# ASIO ring sizing: the floor against the devices that set it.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := asio_ring_test
SOURCES_C := asio_ring_test.c
OBJS      := $(SOURCES_C:.c=.o)

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS   += -Wall -Wno-variadic-macros $(INCDIRS) -D_GNU_SOURCE

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
else
CFLAGS  += -g -O1
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -std=c89 -pedantic -Wdeclaration-after-statement -Wno-long-long -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
