# Menu Item Visibility: each "Show '...'" setting names its entry as the
# Main Menu shows it.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

EXE       :=
TARGET    := visibility_labels_test$(EXE)
SOURCES_C := visibility_labels_test.c
OBJS      := $(SOURCES_C:.c=.o)

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS   += -Wall $(INCDIRS)

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1
LDFLAGS += -fsanitize=$(SANITIZER)
else
CFLAGS  += -g1 -O1
endif

all: $(TARGET)

visibility_labels_test.o: $(CORE_DIR)/intl/msg_hash_us.h

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -std=gnu89 -pedantic -D_GNU_SOURCE -Wdeclaration-after-statement -Wno-long-long -Wno-overlength-strings -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
