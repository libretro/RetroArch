ROOT   = ../../..
COMMON = $(ROOT)/libretro-common
CFLAGS ?= -O2 -g
CPPFLAGS += -I$(COMMON)/include -D_GNU_SOURCE
SRC = nfs_test.c $(COMMON)/net/net_nfs3.c $(COMMON)/net/net_socket.c $(COMMON)/net/net_compat.c \
      $(COMMON)/features/features_cpu.c $(COMMON)/time/rtime.c $(COMMON)/compat/compat_strl.c \
      $(COMMON)/string/stdstring.c $(COMMON)/encodings/encoding_utf.c
ifneq (,$(findstring mingw,$(CC))$(findstring MINGW,$(MSYSTEM))$(findstring Windows,$(OS)))
   LDLIBS += -lws2_32 -liphlpapi
endif
nfs_test: $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDLIBS) -o $@
clean:
	rm -f nfs_test nfs_test.exe
.PHONY: clean
