TARGETS := c_converter_case_test c_converter_missing_key_test

LIBRETRODB_DIR    := ../..
LIBRETRO_COMM_DIR := ../../../libretro-common

SOURCES := \
	$(LIBRETRODB_DIR)/libretrodb.c \
	$(LIBRETRODB_DIR)/rmsgpack.c \
	$(LIBRETRODB_DIR)/rmsgpack_dom.c \
	$(LIBRETRODB_DIR)/bintree.c \
	$(LIBRETRODB_DIR)/query.c \
	$(LIBRETRO_COMM_DIR)/hash/lrc_hash.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_fnmatch.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

OBJS := $(SOURCES:.c=.o) c_converter_main.o

CFLAGS  += -Wall -g \
           -I$(LIBRETRO_COMM_DIR)/include \
           -I$(LIBRETRODB_DIR)
LDFLAGS +=

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGETS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

# The converter's main() is renamed so the test can call it.
c_converter_main.o: $(LIBRETRODB_DIR)/c_converter.c
	$(CC) -c -o $@ $< $(CFLAGS) -Dmain=c_converter_main

c_converter_case_test: c_converter_case_test.o $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

c_converter_missing_key_test: c_converter_missing_key_test.o $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: all
	./c_converter_case_test
	./c_converter_missing_key_test

clean:
	rm -f $(TARGETS) $(OBJS) c_converter_case_test.o \
	      c_converter_missing_key_test.o

.PHONY: clean check
