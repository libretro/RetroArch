# retro_eventcount_init_lock_free in three builds: this host's lock-free
# backend, the forced condition-variable backend (where it must refuse),
# and the build with no host clock and fixed spin counts, which also
# compiles features_cpu.c and file_path.c without their clock functions.
LIBRETRO_COMM_DIR := ../../..

SOURCES := retro_eventcount_lock_free_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

CLOCKLESS_EXTRA := $(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -Wall -W -std=gnu99 -g1 -O1 -DHAVE_THREADS \
           -I$(LIBRETRO_COMM_DIR)/include
LDLIBS  += -lpthread -lrt

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# One line: the samples workflow reads TARGETS from its first line only
TARGETS := retro_eventcount_lock_free_test retro_eventcount_lock_free_test_scond retro_eventcount_lock_free_test_clockless

all: $(TARGETS)

retro_eventcount_lock_free_test: $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) $(LDLIBS)

retro_eventcount_lock_free_test_scond: $(SOURCES)
	$(CC) $(CFLAGS) -DRETRO_EVENTCOUNT_FORCE_SCOND -DEXPECT_LOCK_FREE=0 \
	   -o $@ $(SOURCES) $(LDFLAGS) $(LDLIBS)

retro_eventcount_lock_free_test_clockless: $(SOURCES) $(CLOCKLESS_EXTRA)
	$(CC) $(CFLAGS) -DRETRO_DISABLE_HOST_CLOCK -DRETRO_EVENTCOUNT_FIXED_SPIN=64 \
	   -DRTHREADS_SCOND_FIXED_SPIN=64 \
	   -o $@ $(SOURCES) $(CLOCKLESS_EXTRA) $(LDFLAGS) $(LDLIBS)

check: all
	./retro_eventcount_lock_free_test
	./retro_eventcount_lock_free_test_scond
	./retro_eventcount_lock_free_test_clockless

clean:
	rm -f $(TARGETS)

.PHONY: all check clean
