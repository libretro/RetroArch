TARGET := retro_eventcount_bench

LIBRETRO_COMM_DIR := ../../..

# A benchmark, not a test: not picked up by the sample-discovery CI
# (no test in the name) and built -O2.  Needs two physical cores to
# mean anything.
SOURCES := \
	retro_eventcount_bench.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

# SCOND=1 builds against rthreads' scond instead of the platform's
# address-wait primitive, which is the comparison this bench exists
# for.  On Windows, scond's own tier is picked at runtime with
# RTHREADS_SCOND=alert|keyed, so one SCOND=1 binary covers both.
ifneq ($(SCOND),)
   CFLAGS += -DRETRO_EVENTCOUNT_FORCE_SCOND
   TARGET := retro_eventcount_bench_scond
endif

OBJS := $(SOURCES:.c=.o)

CFLAGS  += $(EXTRA_CFLAGS) -Wall -pedantic -std=gnu99 -O2 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread
ifeq ($(OS),Windows_NT)
else
LDFLAGS += -lrt
endif

# Honour SANITIZER= like the tests do, so the sample CI's build-only
# pass compiles this the same way.  Not meaningful for the numbers.
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
	rm -f $(OBJS) retro_eventcount_bench retro_eventcount_bench_scond

.PHONY: all clean
