include ../../sample_objdir.mk

TARGET := rh265_lossless_test
# The same oracle built without threads: a decoder path that is right
# only with HAVE_THREADS - or wrong only without it - shows here.
TARGET_TEST := rh265_lossless_test_nothreads
# And built threaded on the volatile atomic backend, which has no
# pointer atomics: the decoder takes no pool there and decodes alone.
TARGET_VOLATILE := rh265_lossless_test_volatile
# Parameter sets with ue(v) codes past what any field holds
TARGET_MALFORMED := rh265_malformed_test
TARGET_WEIGHT := rh265_weight_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rh265_lossless_test.c \
	$(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
	$(LIBRETRO_COMM_DIR)/formats/h265/rh265.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/tpool.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))
NOTHREADS_SOURCES := $(filter-out %/rthreads.c %/retro_eventcount.c %/tpool.c,$(SOURCES))
NOTHREADS_OBJS    := $(call sample_objs,$(NOTHREADS_SOURCES:.c=.nt.o))
VOLATILE_OBJS     := $(call sample_objs,$(SOURCES:.c=.vo.o))

# HAVE_THREADS: the oracle decodes every case on a pool as well, and
# the threaded output has to be the single-thread output to the byte.
CFLAGS  += -Wall -std=gnu99 -g1 -O1 -DHAVE_RMP4 -DHAVE_RH265 -DHAVE_THREADS -D_GNU_SOURCE \
           -I$(LIBRETRO_COMM_DIR)/include

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir; honour it (see formats/jpeg/Makefile).
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

ifneq ($(findstring undefined,$(SANITIZER)),)
   CFLAGS += -fno-sanitize-recover=undefined
endif

LDLIBS += -lm -lpthread

all: $(TARGET) $(TARGET_TEST) $(TARGET_VOLATILE) $(TARGET_MALFORMED) $(TARGET_WEIGHT)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.nt.o: %.c
	$(CC) -c -o $@ $< $(filter-out -DHAVE_THREADS,$(CFLAGS))

$(SAMPLE_OBJDIR)/%.nt.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(filter-out -DHAVE_THREADS,$(CFLAGS))

$(TARGET_TEST): $(NOTHREADS_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(filter-out -lpthread,$(LDLIBS))

%.vo.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) -DRETRO_ATOMIC_FORCE_VOLATILE \
	   -DRETRO_ATOMIC_SUPPRESS_WARNING

$(SAMPLE_OBJDIR)/%.vo.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) -DRETRO_ATOMIC_FORCE_VOLATILE \
	   -DRETRO_ATOMIC_SUPPRESS_WARNING

$(TARGET_VOLATILE): $(VOLATILE_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

MALFORMED_OBJS := rh265_malformed_test.o $(filter-out rh265_lossless_test.o,$(OBJS))
$(TARGET_MALFORMED): $(MALFORMED_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TARGET_WEIGHT): rh265_weight_test.c $(LIBRETRO_COMM_DIR)/formats/h265/rh265.c $(LIBRETRO_COMM_DIR)/formats/h265/rh265_bd.inc $(LIBRETRO_COMM_DIR)/include/formats/rh265.h
	$(CC) $(filter-out -DHAVE_THREADS,$(CFLAGS)) -O3 $< -o $@ $(LDFLAGS) -lm

FLAGS_OBJS := rh265_output_flags_test.o $(filter-out rh265_lossless_test.o,$(OBJS))
FLAGS_NT_OBJS := rh265_output_flags_test.nt.o $(filter-out rh265_lossless_test.nt.o,$(NOTHREADS_OBJS))

rh265_output_flags_test: $(FLAGS_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

rh265_output_flags_test_nothreads: $(FLAGS_NT_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(filter-out -lpthread,$(LDLIBS))

output-flags-check: rh265_output_flags_test rh265_output_flags_test_nothreads check_output_flags.py ../vp9/h26x_fixture.py
	python3 check_output_flags.py ./rh265_output_flags_test ./rh265_output_flags_test_nothreads

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS) $(TARGET_TEST) $(NOTHREADS_OBJS) \
	   $(TARGET_VOLATILE) $(VOLATILE_OBJS) \
	   $(TARGET_MALFORMED) rh265_malformed_test.o $(TARGET_WEIGHT)
	rm -f rh265_output_flags_test rh265_output_flags_test.exe \
	   rh265_output_flags_test_nothreads rh265_output_flags_test_nothreads.exe \
	   rh265_output_flags_test.o rh265_output_flags_test.nt.o

.PHONY: clean output-flags-check
