# libFuzzer harnesses for the network clients' parsers (clang only).
#   make fuzz-all      build all four targets
#   make fuzz-run      run each for FUZZ_SECONDS (default 30)
COMMON  = ../..
CC      = clang
CFLAGS  = -O1 -g -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined \
          -I$(COMMON)/include -I$(COMMON)/.. -D_GNU_SOURCE
FUZZ_SECONDS ?= 30
NET_SRC = $(COMMON)/net/net_socket.c $(COMMON)/net/net_compat.c $(COMMON)/features/features_cpu.c \
          $(COMMON)/time/rtime.c $(COMMON)/compat/compat_strl.c $(COMMON)/string/stdstring.c \
          $(COMMON)/encodings/encoding_utf.c $(COMMON)/streams/file_stream.c $(COMMON)/vfs/vfs_implementation.c \
          $(COMMON)/file/file_path.c $(COMMON)/file/file_path_io.c $(COMMON)/string/rstrtod.c \
          $(COMMON)/compat/fopen_utf8.c
CRYPTO_SRC = $(COMMON)/crypto/crypto.c $(COMMON)/crypto/kdf.c $(COMMON)/crypto/pk.c \
          $(COMMON)/crypto/x509.c $(COMMON)/hash/lrc_hash.c $(COMMON)/utils/md5.c \
          $(COMMON)/encodings/encoding_base64.c

fuzz_tls: net_fuzz.c $(COMMON)/net/net_socket_ssl_retro.c $(NET_SRC) $(CRYPTO_SRC)
	$(CC) $(CFLAGS) -DTARGET_TLS -DHAVE_SSL net_fuzz.c $(COMMON)/net/net_socket_ssl_retro.c $(NET_SRC) $(CRYPTO_SRC) -lpthread -o $@
fuzz_smb: net_fuzz.c $(COMMON)/net/net_smb2.c $(COMMON)/net/net_krb5.c $(NET_SRC) $(CRYPTO_SRC)
	$(CC) $(CFLAGS) -DTARGET_SMB net_fuzz.c $(COMMON)/net/net_smb2.c $(COMMON)/net/net_krb5.c $(NET_SRC) $(CRYPTO_SRC) -lpthread -o $@
fuzz_nfs: net_fuzz.c $(COMMON)/net/net_nfs3.c $(NET_SRC)
	$(CC) $(CFLAGS) -DTARGET_NFS net_fuzz.c $(COMMON)/net/net_nfs3.c $(NET_SRC) -lpthread -o $@
fuzz_x509: net_fuzz.c $(CRYPTO_SRC) $(NET_SRC)
	$(CC) $(CFLAGS) -DTARGET_X509 net_fuzz.c $(CRYPTO_SRC) $(NET_SRC) -lpthread -o $@

fuzz-all: fuzz_tls fuzz_smb fuzz_nfs fuzz_x509

# a seed corpus of real traffic makes the first minutes count
fuzz-run: fuzz-all
	@for t in x509 tls smb nfs; do \
	   mkdir -p corpus_$$t; \
	   echo "== fuzz_$$t for $(FUZZ_SECONDS)s"; \
	   ./fuzz_$$t -max_total_time=$(FUZZ_SECONDS) -max_len=16384 -timeout=10 corpus_$$t 2>&1 | grep -E "DONE|Done|crash|ERROR|SUMMARY|timeout-|Slowest" | tail -3 || exit 1; \
	done

clean:
	rm -f fuzz_tls fuzz_smb fuzz_nfs fuzz_x509 crash-* leak-* timeout-*
.PHONY: fuzz-all fuzz-run clean
