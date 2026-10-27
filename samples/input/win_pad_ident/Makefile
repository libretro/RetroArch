TARGET := win_pad_ident_test

LIBRETRO_COMM_DIR := ../../../libretro-common
INPUT_DIR         := ../../../input

# Which of Windows' views reads a controller, so it is one port
# (common/win_pad_ident.h).
SOURCES := win_pad_ident_test.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -std=gnu89 -g1 -O1 \
          -I$(LIBRETRO_COMM_DIR)/include -I$(INPUT_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

win_pad_ident_test.o: $(INPUT_DIR)/common/win_pad_ident.h $(INPUT_DIR)/common/input_ignore_devices.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
