TARGET := tpool_handoff_test

LIBRETRO_COMM_DIR := ../../..

# tpool.c depends on rthreads.c and retro_eventcount.c.  All three are
# self-contained at the libretro-common layer (rthreads.c is the
# platform abstraction over pthreads / Win32 / etc.) so we just compile
# them in directly -- no other libretro-common sources are needed.
SOURCES := \
	tpool_handoff_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -pedantic -std=gnu99 -g -O0 -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread

# rthreads.c uses clock_gettime + CLOCK_REALTIME on Linux glibc; on
# older glibc those live in -lrt.  Harmless on newer glibc.
# Only glibc has it; MinGW has no librt and the link fails on it.
ifeq ($(OS),Windows_NT)
else
LDFLAGS += -lrt
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

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

# The test includes tpool.c, to build it with its test yields on.
tpool_handoff_test.o: $(LIBRETRO_COMM_DIR)/rthreads/tpool.c
