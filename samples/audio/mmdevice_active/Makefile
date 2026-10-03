# The open-endpoint table the WASAPI notifications consult.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := mmdevice_active_test
SOURCES_C := mmdevice_active_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c
OBJS      := $(SOURCES_C:.c=.o)

CFLAGS   += -Wall -I$(LIBRETRO_COMM_DIR)/include -DHAVE_THREADS -D_GNU_SOURCE

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
else
CFLAGS  += -g -O1
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -lpthread -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# The table under ThreadSanitizer, then ASan/UBSan.
check:
	$(MAKE) -s clean
	$(MAKE) -s SANITIZER=thread
	./$(TARGET)
	$(MAKE) -s clean
	$(MAKE) -s SANITIZER=address,undefined
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
