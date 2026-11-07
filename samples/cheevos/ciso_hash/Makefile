TARGET := ciso_hash_test

# rc_hash over a CISO through cheevos/cheevos_ciso.c, against the same
# disc as an ISO.
#    make && make check
REPO_ROOT := ../../..
RC        := $(REPO_ROOT)/deps/rcheevos
LRC       := $(REPO_ROOT)/libretro-common

CFLAGS += -Wall -std=gnu99 -g1 -O1 -I$(RC)/include -I$(LRC)/include \
   -DRC_CLIENT_SUPPORTS_HASH -DRC_NO_THREADS

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifneq ($(findstring undefined,$(SANITIZER)),)
   CFLAGS  += -fno-sanitize-recover=undefined
   LDFLAGS += -fno-sanitize-recover=undefined
endif

SOURCES := ciso_hash_test.c \
   $(REPO_ROOT)/cheevos/cheevos_ciso.c \
   $(LRC)/formats/ciso/ciso.c \
   $(RC)/src/rhash/hash.c \
   $(RC)/src/rhash/hash_disc.c \
   $(RC)/src/rhash/hash_encrypted.c \
   $(RC)/src/rhash/hash_rom.c \
   $(RC)/src/rhash/hash_zip.c \
   $(RC)/src/rhash/cdreader.c \
   $(RC)/src/rhash/aes.c \
   $(RC)/src/rc_util.c \
   $(RC)/src/rc_compat.c \
   $(LRC)/utils/md5.c \
   $(LRC)/streams/file_stream.c \
   $(LRC)/vfs/vfs_implementation.c \
   $(LRC)/file/file_path.c \
   $(LRC)/file/file_path_io.c \
   $(LRC)/string/stdstring.c \
   $(LRC)/string/rstrtod.c \
   $(LRC)/encodings/encoding_utf.c \
   $(LRC)/compat/compat_strl.c \
   $(LRC)/compat/compat_posix_string.c \
   $(LRC)/compat/fopen_utf8.c \
   $(LRC)/features/features_cpu.c \
   $(LRC)/time/rtime.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) -lm

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
