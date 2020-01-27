compiler     := gcc
extra_flags  :=
use_neon     := 0
release	    := release
EXE_EXT	    :=
TARGET       := database_task
HAVE_ZLIB    := 1
HAVE_7ZIP    := 1
HAVE_THREADS := 1

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
CFLAGS += -mcpu=cortex-a9 -mtune=cortex-a9 -mfpu=neon
CXXFLAGS += -mcpu=cortex-a9 -mtune=cortex-a9 -mfpu=neon
use_neon := 1
endif

ifneq (,$(findstring hardfloat,$(extra_rules_gcc)))
extra_flags += -mfloat-abi=hard
CFLAGS += -mfloat-abi=hard
CXXFLAGS += -mfloat-abi=hard
endif

ifeq ($(build),)
build = release
endif

ifeq ($(DEBUG), 1)
build = debug
endif

ifeq (release,$(build))
extra_flags += -O2
CFLAGS += -O2
CXXFLAGS += -O2
LDFLAGS += -O2
endif

ifeq (debug,$(build))
extra_flags += -O0 -g
CFLAGS += -O0 -g
CXXFLAGS += -O0 -g
LDFLAGS += -O0 -g
endif

ifneq ($(SANITIZER),)
   CFLAGS   := -fsanitize=$(SANITIZER) $(CFLAGS)
   CXXFLAGS := -fsanitize=$(SANITIZER) $(CXXFLAGS)
   LDFLAGS  := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

EXE_EXT :=
ifeq ($(platform), unix)
else ifeq ($(platform), osx)
compiler := $(CC)
else
EXE_EXT = .exe
endif

CORE_DIR = ../../..
DEPS_DIR = $(CORE_DIR)/deps
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include

CC      := $(compiler)
CXX     := $(subst CC,++,$(compiler))
asflags := $(extra_flags)
flags   += -std=c99

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
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
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
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

DEFINES    = -DHAVE_LIBRETRODB -DHAVE_COMPRESSION

ifeq ($(HAVE_ZLIB), 1)
SOURCES_C += \
				 $(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
				 $(LIBRETRO_COMM_DIR)/streams/trans_stream_zlib.c
DEFINES += -DHAVE_ZLIB
LIBS += -lz
endif

ifeq ($(HAVE_7ZIP), 1)
SOURCES_C +=  \
				 $(LIBRETRO_COMM_DIR)/file/archive_file_7z.c
DEFINES += -DHAVE_7ZIP -D_7ZIP_ST
INCDIRS += -I$(DEPS_DIR)

SOURCES_C += $(DEPS_DIR)/7zip/7zIn.c \
				 $(DEPS_DIR)/7zip/Bra86.c \
				 $(DEPS_DIR)/7zip/7zFile.c \
				 $(DEPS_DIR)/7zip/7zStream.c \
				 $(DEPS_DIR)/7zip/LzFind.c \
				 $(DEPS_DIR)/7zip/LzmaDec.c \
				 $(DEPS_DIR)/7zip/LzmaEnc.c \
				 $(DEPS_DIR)/7zip/7zCrcOpt.c \
				 $(DEPS_DIR)/7zip/Bra.c \
				 $(DEPS_DIR)/7zip/7zDec.c \
				 $(DEPS_DIR)/7zip/Bcj2.c \
				 $(DEPS_DIR)/7zip/7zCrc.c \
				 $(DEPS_DIR)/7zip/Lzma2Dec.c \
				 $(DEPS_DIR)/7zip/7zBuf.c
endif

ifeq ($(HAVE_THREADS), 1)
SOURCES_C +=  \
				 $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c
DEFINES += -DHAVE_THREADS

ifeq (,$(findstring MSYS,$(uname -s)))
LIBS += -lpthread
endif
endif

flags     := $(INCDIRS)
INCFLAGS  := $(INCDIRS)

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
