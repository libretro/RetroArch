ROOT   = ../../..
COMMON = $(ROOT)/libretro-common
CFLAGS ?= -O2 -g
CPPFLAGS += -I$(COMMON)/include -DHAVE_CRYPTO -D_GNU_SOURCE
SRC = smb_test.c $(COMMON)/net/net_smb2.c $(COMMON)/net/net_krb5.c $(COMMON)/net/net_socket.c $(COMMON)/net/net_compat.c \
      $(COMMON)/crypto/crypto.c $(COMMON)/crypto/kdf.c $(COMMON)/hash/lrc_hash.c $(COMMON)/utils/md5.c $(COMMON)/streams/file_stream.c \
      $(COMMON)/vfs/vfs_implementation.c $(COMMON)/file/file_path.c $(COMMON)/string/rstrtod.c $(COMMON)/file/file_path_io.c $(COMMON)/time/rtime.c \
      $(COMMON)/features/features_cpu.c $(COMMON)/compat/compat_strl.c $(COMMON)/compat/fopen_utf8.c \
      $(COMMON)/string/stdstring.c $(COMMON)/encodings/encoding_utf.c
ifneq (,$(findstring mingw,$(CC))$(findstring MINGW,$(MSYSTEM))$(findstring Windows,$(OS)))
   LDLIBS += -lws2_32 -liphlpapi
endif
smb_test: $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDLIBS) -o $@

VFS_SRC = vfs_test.c $(COMMON)/vfs/vfs_implementation_smb.c $(COMMON)/vfs/vfs_prefetch.c $(filter-out smb_test.c,$(SRC)) \
      $(COMMON)/lists/string_list.c
vfs_test: $(VFS_SRC)
	$(CC) $(CPPFLAGS) -DHAVE_RETROSMB -DHAVE_SMBCLIENT -DHAVE_NETWORKING -DRARCH_INTERNAL -I$(ROOT) $(CFLAGS) $(VFS_SRC) $(LDLIBS) -o $@
IDLE_SRC = smb_idle_test.c $(filter-out vfs_test.c,$(VFS_SRC))
smb_idle_test: $(IDLE_SRC)
	$(CC) $(CPPFLAGS) -DHAVE_RETROSMB -DHAVE_SMBCLIENT -DHAVE_NETWORKING -DRARCH_INTERNAL -I$(ROOT) $(CFLAGS) $(IDLE_SRC) $(LDLIBS) -o $@
THR_SRC = vfs_threads_test.c $(filter-out vfs_test.c,$(VFS_SRC)) $(COMMON)/rthreads/rthreads.c $(COMMON)/rthreads/retro_eventcount.c $(COMMON)/file/retro_dirent.c
vfs_threads_test: $(THR_SRC)
	$(CC) $(CPPFLAGS) -DHAVE_RETROSMB -DHAVE_SMBCLIENT -DHAVE_NETWORKING -DHAVE_THREADS -DRARCH_INTERNAL -I$(ROOT) $(CFLAGS) $(THR_SRC) $(LDLIBS) -lpthread -o $@
# The same VFS tests over a system libsmb2 in place of the built-in
# client (the --enable-libsmb build): the backend with the library
# behind the HAVE_SMBCLIENT surface, no crypto, no built-in client.
LIBSMB2_CFLAGS := $(shell pkg-config --cflags libsmb2 2>/dev/null)
LIBSMB2_LIBS   := $(shell pkg-config --libs libsmb2 2>/dev/null || echo -lsmb2)
L2_SRC = $(COMMON)/vfs/vfs_implementation_smb.c $(COMMON)/vfs/vfs_prefetch.c $(COMMON)/lists/string_list.c \
      $(filter-out smb_test.c $(COMMON)/net/net_smb2.c $(COMMON)/net/net_krb5.c $(COMMON)/crypto/crypto.c $(COMMON)/crypto/kdf.c $(COMMON)/hash/lrc_hash.c $(COMMON)/utils/md5.c,$(SRC))
vfs_test_libsmb2: vfs_test.c $(L2_SRC)
	$(CC) -I$(COMMON)/include -D_GNU_SOURCE -DHAVE_SMBCLIENT -DHAVE_STDINT_H -DHAVE_NETWORKING -DRARCH_INTERNAL -I$(ROOT) $(LIBSMB2_CFLAGS) $(CFLAGS) vfs_test.c $(L2_SRC) $(LIBSMB2_LIBS) $(LDLIBS) -o $@
smb_idle_test_libsmb2: smb_idle_test.c $(L2_SRC)
	$(CC) -I$(COMMON)/include -D_GNU_SOURCE -DHAVE_SMBCLIENT -DHAVE_STDINT_H -DHAVE_NETWORKING -DRARCH_INTERNAL -I$(ROOT) $(LIBSMB2_CFLAGS) $(CFLAGS) smb_idle_test.c $(L2_SRC) $(LIBSMB2_LIBS) $(LDLIBS) -o $@
vfs_threads_test_libsmb2: vfs_threads_test.c $(L2_SRC)
	$(CC) -I$(COMMON)/include -D_GNU_SOURCE -DHAVE_SMBCLIENT -DHAVE_STDINT_H -DHAVE_NETWORKING -DHAVE_THREADS -DRARCH_INTERNAL -I$(ROOT) $(LIBSMB2_CFLAGS) $(CFLAGS) vfs_threads_test.c $(L2_SRC) $(COMMON)/rthreads/rthreads.c $(COMMON)/rthreads/retro_eventcount.c $(COMMON)/file/retro_dirent.c $(LIBSMB2_LIBS) $(LDLIBS) -lpthread -o $@

clean:
	rm -f smb_idle_test smb_idle_test.exe smb_idle_test_libsmb2 smb_test smb_test.exe vfs_test vfs_test.exe vfs_threads_test vfs_threads_test.exe vfs_test_libsmb2 vfs_threads_test_libsmb2
.PHONY: clean
