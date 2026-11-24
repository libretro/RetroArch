include ../../sample_objdir.mk

# one line: the samples workflow reads the target list from it
TARGETS := local_utf8_test local_utf8_posix_test

LIBRETRO_COMM_DIR := ../../..

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include
# On the link line rather than in LDFLAGS, which a make command line
# replaces: the test counts encoding_utf.c's allocations through it
WRAP := -Wl,--wrap=calloc

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# encoding_utf.c's Windows path, against shim/windows.h
WIN_FLAGS := -D_WIN32 -Ishim
WIN_OBJS  := $(SAMPLE_OBJDIR)/win/local_utf8_test.o \
	$(SAMPLE_OBJDIR)/win/encoding_utf.o $(SAMPLE_OBJDIR)/win/shim_windows.o \
	$(SAMPLE_OBJDIR)/win/compat_strl.o
POSIX_OBJS := $(SAMPLE_OBJDIR)/posix/local_utf8_test.o \
	$(SAMPLE_OBJDIR)/posix/encoding_utf.o $(SAMPLE_OBJDIR)/posix/compat_strl.o

all: $(TARGETS)

$(SAMPLE_OBJDIR)/win/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) $(WIN_FLAGS)
$(SAMPLE_OBJDIR)/win/encoding_utf.o: $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) $(WIN_FLAGS)
$(SAMPLE_OBJDIR)/win/compat_strl.o: $(LIBRETRO_COMM_DIR)/compat/compat_strl.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) $(WIN_FLAGS)

$(SAMPLE_OBJDIR)/posix/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)
$(SAMPLE_OBJDIR)/posix/encoding_utf.o: $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)
$(SAMPLE_OBJDIR)/posix/compat_strl.o: $(LIBRETRO_COMM_DIR)/compat/compat_strl.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

local_utf8_test: $(WIN_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(WRAP)

local_utf8_posix_test: $(POSIX_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(WRAP)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGETS)

.PHONY: all clean
