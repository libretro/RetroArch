TARGET := crc32_test

LIBRETRO_COMM_DIR := ../../..

# The concurrency check needs real threads.  Without HAVE_THREADS the
# other five parts still run, which is what the console targets that
# lack rthreads would exercise anyway.
HAVE_THREADS ?= 1

# encoding_crc32.c asks cpu_features_get() whether the CPU has
# PCLMULQDQ.  features_cpu.c needs no VFS, only compat_strl.c.
SOURCES := \
	crc32_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c

CFLAGS  += -Wall -pedantic -std=gnu99 -g -O2 -I$(LIBRETRO_COMM_DIR)/include

ifeq ($(HAVE_THREADS),1)
   CFLAGS  += -DHAVE_THREADS
   SOURCES += $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c
   LDFLAGS += -lpthread
   # rthreads.c uses clock_gettime + CLOCK_REALTIME on Linux glibc; on
   # older glibc those live in -lrt.  Harmless on newer glibc.
   LDFLAGS += -lrt
endif

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

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
