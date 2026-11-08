include ../../sample_objdir.mk

TARGET := core_signature_test

# Core signature checks: .sig parsing and verification against
# fixtures from tools/core_sign.py (gen_fixtures.py).
# Builds on Linux and with mingw-w64.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := core_signature_test.c \
           $(REPO_ROOT)/core_signature.c \
           $(LIBRETRO_COMM_DIR)/crypto/pk.c \
           $(LIBRETRO_COMM_DIR)/crypto/crypto.c \
           $(LIBRETRO_COMM_DIR)/crypto/kdf.c \
           $(LIBRETRO_COMM_DIR)/utils/md5.c \
           $(LIBRETRO_COMM_DIR)/hash/lrc_hash.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
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
          -DHAVE_CRYPTO -DHAVE_THREADS

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

# libretro-common is built as the tree builds it; core_signature*.c and
# the test as C89
STRICT_OBJS  := core_signature_test.o \
                $(call sample_objs,$(REPO_ROOT)/core_signature.o)
STRICT_FLAGS := -std=c89 -pedantic -Wno-long-long \
                -Wdeclaration-after-statement -Wframe-larger-than=4096 -Werror
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
# boolean.h uses the compiler _Bool spelling in C89 builds.
STRICT_FLAGS += -Wno-c99-extensions -Wno-c11-extensions
endif
$(STRICT_OBJS): CFLAGS += $(STRICT_FLAGS)

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
	$(RUNNER) ./$(TARGET)$(EXE)

clean:
	rm -rf $(SAMPLE_OBJDIR) core_signature_test_tmp
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean
