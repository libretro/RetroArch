compiler     := gcc
extra_flags  :=
EXE_EXT      :=
TARGET       := dirwalk_budget_test
HAVE_THREADS := 1

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
endif
endif

ifeq ($(build),)
build = release
endif

ifeq ($(DEBUG), 1)
build = debug
endif

ifeq (release,$(build))
CFLAGS += -O2
LDFLAGS += -O2
endif

ifeq (debug,$(build))
CFLAGS += -O0 -g
LDFLAGS += -O0 -g
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

ifeq ($(platform), unix)
else ifeq ($(platform), osx)
compiler := $(CC)
else
EXE_EXT = .exe
endif

CORE_DIR = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

CC      := $(compiler)
flags   += -std=c99

SOURCES_C := \
	$(CORE_DIR)/samples/menu/dirwalk/dirwalk_budget_test.c \
	$(CORE_DIR)/menu/menu_dirwalk.c \
	$(CORE_DIR)/tasks/task_nbio_slice.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/lists/dir_list.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

CFLAGS += -Wall $(INCDIRS)
CFLAGS += -DHAVE_THREADS -D_GNU_SOURCE
# Tiny sort-machinery constants: the oracle drives run sorting, the
# resumable merge, and sort-only deferral deterministically.
CFLAGS += -DMENU_DIRWALK_SORT_RUN=32 -DMENU_DIRWALK_SORT_BATCH=8 -DMENU_DIRWALK_SORT_SYNC_MAX=64
LDFLAGS += -lm -lpthread

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

%.o: %.c
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -c -o$@ $<

$(TARGET): $(OBJS)
	$(CC) -o$@$(EXE_EXT) $(OBJS) $(LDFLAGS)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean all
