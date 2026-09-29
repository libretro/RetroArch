TARGET := retro_eventcount_test

LIBRETRO_COMM_DIR := ../../..

# Builds the real primitive, not a copy: retro_eventcount.c and
# rthreads.c straight out of the tree, retro_atomic.h header-only.  A
# change to any of the three is what this harness exists to catch.
SOURCES := \
	retro_eventcount_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

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
# SEM=1 builds the semaphore backend - the waiter list slept on with a
# semaphore of the waiting thread's own, which Darwin and the BSDs
# select - on this host, over a POSIX semaphore, so its list protocol
# runs here under the sanitizers.  The Mach spelling on Darwin differs
# in the three semaphore calls only.
ifneq ($(SEM),)
   CFLAGS += -DRETRO_EVENTCOUNT_FORCE_SEM
   TARGET := retro_eventcount_test_sem
endif
# SEMPOOL=1 builds the pooled variant of the semaphore backend, the
# one the Vita takes because its threads have no storage of their own
# to keep a semaphore in: the same list, the semaphores drawn from a
# lock-free pool per park.  Over POSIX semaphores here; the Vita
# spelling differs in the three semaphore calls only.
ifneq ($(SEMPOOL),)
   CFLAGS += -DRETRO_EVENTCOUNT_FORCE_SEM_POOL
   TARGET := retro_eventcount_test_sempool
endif
# CTR=1 builds the 3DS address-arbiter backend on this host, against
# hostctr/3ds - the slice of libctru it uses - with the two arbiter
# wrappers stood in for over futex in ctr_arbiter_shim.c.
ifneq ($(CTR),)
   CFLAGS += -DRETRO_EVENTCOUNT_FORCE_3DS -Ihostctr
   SOURCES += ctr_arbiter_shim.c
   TARGET := retro_eventcount_test_ctr
endif
# SWITCH=1 builds the Horizon address-arbiter backend on this host,
# against hostnx/switch.h - the slice of libnx it uses - with the two
# system calls and the firmware query stood in for over futex in
# switch_arbiter_shim.c.  check runs it as current firmware and as
# firmware before 4.0.0, where it must fall back to scond.
ifneq ($(SWITCH),)
   CFLAGS += -DRETRO_EVENTCOUNT_FORCE_SWITCH -Ihostnx
   SOURCES += switch_arbiter_shim.c
   TARGET := retro_eventcount_test_switch
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
	@$(MAKE) -s clean
	@$(MAKE) -s SEM=1 $(if $(SANITIZER),SANITIZER=$(SANITIZER))
	./retro_eventcount_test_sem
	@$(MAKE) -s clean
	@$(MAKE) -s SEMPOOL=1 $(if $(SANITIZER),SANITIZER=$(SANITIZER))
	./retro_eventcount_test_sempool
	@$(MAKE) -s clean
	@$(MAKE) -s SWITCH=1 $(if $(SANITIZER),SANITIZER=$(SANITIZER))
	./retro_eventcount_test_switch
	HOSTNX_OLD_FIRMWARE=1 ./retro_eventcount_test_switch
	@$(MAKE) -s clean
	@$(MAKE) -s CTR=1 $(if $(SANITIZER),SANITIZER=$(SANITIZER))
	./retro_eventcount_test_ctr

clean:
	rm -f $(OBJS) switch_arbiter_shim.o ctr_arbiter_shim.o retro_eventcount_test \
	      retro_eventcount_test_scond retro_eventcount_test_sem \
	      retro_eventcount_test_sempool retro_eventcount_test_switch \
	      retro_eventcount_test_ctr

.PHONY: all check clean
