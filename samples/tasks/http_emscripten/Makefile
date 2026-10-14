include ../../sample_objdir.mk

TARGET := http_emscripten_park_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The unit under test is tasks/task_http_emscripten.c, included by the
# test and built against the stand-in Emscripten headers in include/;
# the task queue is the real one.  task_get_flags() is wrapped so the
# test can count the handler's passes.
SOURCES := http_emscripten_park_test.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -fno-omit-frame-pointer \
           -DHAVE_THREADS -DHAVE_NETWORKING -DRARCH_INTERNAL \
           -Iinclude -I$(REPO_ROOT) -I$(REPO_ROOT)/tasks \
           -I$(LIBRETRO_COMM_DIR)/include

LDFLAGS += -Wl,--wrap=task_get_flags
LIBS    += -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean
