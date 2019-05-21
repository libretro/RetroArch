compiler    := gcc
extra_flags :=
use_neon    := 0
release	   := release
EXE_EXT	      :=
TARGET      := database_task

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
   arch = intel
ifeq ($(shell uname -p),powerpc)
   arch = ppc
endif
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
endif
endif

ifeq ($(compiler),gcc)
extra_rules_gcc := $(shell $(compiler) -dumpmachine)
endif

ifneq (,$(findstring armv7,$(extra_rules_gcc)))
extra_flags += -mcpu=cortex-a9 -mtune=cortex-a9 -mfpu=neon
use_neon := 1
endif

ifneq (,$(findstring hardfloat,$(extra_rules_gcc)))
extra_flags += -mfloat-abi=hard
endif

ifeq (release,$(build))
extra_flags += -O2
endif

ifeq (debug,$(build))
extra_flags += -O0 -g
endif

ldflags :=

EXE_EXT :=
ifeq ($(platform), unix)
else ifeq ($(platform), osx)
compiler := $(CC)
else
EXE_EXT = .exe
endif

CORE_DIR = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common

CC      := $(compiler)
CXX     := $(subst CC,++,$(compiler))
flags   := -I$(LIBRETRO_COMM_DIR)/include
asflags := $(extra_flags)
LDFLAGS :=
flags   += -std=c99
INCFLAGS  := -I$(LIBRETRO_COMM_DIR)/include

SOURCES_C := \
	$(CORE_DIR)/samples/tasks/database/main.c \
	$(CORE_DIR)/tasks/task_database.c \
	$(CORE_DIR)/tasks/task_database_cue.c \
	$(CORE_DIR)/database_info.c \
	$(CORE_DIR)/core_info.c \
	$(CORE_DIR)/file_path_str.c \
	$(CORE_DIR)/msg_hash.c \
	$(CORE_DIR)/intl/msg_hash_us.c \
	$(CORE_DIR)/playlist.c \
	$(CORE_DIR)/verbosity.c \
	$(CORE_DIR)/libretro-db/bintree.c \
	$(CORE_DIR)/libretro-db/libretrodb.c \
	$(CORE_DIR)/libretro-db/query.c \
	$(CORE_DIR)/libretro-db/rmsgpack.c \
	$(CORE_DIR)/libretro-db/rmsgpack_dom.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file.c \
	$(LIBRETRO_COMM_DIR)/file/config_file.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/hash/rhash.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_fnmatch.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/formats/json/jsonsax.c \
	$(LIBRETRO_COMM_DIR)/formats/json/jsonsax_full.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/lists/dir_list.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

DEFINES    = -DHAVE_LIBRETRODB -DHAVE_COMPRESSION

CFLAGS    += $(DEFINES)
CXXFLAGS  += $(DEFINES)

OBJECTS    = $(SOURCES_C:.c=.o)

OBJOUT   = -o
LINKOUT  = -o

ifneq (,$(findstring msvc,$(platform)))
	OBJOUT = -Fo
LINKOUT = -out:
ifeq ($(STATIC_LINKING),1)
	LD ?= lib.exe
else
	LD = link.exe
endif
else
	LD = $(CC)
endif

all: $(TARGET)$(EXE_EXT)
$(TARGET)$(EXE_EXT): $(OBJECTS)
	$(LD)  $(LINKOUT)$@ $(SHARED) $(OBJECTS) $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(INCFLAGS) $(CFLAGS) -c $(OBJOUT)$@ $<

%.o: %.cpp
	$(CXX) $(INCFLAGS) $(CXXFLAGS) -c $(OBJOUT)$@ $<

clean:
	rm -f $(OBJECTS)
