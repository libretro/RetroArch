include ../../sample_objdir.mk

TARGET := autodetect_handoff_test

# Drives the shipping tasks/task_autodetect.c on the shipping task
# queue, with the rest of RetroArch stubbed in the test.  The queue's
# find and push are wrapped to see which thread calls them.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := autodetect_handoff_test.c \
           $(REPO_ROOT)/tasks/task_autodetect.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -std=gnu99 -g1 -DHAVE_THREADS -DRARCH_INTERNAL \
           -DHAVE_TEST_DRIVERS -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -Wl,--wrap=task_queue_find -Wl,--wrap=task_queue_push \
           -lpthread -lm

# Without HAVE_MENU nothing it includes declares command_event().
AUTODETECT_O := $(call sample_objs,$(REPO_ROOT)/tasks/task_autodetect.o)
$(AUTODETECT_O): CFLAGS += -include $(REPO_ROOT)/command.h

# The samples workflow passes SANITIZER=address,undefined; honour it.
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: clean all
