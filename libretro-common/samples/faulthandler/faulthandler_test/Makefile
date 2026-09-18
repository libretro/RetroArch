TARGET := faulthandler_test
LIBRETRO_COMM_DIR := ../../..
SOURCES := faulthandler_test.c \
	$(LIBRETRO_COMM_DIR)/faulthandler/faulthandler.c \
	$(LIBRETRO_COMM_DIR)/memmap/memmap.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c
CFLAGS  += -Wall -W -std=gnu99 -g -O1 -I$(LIBRETRO_COMM_DIR)/include -DHAVE_THREADS
LDFLAGS += -lpthread -lrt
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
