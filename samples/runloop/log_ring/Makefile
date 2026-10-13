compiler ?= gcc
ROOT := ../../..
LC := $(ROOT)/libretro-common
TARGET := log_ring_test
SOURCES := log_ring_test.c $(ROOT)/verbosity.c \
   $(LC)/rthreads/rthreads.c $(LC)/compat/compat_strl.c \
   $(LC)/file/file_path.c $(LC)/file/file_path_io.c \
   $(LC)/streams/file_stream.c $(LC)/vfs/vfs_implementation.c \
   $(LC)/string/stdstring.c $(LC)/encodings/encoding_utf.c \
   $(LC)/compat/fopen_utf8.c $(LC)/time/rtime.c $(LC)/string/rstrtod.c
CFLAGS += -std=gnu89 -Wall -g1 -O1 -I$(ROOT) -I$(LC)/include -DHAVE_MCP \
   -DHAVE_THREADS
LDFLAGS += -lpthread
ifneq ($(SANITIZER),)
   CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifneq ($(findstring mingw,$(compiler)),)
   TARGET := $(TARGET).exe
   LDFLAGS := $(filter-out -lpthread,$(LDFLAGS))
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(ROOT)/verbosity.h
	$(compiler) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)
check: $(TARGET)
	./$(TARGET) 2>/dev/null
clean:
	rm -f log_ring_test log_ring_test.exe
.PHONY: all check clean
