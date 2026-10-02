TARGET := gfx_widgets_task_gate_test

# The task queue's retirement push against the widgets freed on the
# video thread. The test includes gfx/gfx_widgets.c for its static free
# path and links the real task queue.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := gfx_widgets_task_gate_test.c \
           ../gfx_widgets_task_bar_stack/stubs_retroarch.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_THREADS -DHAVE_MENU -DHAVE_GFX_WIDGETS \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/gfx_widgets.c
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
