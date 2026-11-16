include ../../sample_objdir.mk

TARGET := core_load_check_test

# The load check (core_load_check.c) against the real trust store,
# checks and loader, the frontend stubbed. Linux and mingw-w64.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := core_load_check_test.c \
           $(REPO_ROOT)/core_load_check.c \
           $(REPO_ROOT)/core_load_file.c \
           $(REPO_ROOT)/core_trust_runtime.c \
           $(REPO_ROOT)/core_signature.c \
           $(REPO_ROOT)/core_signature_io.c \
           $(LIBRETRO_COMM_DIR)/dynamic/dylib.c \
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
   EXE       := .exe
   LIB_EXT   := .dll
   LIB_FLAGS :=
else
   EXE       :=
   LIB_EXT   := .so
   LIB_FLAGS := -fPIC
   LIBS += -lpthread -ldl
   ifeq ($(shell uname -s),Darwin)
      LIBS += -framework CoreFoundation
   endif
   ifeq ($(shell uname -s),Linux)
      # no_ctime_fs.c: a filesystem that doesn't keep ctime, for the
      # held-core tests
      SOURCES += no_ctime_fs.c
      LDFLAGS += -Wl,--wrap=fstatfs -Wl,--wrap=fstatfs64
      # The check's fs-verity step against stand-ins for the kernel's,
      # in a second build of core_load_check.c as for Android
      CFLAGS  += -DVERITY_STANDIN
      LDFLAGS += -Wl,--wrap=core_file_verity_enable \
                 -Wl,--wrap=core_file_verity_measure -Wl,--wrap=ioctl \
                 -Wl,--wrap=core_trust_runtime_always
      ANDROID_TEST := $(TARGET)_android
   endif
endif

CFLAGS += -Wall -g1 -O0 -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT) \
          -I$(REPO_ROOT)/deps -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_CRYPTO \
          -DHAVE_DYNAMIC -DHAVE_MENU -DHAVE_CORE_SIGNATURES \
          -DHAVE_CORE_LOAD_CHECK -DHAVE_CORE_SIGNING_TEST_KEY

# Counts the hashes the check starts, and moves a core between its
# check and its pass; Darwin's ld has no --wrap
ifneq ($(shell uname -s),Darwin)
   CFLAGS  += -DCOUNT_HASHES
   LDFLAGS += -Wl,--wrap=core_hash_init \
              -Wl,--wrap=core_trust_runtime_apply
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

# _POSIX_C_SOURCE lets retro_timers.h see struct timespec under C89
core_load_check_test.o: CFLAGS += -std=c89 -Wno-long-long \
          -Wdeclaration-after-statement -Wframe-larger-than=4096 \
          -D_POSIX_C_SOURCE=199309L
ifeq ($(shell uname -s),Darwin)
# _POSIX_C_SOURCE hides strlcpy() from Darwin string.h, which strl.h relies on
core_load_check_test.o: CFLAGS += -D_DARWIN_C_SOURCE
endif

# No other C89 build compiles the check's Windows branch
$(SAMPLE_OBJDIR)/__/__/__/core_load_check.o: CFLAGS += \
          -Werror=declaration-after-statement

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# core_signature's test library, built three times, told apart by
# core_load_test_id()
TEST_LIBS := core_load_check_test_a$(LIB_EXT) core_load_check_test_b$(LIB_EXT) \
             core_load_check_test_c$(LIB_EXT)

all: $(TARGET)$(EXE) $(ANDROID_TEST) $(TEST_LIBS)

core_load_check_test_a$(LIB_EXT): ../core_signature/core_load_lib.c
	$(CC) -shared $(LIB_FLAGS) -DCORE_LOAD_ID=1 -o $@ $<

core_load_check_test_b$(LIB_EXT): ../core_signature/core_load_lib.c
	$(CC) -shared $(LIB_FLAGS) -DCORE_LOAD_ID=2 -o $@ $<

core_load_check_test_c$(LIB_EXT): ../core_signature/core_load_lib.c
	$(CC) -shared $(LIB_FLAGS) -DCORE_LOAD_ID=3 -o $@ $<

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET)$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

# Its other Android blocks (loads by fd, real paths) would fail the
# desktop tests: this build runs only the fs-verity and load-by-fd ones,
# with the tree's NDK header stubs. -rdynamic: core_load_file.c finds
# the test's android_dlopen_ext by dlsym.
ifneq ($(ANDROID_TEST),)
$(SAMPLE_OBJDIR)/core_load_check_android.o: $(REPO_ROOT)/core_load_check.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) -DANDROID \
	   -I$(REPO_ROOT)/tools/platform_stubs/android \
	   -Werror=declaration-after-statement

$(SAMPLE_OBJDIR)/core_load_file_android.o: $(REPO_ROOT)/core_load_file.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) -DANDROID -D_GNU_SOURCE \
	   -I$(REPO_ROOT)/tools/platform_stubs/android \
	   -Werror=declaration-after-statement

$(ANDROID_TEST): $(SAMPLE_OBJDIR)/core_load_check_android.o \
      $(SAMPLE_OBJDIR)/core_load_file_android.o \
      $(filter-out $(SAMPLE_OBJDIR)/__/__/__/core_load_check.o \
         $(SAMPLE_OBJDIR)/__/__/__/core_load_file.o,$(OBJS))
	$(CC) -o $@ $^ -rdynamic $(LDFLAGS) $(LIBS)
endif

check: all
	rm -rf core_load_check_test_tmp
	$(RUNNER) ./$(TARGET)$(EXE)
ifneq ($(ANDROID_TEST),)
	rm -rf core_load_check_test_tmp
	$(RUNNER) ./$(ANDROID_TEST) android
endif

clean:
	rm -rf $(SAMPLE_OBJDIR) core_load_check_test_tmp
	rm -f $(TARGET) $(TARGET).exe $(TARGET)_android $(OBJS) \
	      core_load_check_test_a.so \
	      core_load_check_test_b.so core_load_check_test_c.so \
	      core_load_check_test_a.dll core_load_check_test_b.dll \
	      core_load_check_test_c.dll

.PHONY: all check clean
