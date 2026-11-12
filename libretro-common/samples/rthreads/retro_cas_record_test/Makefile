# rthreads/retro_cas_record.h: a record any thread reads and any thread changes,
# with no lock.

TARGET    := retro_cas_record_test
CORE_DIR  := ../../..

SOURCES := retro_cas_record_test.c
CFLAGS  += -Wall -std=gnu99 -g1 -O1 -I$(CORE_DIR)/include
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
