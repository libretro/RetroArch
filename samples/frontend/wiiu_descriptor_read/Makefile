# filestream_read_file() on the descriptor path with the Wii U's read
# alignment, as Makefile.wiiu builds it with HAVE_WIIU_DESCRIPTOR_IO=1:
# file_stream.c and vfs_implementation.c, the latter's read() calls
# routed to the test's stand-in (glibc's fortified read() wrapper is
# kept out of that unit, or the redirect would rename it). Builds for
# Linux and, with CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for
# Windows.
TARGET   := wiiu_descriptor_read_test$(EXE)
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common

SOURCES := wiiu_descriptor_read_test.c \
           $(LRC)/streams/file_stream.c \
           $(LRC)/file/file_path.c \
           $(LRC)/file/file_path_io.c \
           $(LRC)/string/stdstring.c \
           $(LRC)/string/rstrtod.c \
           $(LRC)/compat/compat_strl.c \
           $(LRC)/compat/fopen_utf8.c \
           $(LRC)/encodings/encoding_utf.c \
           $(LRC)/time/rtime.c

CFLAGS  += -std=gnu89 -Wall -g1 -O1 -I$(LRC)/include \
           -DVFS_HAVE_DESCRIPTOR_IO=1 -DVFS_DESCRIPTOR_READ_ALIGN=64
LDFLAGS += -lm

ifeq ($(EXE),.exe)
   # No memalign there whose block free() takes; the alignment check is
   # the Linux build's.
   CFLAGS += -D'memalign(a,n)=malloc(n)'
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

vfs_implementation.o: $(LRC)/vfs/vfs_implementation.c
	$(CC) $(CFLAGS) -U_FORTIFY_SOURCE -D'read(f,b,n)=test_read(f,b,n)' \
	   -include wiiu_descriptor_read_stub.h -c -o $@ $<

$(TARGET): $(SOURCES) vfs_implementation.o
	$(CC) $(CFLAGS) -o $@ $(SOURCES) vfs_implementation.o $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -f wiiu_descriptor_read_test wiiu_descriptor_read_test.exe \
	      vfs_implementation.o wiiu_descriptor_read_*.tmp

.PHONY: all check clean
