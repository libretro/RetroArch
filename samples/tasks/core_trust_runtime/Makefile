include ../../sample_objdir.mk

TARGET := core_trust_runtime_test

# RetroArch's trust store (core_trust_runtime.c): the startup load,
# saves and snapshots put on another thread. TEST_KEY=0 is a build
# with no keys at all, LOAD_CHECK=0 one without the load check, as
# the tree builds by default.
# Linux and mingw-w64.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common
TEST_KEY          ?= 1
LOAD_CHECK        ?= 1

SOURCES := core_trust_runtime_test.c \
           $(REPO_ROOT)/core_trust_runtime.c \
           $(REPO_ROOT)/core_signature.c \
           $(REPO_ROOT)/core_signature_io.c \
           $(LIBRETRO_COMM_DIR)/crypto/pk.c \
           $(LIBRETRO_COMM_DIR)/crypto/crypto.c \
           $(LIBRETRO_COMM_DIR)/crypto/kdf.c \
           $(LIBRETRO_COMM_DIR)/utils/md5.c \
           $(LIBRETRO_COMM_DIR)/hash/lrc_hash.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c

ifneq ($(findstring mingw,$(CC)),)
   EXE := .exe
else
   EXE :=
   LIBS += -lpthread
endif

CFLAGS += -Wall -g1 -O0 -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT) \
          -I$(REPO_ROOT)/deps -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_CRYPTO \
          -DHAVE_DYNAMIC -DHAVE_MENU -DHAVE_CORE_SIGNATURES
ifeq ($(TEST_KEY),1)
   CFLAGS += -DHAVE_CORE_SIGNING_TEST_KEY
endif
ifeq ($(LOAD_CHECK),1)
   CFLAGS += -DHAVE_CORE_LOAD_CHECK
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

# The tree's C89_BUILD holds core_trust_runtime.c to C89; the test is
# C89 here too
core_trust_runtime_test.o: CFLAGS += -std=c89 -Wno-long-long \
          -Wdeclaration-after-statement -Wframe-larger-than=4096

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
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
	rm -rf core_trust_runtime_test_tmp
	$(RUNNER) ./$(TARGET)$(EXE)

clean:
	rm -rf $(SAMPLE_OBJDIR) core_trust_runtime_test_tmp
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean
