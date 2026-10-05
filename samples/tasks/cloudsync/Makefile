# Regression tests for cloud sync:
#  - cloudsync_savefile_owner_test: the save-file ownership predicate
#    cloud sync consults before it replaces or removes a local file, and
#    the manifest key lookup its carry-forward depends on.
#  - cloudsync_path_safety_test: the manifest-key path-traversal guard
#    (tasks/task_cloudsync_path.c, #included by the test).
#  - cloudsync_manifest_adds_test: entries added to the updated
#    manifests from several threads at once, with no lock
#    (tasks/task_cloudsync.c, #included by the test).
#  - cloudsync_pacing_test: the diff runs as much as the shared
#    per-frame I/O window grants, hashing large files a chunk at a time
#    (tasks/task_cloudsync.c, #included by the test).
#  - cloudsync_async_test: a blocking driver's calls run on a worker
#    and complete through cloud_sync_poll() on the caller's thread
#    (network/cloud_sync_driver.c).
#
#   make check
#   make check SANITIZER=address
#   make check SANITIZER=undefined

TARGET            := cloudsync_savefile_owner_test
PATH_TARGET       := cloudsync_path_safety_test
CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES := \
	cloudsync_savefile_owner_test.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/lists/file_list.c \
	$(LIBRETRO_COMM_DIR)/lists/dir_list.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c

OBJS := $(SOURCES:.c=.o)

PATH_OBJS := cloudsync_path_safety_test.o

# Built in one step with the task's own defines: the library objects
# above are compiled without threads.
ADDS_TARGET  := cloudsync_manifest_adds_test
ADDS_SOURCES := cloudsync_manifest_adds_test.c \
	$(filter-out cloudsync_savefile_owner_test.c,$(SOURCES)) \
	$(CORE_DIR)/tasks/task_cloudsync_path.c \
	$(LIBRETRO_COMM_DIR)/formats/json/rjson.c \
	$(LIBRETRO_COMM_DIR)/utils/md5.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c
PACING_TARGET := cloudsync_pacing_test

ASYNC_TARGET  := cloudsync_async_test
ASYNC_SOURCES := cloudsync_async_test.c \
	$(CORE_DIR)/network/cloud_sync_driver.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c
ASYNC_DEFS    := -DHAVE_THREADS -DRARCH_INTERNAL -DHAVE_CLOUDSYNC \
	-DHAVE_NETWORKING

ADDS_DEFS    := -DHAVE_THREADS -DRARCH_INTERNAL -DHAVE_CLOUDSYNC \
	-DHAVE_NETWORKING -I$(CORE_DIR)/deps

CFLAGS += -Wall -pedantic -std=gnu99 -g -O1 -fno-omit-frame-pointer \
          -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(PATH_TARGET) $(ADDS_TARGET) $(PACING_TARGET) $(ASYNC_TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(PATH_TARGET): $(PATH_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(ADDS_TARGET): $(ADDS_SOURCES) $(CORE_DIR)/tasks/task_cloudsync.c
	$(CC) -o $@ $(ADDS_SOURCES) $(filter-out -pedantic,$(CFLAGS)) $(ADDS_DEFS) $(LDFLAGS) -lpthread

$(PACING_TARGET): cloudsync_pacing_test.c $(filter-out cloudsync_manifest_adds_test.c,$(ADDS_SOURCES)) $(CORE_DIR)/tasks/task_cloudsync.c
	$(CC) -o $@ cloudsync_pacing_test.c $(filter-out cloudsync_manifest_adds_test.c,$(ADDS_SOURCES)) $(filter-out -pedantic,$(CFLAGS)) $(ADDS_DEFS) $(LDFLAGS) -lpthread

$(ASYNC_TARGET): $(ASYNC_SOURCES) $(CORE_DIR)/network/cloud_sync_driver.h
	$(CC) -o $@ $(ASYNC_SOURCES) $(filter-out -pedantic,$(CFLAGS)) $(ASYNC_DEFS) $(LDFLAGS) -lpthread

check: $(TARGET) $(PATH_TARGET) $(ADDS_TARGET) $(PACING_TARGET) $(ASYNC_TARGET)
	./$(TARGET)
	./$(PATH_TARGET)
	./$(ADDS_TARGET)
	./$(PACING_TARGET)
	./$(ASYNC_TARGET)

clean:
	rm -f $(TARGET) $(OBJS) $(PATH_TARGET) $(PATH_OBJS) $(ADDS_TARGET) $(PACING_TARGET) $(ASYNC_TARGET)

.PHONY: all check clean
