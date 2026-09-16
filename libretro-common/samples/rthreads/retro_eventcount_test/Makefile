TARGET := retro_eventcount_test

LIBRETRO_COMM_DIR := ../../..

# Builds the real primitive, not a copy: retro_eventcount.c and
# rthreads.c straight out of the tree, retro_atomic.h header-only.  A
# change to any of the three is what this harness exists to catch.
SOURCES := \
	retro_eventcount_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

CFLAGS  += -Wall -W -std=gnu99 -g -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread
# rthreads.c uses clock_gettime + CLOCK_REALTIME on Linux glibc; on
# older glibc those live in -lrt.  Harmless on newer glibc.
LDFLAGS += -lrt

# SCOND=1 forces the condition-variable backend, which is what macOS,
# the BSDs and the console ports get.  'check' runs both: the default
# build covers this host's address-wait path, the SCOND build covers
# the path most targets actually take.
#
# The Windows tiers are not covered by 'check' because they need a
# cross compiler and something to run the result on, but they are
# testable from a Linux host and worth doing after any change to
# retro_eventcount.c's win32 half:
#
#   x86_64-w64-mingw32-gcc -o ec.exe retro_eventcount_test.c \
#       ../../../rthreads/retro_eventcount.c \
#       ../../../rthreads/rthreads.c \
#       -Wall -W -std=gnu99 -O1 -DHAVE_THREADS -I../../../include
#   for t in alert keyed event none; do \
#       RETRO_EVENTCOUNT_WIN32=$$t wine64 ./ec.exe; done
#
# RETRO_EVENTCOUNT_WIN32 pins the sleep tier.  'none' pins the case
# where no tier is available at all, which otherwise needs the process
# to run out of TLS indices; that path used to hang, and is the reason
# the knob exists.
ifneq ($(SCOND),)
   CFLAGS += -DRETRO_EVENTCOUNT_FORCE_SCOND
   TARGET := retro_eventcount_test_scond
endif

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
	@$(MAKE) -s clean
	@$(MAKE) -s SCOND=1 $(if $(SANITIZER),SANITIZER=$(SANITIZER))
	./retro_eventcount_test_scond

clean:
	rm -f $(OBJS) retro_eventcount_test retro_eventcount_test_scond

.PHONY: all check clean
