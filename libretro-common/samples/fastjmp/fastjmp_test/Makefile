TARGET := fastjmp_test
LIBRETRO_COMM_DIR := ../../..
SOURCES := fastjmp_test.c $(LIBRETRO_COMM_DIR)/fastjmp/fastjmp.c
CFLAGS  += -Wall -W -std=gnu99 -g -O2 -I$(LIBRETRO_COMM_DIR)/include
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
