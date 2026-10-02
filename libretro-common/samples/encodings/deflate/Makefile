TARGET := rinflate_window_test

LIBRETRO_COMM_DIR := ../../..

# encoding_deflate.c folds CRC-32 through encoding_crc32.c, which asks
# cpu_features_get() for PCLMULQDQ; features_cpu.c needs compat_strl.c.
SOURCES := \
	rinflate_window_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c

CFLAGS  += -Wall -pedantic -std=gnu99 -g -O2 -I$(LIBRETRO_COMM_DIR)/include

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean
