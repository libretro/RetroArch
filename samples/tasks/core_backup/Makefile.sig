# Its own object folder: Makefile here builds the same sources with
# other flags
SAMPLE_OBJDIR := obj-sig
include ../../sample_objdir.mk

TARGET := core_backup_sig_test

# Core backups and restores with core signatures: a backup keeps the
# core's .sig, a restore or Install or Restore a Core is checked before
# it replaces a core. TEST_KEY=0 is a build with no keys at all.
# PLAY=1 (Linux) builds the task and this test as for a Play Store
# build, against the test's play feature delivery.
# Linux and mingw-w64; no compression, so no zlib.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common
TEST_KEY          ?= 1
PLAY              ?= 0

SOURCES := core_backup_sig_test.c \
           $(REPO_ROOT)/tasks/task_core_backup.c \
           $(REPO_ROOT)/tasks/task_nbio_slice.c \
           $(REPO_ROOT)/core_backup.c \
           $(REPO_ROOT)/core_install_check.c \
           $(REPO_ROOT)/core_trust_runtime.c \
           $(REPO_ROOT)/core_signature.c \
           $(REPO_ROOT)/core_signature_io.c \
           $(LIBRETRO_COMM_DIR)/crypto/pk.c \
           $(LIBRETRO_COMM_DIR)/crypto/crypto.c \
           $(LIBRETRO_COMM_DIR)/crypto/kdf.c \
           $(LIBRETRO_COMM_DIR)/utils/md5.c \
           $(LIBRETRO_COMM_DIR)/hash/lrc_hash.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/lists/dir_list.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c

ifneq ($(findstring mingw,$(CC)),)
   EXE := .exe
else
   EXE :=
   LIBS += -lpthread
endif

CFLAGS += -Wall -g1 -O0 -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT) \
          -I$(REPO_ROOT)/deps -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_MENU \
          -DHAVE_CRYPTO -DHAVE_DYNAMIC -DHAVE_CORE_SIGNATURES
ifeq ($(TEST_KEY),1)
   CFLAGS += -DHAVE_CORE_SIGNING_TEST_KEY
endif
LIBS += -lm
# The slice clock, so a lane can cancel a restore part-way through its
# check
LDFLAGS += -Wl,--wrap=cpu_features_get_time_usec

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# Only these two, which share the menu's ANDROID layout; the others
# hand msg_hash values only to the stand-in, which ignores them
ifeq ($(PLAY),1)
core_backup_sig_test.o $(call sample_objs,$(REPO_ROOT)/tasks/task_core_backup.o): \
   CFLAGS += -DANDROID
endif

all: $(TARGET)$(EXE)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET)$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

check: all
	rm -rf core_backup_sig_test_tmp
	$(RUNNER) ./$(TARGET)$(EXE)

clean:
	rm -rf $(SAMPLE_OBJDIR) core_backup_sig_test_tmp
	rm -f $(TARGET) $(TARGET).exe core_backup_sig_test.o

.PHONY: all check clean
