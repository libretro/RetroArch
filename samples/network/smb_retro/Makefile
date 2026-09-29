ROOT   = ../../..
COMMON = $(ROOT)/libretro-common
CFLAGS ?= -O2 -g
CPPFLAGS += -I$(COMMON)/include -DHAVE_CRYPTO -D_GNU_SOURCE
SRC = smb_test.c $(COMMON)/net/net_smb2.c $(COMMON)/net/net_socket.c $(COMMON)/net/net_compat.c \
      $(COMMON)/crypto/crypto.c $(COMMON)/crypto/kdf.c $(COMMON)/hash/lrc_hash.c $(COMMON)/utils/md5.c $(COMMON)/streams/file_stream.c \
      $(COMMON)/vfs/vfs_implementation.c $(COMMON)/file/file_path.c $(COMMON)/string/rstrtod.c $(COMMON)/file/file_path_io.c $(COMMON)/time/rtime.c \
      $(COMMON)/features/features_cpu.c $(COMMON)/compat/compat_strl.c $(COMMON)/compat/fopen_utf8.c \
      $(COMMON)/string/stdstring.c $(COMMON)/encodings/encoding_utf.c
ifneq (,$(findstring mingw,$(CC))$(findstring MINGW,$(MSYSTEM))$(findstring Windows,$(OS)))
   LDLIBS += -lws2_32 -liphlpapi
endif
smb_test: $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDLIBS) -o $@

VFS_SRC = vfs_test.c $(COMMON)/vfs/vfs_implementation_smb.c $(filter-out smb_test.c,$(SRC)) \
      $(COMMON)/lists/string_list.c
vfs_test: $(VFS_SRC)
	$(CC) $(CPPFLAGS) -DHAVE_RETROSMB -DHAVE_SMBCLIENT -DHAVE_NETWORKING -DRARCH_INTERNAL -I$(ROOT) $(CFLAGS) $(VFS_SRC) $(LDLIBS) -o $@
THR_SRC = vfs_threads_test.c $(filter-out vfs_test.c,$(VFS_SRC)) $(COMMON)/rthreads/rthreads.c $(COMMON)/file/retro_dirent.c
vfs_threads_test: $(THR_SRC)
	$(CC) $(CPPFLAGS) -DHAVE_RETROSMB -DHAVE_SMBCLIENT -DHAVE_NETWORKING -DHAVE_THREADS -DRARCH_INTERNAL -I$(ROOT) $(CFLAGS) $(THR_SRC) $(LDLIBS) -lpthread -o $@
clean:
	rm -f smb_test smb_test.exe vfs_test vfs_test.exe vfs_threads_test vfs_threads_test.exe
.PHONY: clean
