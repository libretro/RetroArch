compiler     := gcc
TARGET       := cheevos_login_test

CORE_DIR = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

CC      := $(compiler)
flags   += -std=gnu99

# SSL_BACKEND=bear (default, builtin BearSSL from deps/) or mbed
# (system mbedTLS) - the two backends a Linux build can ship with.
SSL_BACKEND ?= bear

SOURCES_C := \
	cheevos_login_test.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_base64.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/net/net_http.c \
	$(LIBRETRO_COMM_DIR)/net/net_compat.c \
	$(LIBRETRO_COMM_DIR)/net/net_socket.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS += -Wall $(INCDIRS)
CFLAGS += -DHAVE_NETWORKING -DHAVE_SSL -D_GNU_SOURCE
LDFLAGS += -lm

ifeq ($(SSL_BACKEND),bear)
   SOURCES_C += bear_seed.c
   BEAR_DIR   = $(CORE_DIR)/deps/bearssl-0.6
   SOURCES_C += $(wildcard $(BEAR_DIR)/src/*/*.c) $(wildcard $(BEAR_DIR)/src/*.c)
   CFLAGS    += -DHAVE_BEARSSL -DTEST_SSL_BEAR
   CFLAGS    += -I$(BEAR_DIR)/src -I$(BEAR_DIR)/inc
else
   # mbed_seed.c #includes the real backend (see the file).
   SOURCES_C += mbed_seed.c
   CFLAGS    += -DHAVE_MBEDTLS
   # MBEDTLS_DIR=<built source tree> tests against that Mbed TLS instead
   # of the system one; CI uses it for a TLS 1.3 capable 3.6.0.
   ifneq ($(MBEDTLS_DIR),)
      CFLAGS  += -I$(MBEDTLS_DIR)/include
      LDFLAGS += -L$(MBEDTLS_DIR)/library
   endif
   LDFLAGS   += -lmbedtls -lmbedx509 -lmbedcrypto
endif

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

# Test PKI: a throwaway CA and a localhost leaf it signs. Generated
# once; both the mock server and the client trust store use it.
fixtures/server.crt:
	mkdir -p fixtures
	openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 \
	   -keyout fixtures/ca.key -out fixtures/ca.crt -days 3650 -nodes \
	   -subj "/CN=cheevos-login-test-ca" \
	   -addext "basicConstraints=critical,CA:TRUE" 2>/dev/null
	openssl req -newkey ec -pkeyopt ec_paramgen_curve:P-256 \
	   -keyout fixtures/server.key -out fixtures/server.csr -nodes \
	   -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost" 2>/dev/null
	openssl x509 -req -in fixtures/server.csr -CA fixtures/ca.crt \
	   -CAkey fixtures/ca.key -CAcreateserial -days 3650 \
	   -copy_extensions copyall -out fixtures/server.crt 2>/dev/null

%.o: %.c
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -c -o$@ $<

$(TARGET): $(OBJS)
	$(CC) -o$@ $(OBJS) $(LDFLAGS)

check: $(TARGET) fixtures/server.crt
	rm -f mock_ra_server.ready
	TEST_CA_PEM=fixtures/ca.crt ./$(TARGET) mock_ra_server.py \
	   fixtures/server.crt fixtures/server.key

clean:
	rm -f $(OBJS) bear_seed.o mbed_seed.o
	rm -f $(TARGET)

.PHONY: clean all check

mbed_seed.o: $(LIBRETRO_COMM_DIR)/net/net_socket_ssl_mbed.c
bear_seed.o: $(LIBRETRO_COMM_DIR)/net/net_socket_ssl_bear.c
