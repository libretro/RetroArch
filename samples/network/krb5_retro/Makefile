ROOT   = ../../..
COMMON = $(ROOT)/libretro-common
CFLAGS ?= -O2 -g
CPPFLAGS += -I$(COMMON)/include -D_GNU_SOURCE
SRC = krb5_test.c $(COMMON)/net/net_krb5.c $(COMMON)/net/net_socket.c $(COMMON)/net/net_compat.c \
      $(COMMON)/crypto/crypto.c $(COMMON)/crypto/kdf.c $(COMMON)/utils/md5.c $(COMMON)/hash/lrc_hash.c \
      $(COMMON)/features/features_cpu.c $(COMMON)/time/rtime.c $(COMMON)/compat/compat_strl.c \
      $(COMMON)/string/stdstring.c $(COMMON)/encodings/encoding_utf.c $(COMMON)/streams/file_stream.c \
      $(COMMON)/vfs/vfs_implementation.c $(COMMON)/file/file_path.c $(COMMON)/file/file_path_io.c \
      $(COMMON)/string/rstrtod.c $(COMMON)/compat/fopen_utf8.c
ifneq (,$(findstring mingw,$(CC))$(findstring MINGW,$(MSYSTEM))$(findstring Windows,$(OS)))
   LDLIBS += -lws2_32 -liphlpapi -lbcrypt
endif
krb5_test: $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDLIBS) -o $@
clean:
	rm -f krb5_test krb5_test.exe
.PHONY: clean
