# queues/retro_triple_buffer.h: a frame handoff between a producer and
# a consumer that never wait for each other.

TARGET    := retro_triple_buffer_test
CORE_DIR  := ../../..

SOURCES := retro_triple_buffer_test.c
CFLAGS  += -Wall -std=gnu99 -g -O1 -I$(CORE_DIR)/include
LDFLAGS += -lpthread
OBJS    := $(SOURCES:.c=.o)
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(OBJS)
.PHONY: all check clean
