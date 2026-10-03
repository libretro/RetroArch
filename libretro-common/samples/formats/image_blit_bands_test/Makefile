TARGET := image_blit_bands_test
LIBRETRO_COMM_DIR := ../../..
SOURCES := image_blit_bands_test.c \
	$(LIBRETRO_COMM_DIR)/formats/image/image_blit_bands.c \
	$(LIBRETRO_COMM_DIR)/rthreads/tpool.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c
CFLAGS  += -Wall -W -std=gnu99 -g -O1 -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
# malloc/free are wrapped so the test can count heap traffic per blit.
LDFLAGS += -lpthread -lrt -Wl,--wrap=malloc,--wrap=free
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
