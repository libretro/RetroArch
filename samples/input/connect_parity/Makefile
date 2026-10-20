# The converted pad handlers of input/connect against the copying
# handlers they replace.
ROOT   := ../../..
TARGET := connect_parity_test
CC     ?= cc
CFLAGS += -std=gnu89 -O1 -g1 -Wall -DHAVE_HID \
          -I$(ROOT) -I$(ROOT)/libretro-common/include
SOURCES := connect_parity_test.c \
           $(ROOT)/input/connect/connect_ps4.c
ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: all
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean
