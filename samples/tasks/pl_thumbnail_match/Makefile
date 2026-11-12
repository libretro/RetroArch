include ../../sample_objdir.mk

TARGET := pl_thumbnail_match_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The test includes the task, gfx_thumbnail_results.c and
# gfx_thumbnail_match.c.
SOURCES := pl_thumbnail_match_test.c \
           $(REPO_ROOT)/tasks/task_nbio_slice.c \
           $(REPO_ROOT)/playlist.c \
           $(REPO_ROOT)/verbosity.c \
           $(REPO_ROOT)/samples/ui_companion_log_stub.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
           $(LIBRETRO_COMM_DIR)/formats/json/rjson.c \
           $(LIBRETRO_COMM_DIR)/formats/m3u/rm3u.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
           $(LIBRETRO_COMM_DIR)/file/archive_file.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -fno-omit-frame-pointer \
           -DRARCH_INTERNAL -DHAVE_THREADS \
           -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)
LIBS    += -lpthread -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

pl_thumbnail_match_test.o: $(REPO_ROOT)/gfx/gfx_thumbnail_results.c \
                           $(REPO_ROOT)/gfx/gfx_thumbnail_results.h \
                           $(REPO_ROOT)/gfx/gfx_thumbnail_match.c \
                           $(REPO_ROOT)/gfx/gfx_thumbnail_match.h \
                           $(REPO_ROOT)/tasks/task_pl_thumbnail_match.c
# The task's menu refresh, through a stub menu
pl_thumbnail_match_test.o: CFLAGS += -DHAVE_MENU

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
	rm -rf $(SAMPLE_OBJDIR) fixture
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean
