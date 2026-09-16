TARGET := retro_procbarrier_test

LIBRETRO_COMM_DIR := ../../..

# The real primitive out of the tree, plus rthreads for the threads and
# the condvar the consumer parks on.
SOURCES := \
	retro_procbarrier_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_procbarrier.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

CFLAGS  += -Wall -W -std=gnu99 -g -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread
# rthreads.c uses clock_gettime on Linux glibc; on older glibc that is in
# -lrt. Harmless on newer glibc.
LDFLAGS += -lrt

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

# check runs the natural resolution, then forces each tier this platform
# can have so all of them are exercised on one machine. A tier the kernel
# lacks falls through to the natural one, so a forced run never fails for
# being unavailable -- the tier line in the output says which ran.
check: all
	./$(TARGET)
	RETRO_PROCBARRIER=membarrier ./$(TARGET)
	RETRO_PROCBARRIER=pageflip   ./$(TARGET)
	RETRO_PROCBARRIER=signal     ./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
