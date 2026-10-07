TARGET := net_retropad_repeat_test

# The Remote RetroPad core itself, driven by a frontend of a few
# callbacks, its messages read back over loopback.
#    make && make check
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

CFLAGS += -Wall -std=gnu99 -g -O1 -I$(LRC)/include

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

SOURCES := net_retropad_repeat_test.c \
   $(REPO_ROOT)/cores/libretro-net-retropad/net_retropad_core.c \
   $(LRC)/net/net_compat.c \
   $(LRC)/net/net_socket.c \
   $(LRC)/compat/compat_strl.c \
   $(LRC)/compat/fopen_utf8.c \
   $(LRC)/compat/compat_posix_string.c \
   $(LRC)/encodings/encoding_utf.c \
   $(LRC)/encodings/encoding_crc32.c \
   $(LRC)/features/features_cpu.c \
   $(LRC)/file/file_path.c \
   $(LRC)/file/file_path_io.c \
   $(LRC)/formats/json/rjson.c \
   $(LRC)/streams/file_stream.c \
   $(LRC)/string/rstrtod.c \
   $(LRC)/streams/interface_stream.c \
   $(LRC)/streams/memory_stream.c \
   $(LRC)/string/stdstring.c \
   $(LRC)/time/rtime.c \
   $(LRC)/vfs/vfs_implementation.c

LIBS := -lm
ifneq ($(findstring mingw,$(CC)),)
   LIBS += -lws2_32 -liphlpapi
endif
ifeq ($(OS),Windows_NT)
   LIBS += -lws2_32 -liphlpapi
endif

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) $(LIBS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
