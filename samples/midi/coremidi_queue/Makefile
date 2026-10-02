# midi/drivers/coremidi_queue.h: the CoreMIDI driver's lock-free input
# queue, which builds and runs anywhere.

TARGET    := coremidi_queue_test
REPO_ROOT := ../../..

SOURCES := coremidi_queue_test.c
CFLAGS  += -Wall -std=gnu99 -g -O1 -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include
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
