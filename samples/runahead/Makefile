compiler     := gcc
TARGET       := runahead_copy_test

CORE_DIR = ../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -I$(CORE_DIR)/deps

CC      := $(compiler)
flags   += -std=gnu99

# The core-copy fragment of runahead.c is bounded by its BEGIN/END
# markers and depends only on libretro-common plus libc; it is
# extracted verbatim at build time so the harness runs the shipping
# code. The task queue is the real one, built without threads so a
# pushed task runs inside task_queue_check() on this thread.
SOURCES_C := \
	runahead_copy_test.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS += -Wall $(INCDIRS) -DRARCH_INTERNAL -D_GNU_SOURCE
LDFLAGS += -lm -lpthread

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

runahead_copy_fragment.c: $(CORE_DIR)/runahead.c
	sed -n '/===== BEGIN runahead core-copy fragment =====/,/===== END runahead core-copy fragment =====/p' $< > $@

preempt_mask_fragment.c: $(CORE_DIR)/runahead.c
	sed -n '/===== BEGIN preempt analog-mask bit =====/,/===== END preempt analog-mask bit =====/p' $< > $@

dense_cache_fragment.c: $(CORE_DIR)/runahead.c
	sed -n '/===== BEGIN runahead dense input cache =====/,/===== END runahead dense input cache =====/p' $< > $@

preempt_slab_fragment.c: $(CORE_DIR)/runahead.c
	sed -n '/===== BEGIN preempt slab =====/,/===== END preempt slab =====/p' $< > $@

runahead_copy_test.o: runahead_copy_fragment.c preempt_mask_fragment.c dense_cache_fragment.c preempt_slab_fragment.c

%.o: %.c
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -c -o$@ $<

$(TARGET): $(OBJS)
	$(CC) -o$@ $(OBJS) $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) runahead_copy_fragment.c preempt_mask_fragment.c dense_cache_fragment.c preempt_slab_fragment.c
	rm -f $(TARGET)

.PHONY: clean all check
