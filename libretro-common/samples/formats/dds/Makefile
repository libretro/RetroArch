TARGET := rdds_block_test
LIBRETRO_COMM_DIR := ../../..
SOURCES := rdds_block_test.c $(LIBRETRO_COMM_DIR)/formats/dds/rdds.c
CFLAGS  += -Wall -W -std=gnu99 -g -O1 -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lm
OBJS := $(SOURCES:.c=.o)
ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
all: $(TARGET)
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
check: all
	./$(TARGET)
clean:
	rm -f $(OBJS) $(TARGET)
.PHONY: all check clean
