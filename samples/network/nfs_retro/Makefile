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

VFS_SRC = vfs_test.c $(COMMON)/vfs/vfs_implementation_nfs.c $(COMMON)/vfs/vfs_implementation.c \
      $(COMMON)/streams/file_stream.c $(COMMON)/file/retro_dirent.c $(COMMON)/file/file_path.c \
      $(COMMON)/file/file_path_io.c $(COMMON)/string/rstrtod.c $(COMMON)/compat/fopen_utf8.c \
      $(filter-out nfs_test.c,$(SRC))
vfs_test: $(VFS_SRC)
	$(CC) $(CPPFLAGS) -DHAVE_NFSCLIENT -I$(ROOT) $(CFLAGS) $(VFS_SRC) $(LDLIBS) -o $@
THR_SRC = vfs_threads_test.c $(filter-out vfs_test.c,$(VFS_SRC)) $(COMMON)/rthreads/rthreads.c
vfs_threads_test: $(THR_SRC)
	$(CC) $(CPPFLAGS) -DHAVE_NFSCLIENT -DHAVE_THREADS -I$(ROOT) $(CFLAGS) $(THR_SRC) $(LDLIBS) -lpthread -o $@
clean:
	rm -f nfs_test nfs_test.exe vfs_test vfs_test.exe vfs_threads_test vfs_threads_test.exe
.PHONY: clean
