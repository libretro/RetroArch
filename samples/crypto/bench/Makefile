ROOT   = ../../..
COMMON = $(ROOT)/libretro-common
CFLAGS ?= -O3
CPPFLAGS += -I$(COMMON)/include
SRC = bench.c $(COMMON)/crypto/crypto.c $(COMMON)/crypto/kdf.c $(COMMON)/crypto/pk.c \
      $(COMMON)/hash/lrc_hash.c $(COMMON)/utils/md5.c $(COMMON)/streams/file_stream.c $(COMMON)/vfs/vfs_implementation.c \
      $(COMMON)/file/file_path.c $(COMMON)/string/rstrtod.c $(COMMON)/file/file_path_io.c \
      $(COMMON)/features/features_cpu.c $(COMMON)/compat/compat_strl.c $(COMMON)/time/rtime.c \
      $(COMMON)/string/stdstring.c $(COMMON)/encodings/encoding_utf.c

all: bench

bench: $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) -lmbedcrypto -o $@
run: bench
	./bench
clean:
	rm -f bench
.PHONY: all run clean
