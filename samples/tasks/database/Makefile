include ../../sample_objdir.mk

compiler     := gcc
extra_flags  :=
use_neon     := 0
release	    := release
EXE_EXT	    :=
TARGET       := database_task
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
extra_flags += -O0 -g1
CFLAGS += -O0 -g1
CXXFLAGS += -O0 -g1
LDFLAGS += -O0 -g1
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
	$(LIBRETRO_COMM_DIR)/memory/mem_stats.c \
	$(CORE_DIR)/tasks/task_database.c \
	$(CORE_DIR)/tasks/task_nbio_slice.c \
	$(CORE_DIR)/tasks/task_database_cue.c \
	$(CORE_DIR)/database_info.c \
	$(CORE_DIR)/core_info.c \
	$(CORE_DIR)/msg_hash.c \
	$(CORE_DIR)/playlist.c \
	$(CORE_DIR)/manual_content_scan.c \
	$(CORE_DIR)/verbosity.c \
	$(LIBRETRO_COMM_DIR)/formats/m3u/rm3u.c \
	$(LIBRETRO_COMM_DIR)/formats/logiqx_dat/logiqx_dat.c \
	$(CORE_DIR)/libretro-db/bintree.c \
	$(CORE_DIR)/libretro-db/libretrodb.c \
	$(CORE_DIR)/libretro-db/query.c \
	$(CORE_DIR)/libretro-db/rmsgpack.c \
	$(CORE_DIR)/libretro-db/rmsgpack_dom.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file.c \
	$(LIBRETRO_COMM_DIR)/file/config_file.c \
	$(LIBRETRO_COMM_DIR)/file/config_file_io.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/hash/lrc_hash.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_fnmatch.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/formats/json/rjson.c \
	$(LIBRETRO_COMM_DIR)/formats/xml/rxml.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/lists/dir_list.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/streams/rzip_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_rzstd.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_zstd.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_bz2.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_rar.c \
	$(LIBRETRO_COMM_DIR)/formats/rar/rrar_archive.c \
	$(LIBRETRO_COMM_DIR)/formats/rar/rrar_ppmd7.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_rzstd.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_xxh64.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_rbzip2.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

# For the scan test: a lost wake, the scan's or a reader's, shows as a
# stall rather than a 100 ms watchdog, and the reader cutoff suits its
# fixtures.  The objects are shared, so the index and budget tests and
# database_task build task_database.c with these too.
# SCAN_TEST_DEFINES= clears them, and SCAN_DEFINES adds more
# (-DSCAN_READERS_MAX=0).
SCAN_TEST_DEFINES ?= -DSCAN_READER_PARK_USEC=30000000 \
	-DSCAN_READER_CUTOFF=65536
DEFINES    = -DHAVE_LIBRETRODB -DHAVE_COMPRESSION \
	-DHAVE_RZSTD -DHAVE_RBZIP2 -DHAVE_RRAR \
	$(SCAN_TEST_DEFINES) $(SCAN_DEFINES)

LIBS += -lm

ifeq ($(HAVE_7ZIP), 1)
SOURCES_C +=  \
				 $(LIBRETRO_COMM_DIR)/file/archive_file_7z.c
DEFINES += -DHAVE_7ZIP
INCDIRS += -I$(DEPS_DIR)

SOURCES_C += $(LIBRETRO_COMM_DIR)/formats/7z/r7z_archive.c \
				 $(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma.c \
				 $(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma_stream.c \
				 $(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma2.c \
				 $(LIBRETRO_COMM_DIR)/formats/7z/r7z_bcj2.c \
				 $(LIBRETRO_COMM_DIR)/formats/7z/r7z_filters.c
endif

ifeq ($(HAVE_THREADS), 1)
SOURCES_C +=  \
				 $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
				 $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c
DEFINES += -DHAVE_THREADS

ifeq (,$(findstring MSYS,$(uname -s)))
LIBS += -lpthread
endif
endif

flags     := $(INCDIRS)
INCFLAGS  := $(INCDIRS)

CFLAGS    += $(DEFINES)
CXXFLAGS  += $(DEFINES)

OBJECTS    = $(call sample_objs,$(SOURCES_C:.c=.o))

# The index pairing test links the same objects but its own entry
# point, so the scanner's main() is swapped out for it.
SCAN_TEST         := database_scan_test
SCAN_TEST_MAIN    := $(CORE_DIR)/samples/tasks/database/database_scan_test.o
SCAN_TEST_OBJECTS  = $(filter-out $(call sample_objs,$(CORE_DIR)/samples/tasks/database/main.o),$(OBJECTS)) \
	$(SCAN_TEST_MAIN)

INDEX_TEST        := database_index_test
INDEX_TEST_MAIN   := $(CORE_DIR)/samples/tasks/database/database_index_test.o
INDEX_TEST_OBJECTS = $(filter-out $(call sample_objs,$(CORE_DIR)/samples/tasks/database/main.o),$(OBJECTS)) \
	$(INDEX_TEST_MAIN)

BEGIN_TEST        := scan_begin_budget_test
BEGIN_TEST_MAIN   := $(CORE_DIR)/samples/tasks/database/scan_begin_budget_test.o
BEGIN_TEST_OBJECTS = $(filter-out $(call sample_objs,$(CORE_DIR)/samples/tasks/database/main.o),$(OBJECTS)) \
	$(BEGIN_TEST_MAIN)

CLAIM_TEST        := core_info_claim_test
CLAIM_TEST_MAIN   := $(CORE_DIR)/samples/tasks/database/core_info_claim_test.o
CLAIM_TEST_OBJECTS = $(filter-out $(call sample_objs,$(CORE_DIR)/samples/tasks/database/main.o),$(OBJECTS)) \
	$(CLAIM_TEST_MAIN)

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

all: $(TARGET)$(EXE_EXT) $(BEGIN_TEST)$(EXE_EXT)
$(TARGET)$(EXE_EXT): $(OBJECTS)
	$(LD)  $(LINKOUT)$@ $(SHARED) $(OBJECTS) $(LDFLAGS) $(LIBS)

$(INDEX_TEST)$(EXE_EXT): $(INDEX_TEST_OBJECTS)
	$(LD)  $(LINKOUT)$@ $(SHARED) $(INDEX_TEST_OBJECTS) $(LDFLAGS) $(LIBS)

# Builds its own databases, so it needs nothing but a writable dir.
# The wraps count the scanner's database walks and parks, slow chosen
# reads and tell the scan's own from its readers', spend its window,
# count and refuse reader pushes, count queue worker starts, count an
# archive's stored-CRC and directory reads, fail a chosen serial's
# query, order the database and content lists, count reader passes,
# hold chosen lookups, count an archive's member hits a step and hold
# a chosen index build in one step.
SCAN_TEST_WRAPS := -Wl,--wrap=libretrodb_open -Wl,--wrap=task_queue_park \
	-Wl,--wrap=intfstream_crc_step -Wl,--wrap=rm3u_is_m3u \
	-Wl,--wrap=task_nbio_slice_open -Wl,--wrap=task_nbio_slice_close \
	-Wl,--wrap=task_queue_push \
	-Wl,--wrap=file_archive_get_file_crc32_and_size \
	-Wl,--wrap=file_archive_parse_file_iterate \
	-Wl,--wrap=bin_to_hex_alloc -Wl,--wrap=dir_list_new \
	-Wl,--wrap=task_queue_is_threaded \
	-Wl,--wrap=database_info_list_new_crc \
	-Wl,--wrap=database_info_index_job_new \
	-Wl,--wrap=database_info_dir_init_from_list \
	-Wl,--wrap=database_info_index_job_step \
	-Wl,--wrap=task_nbio_slice_within_budget \
	-Wl,--wrap=fill_pathname_join_delim
ifeq ($(HAVE_THREADS), 1)
SCAN_TEST_WRAPS += -Wl,--wrap=sthread_create
endif
$(SCAN_TEST)$(EXE_EXT): $(SCAN_TEST_OBJECTS)
	$(LD)  $(LINKOUT)$@ $(SHARED) $(SCAN_TEST_OBJECTS) $(LDFLAGS) $(SCAN_TEST_WRAPS) $(LIBS)

$(BEGIN_TEST)$(EXE_EXT): $(BEGIN_TEST_OBJECTS)
	$(LD)  $(LINKOUT)$@ $(SHARED) $(BEGIN_TEST_OBJECTS) $(LDFLAGS) \
	      -Wl,--wrap=task_nbio_slice_open $(LIBS)

# Needs a writable dir for its core info.
$(CLAIM_TEST)$(EXE_EXT): $(CLAIM_TEST_OBJECTS)
	$(LD)  $(LINKOUT)$@ $(SHARED) $(CLAIM_TEST_OBJECTS) $(LDFLAGS) $(LIBS)

# The scan test runs twice in one root: what a run leaves behind must
# not decide the next one's checks. A sanitized build gives the budget
# test its sanitizer's allowance, as CI does.
check: $(INDEX_TEST)$(EXE_EXT) $(SCAN_TEST)$(EXE_EXT) $(BEGIN_TEST)$(EXE_EXT) \
	$(CLAIM_TEST)$(EXE_EXT)
	./$(INDEX_TEST)$(EXE_EXT)
	root=$$(mktemp -d) && ./$(CLAIM_TEST)$(EXE_EXT) "$$root"; r=$$?; \
	   rm -rf "$$root"; exit $$r
	root=$$(mktemp -d) && ./$(SCAN_TEST)$(EXE_EXT) "$$root" \
	   && ./$(SCAN_TEST)$(EXE_EXT) "$$root"; r=$$?; rm -rf "$$root"; exit $$r
	./$(BEGIN_TEST)$(EXE_EXT) $(if $(SANITIZER),sanitize)

%.o: %.c
	$(CC) $(INCFLAGS) $(CFLAGS) -c $(OBJOUT)$@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(INCFLAGS) $(CFLAGS) -c $(OBJOUT)$@ $<

%.o: %.cpp
	$(CXX) $(INCFLAGS) $(CXXFLAGS) -c $(OBJOUT)$@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).cpp
	@mkdir -p $(@D)
	$(CXX) $(INCFLAGS) $(CXXFLAGS) -c $(OBJOUT)$@ $<

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJECTS) $(INDEX_TEST_MAIN) $(SCAN_TEST_MAIN) $(BEGIN_TEST_MAIN) \
	      $(CLAIM_TEST_MAIN) \
	      $(TARGET)$(EXE_EXT) $(INDEX_TEST)$(EXE_EXT) $(SCAN_TEST)$(EXE_EXT) \
	      $(BEGIN_TEST)$(EXE_EXT) $(CLAIM_TEST)$(EXE_EXT)

.PHONY: all check clean
