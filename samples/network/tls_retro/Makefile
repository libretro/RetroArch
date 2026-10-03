ROOT   = ../../..
COMMON = $(ROOT)/libretro-common
CFLAGS ?= -O2 -g
CPPFLAGS += -I$(COMMON)/include -DHAVE_SSL -DHAVE_RETROSSL -DHAVE_CRYPTO -D_GNU_SOURCE
SRC = tls_fetch.c $(COMMON)/net/net_socket_ssl_retro.c $(COMMON)/net/net_socket.c $(COMMON)/net/net_compat.c \
      $(COMMON)/crypto/crypto.c $(COMMON)/crypto/kdf.c $(COMMON)/crypto/pk.c $(COMMON)/crypto/x509.c \
      $(COMMON)/hash/lrc_hash.c $(COMMON)/utils/md5.c $(COMMON)/encodings/encoding_base64.c $(COMMON)/streams/file_stream.c \
      $(COMMON)/vfs/vfs_implementation.c $(COMMON)/file/file_path.c $(COMMON)/string/rstrtod.c \
      $(COMMON)/file/file_path_io.c $(COMMON)/features/features_cpu.c $(COMMON)/compat/compat_strl.c \
      $(COMMON)/compat/fopen_utf8.c \
      $(COMMON)/time/rtime.c $(COMMON)/string/stdstring.c $(COMMON)/encodings/encoding_utf.c

# Winsock on Windows (MSYS2 / MinGW, or a cross build with a *-w64-mingw32 CC).
ifneq (,$(findstring mingw,$(CC))$(findstring MINGW,$(MSYSTEM))$(findstring Windows,$(OS)))
   LDLIBS += -lws2_32 -liphlpapi
endif

all: tls_fetch tls_http tls_threads

tls_fetch: $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDLIBS) -o $@
# the same tool with no AES instructions to use, as on a 3DS or a Vita
tls_fetch_noaes: $(SRC)
	$(CC) $(CPPFLAGS) -DAES_NO_X86 -DAES_NO_ARM $(CFLAGS) $(SRC) $(LDLIBS) -o $@
tls_bench: tls_bench.c $(filter-out tls_fetch.c,$(SRC))
	$(CC) $(CPPFLAGS) $(CFLAGS) tls_bench.c $(filter-out tls_fetch.c,$(SRC)) $(LDLIBS) -o $@
tls_keyupdate: tls_keyupdate.c $(filter-out tls_fetch.c,$(SRC))
	$(CC) $(CPPFLAGS) $(CFLAGS) tls_keyupdate.c $(filter-out tls_fetch.c,$(SRC)) $(LDLIBS) -o $@
# net_http over the built-in client, as RetroArch's HTTP task drives it
HTTP_SRC = tls_http.c $(filter-out tls_fetch.c,$(SRC)) $(COMMON)/net/net_http.c \
      $(COMMON)/compat/compat_strcasestr.c $(COMMON)/compat/compat_posix_string.c \
      $(COMMON)/lists/string_list.c $(COMMON)/file/retro_dirent.c $(COMMON)/rthreads/rthreads.c \
      $(COMMON)/rthreads/retro_eventcount.c
tls_http: $(HTTP_SRC)
	$(CC) $(CPPFLAGS) -DHAVE_THREADS $(CFLAGS) $(HTTP_SRC) $(LDLIBS) -lpthread -o $@
THR_SRC = tls_threads.c $(filter-out tls_fetch.c,$(SRC)) $(COMMON)/rthreads/rthreads.c
tls_threads: $(THR_SRC)
	$(CC) $(CPPFLAGS) -DHAVE_THREADS $(CFLAGS) $(THR_SRC) $(LDLIBS) -lpthread -o $@
clean:
	rm -f tls_fetch_noaes tls_fetch_noaes.exe tls_http tls_http.exe tls_fetch tls_fetch.exe tls_bench tls_bench.exe tls_keyupdate tls_keyupdate.exe tls_threads tls_threads.exe
.PHONY: all clean
