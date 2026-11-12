# queues/retro_deque.h: a double-ended queue of one type in one block.
# Built as C89, pedantic, as the header promises.

TARGET    := retro_deque_test
CORE_DIR  := ../../..

SOURCES := retro_deque_test.c
CFLAGS  += -Wall -Wextra -std=c89 -pedantic -Wdeclaration-after-statement -g1 -O2 -I$(CORE_DIR)/include
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
