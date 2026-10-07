TARGET := image_tile_gx_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# image_texture.c is here for image_texture_tile_gx() alone; its loaders
# are dropped at the link rather than chased through the transfer code.
SOURCES := image_tile_gx_test.c \
           $(LIBRETRO_COMM_DIR)/formats/image_texture.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 -ffunction-sections \
           -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -Wl,--gc-sections

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean
