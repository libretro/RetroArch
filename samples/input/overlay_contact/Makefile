TARGET := overlay_contact_test

LIBRETRO_COMM_DIR := ../../../libretro-common
INPUT_DIR         := ../../../input

# A finger's contact on an overlay D-pad or face-button area
# (common/overlay_contact.h).
SOURCES := overlay_contact_test.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -std=gnu89 -g1 -O1 \
          -I$(LIBRETRO_COMM_DIR)/include -I$(INPUT_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -lm

overlay_contact_test.o: $(INPUT_DIR)/common/overlay_contact.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
