include ../../sample_objdir.mk

TARGET       := r7z_lzma_props_test
TARGET_TEST2 := r7z_archive_detach_test
TARGET_TEST3 := r7z_bcj2_test
TARGET_TEST4 := r7z_stored_test
TARGET_TEST5 := r7z_methods_test
TARGET_TEST6 := r7z_stream_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	r7z_lzma_props_test.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma.c

ARCHIVE_SOURCES := \
	r7z_archive_detach_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_archive.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_bcj2.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_filters.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma2.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma_stream.c \
	$(LIBRETRO_COMM_DIR)/formats/rar/rrar_ppmd7.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_rbzip2.c

OBJS         := $(call sample_objs,$(SOURCES:.c=.o))
ARCHIVE_OBJS := $(call sample_objs,$(ARCHIVE_SOURCES:.c=.o))

# BCJ2 extraction, with r7z_archive.c's allocations counted
BCJ2_OBJS    := r7z_bcj2_test.o $(SAMPLE_OBJDIR)/counted/r7z_archive.o \
                $(filter-out r7z_archive_detach_test.o %/r7z_archive.o,$(ARCHIVE_OBJS))

# a solid folder's entries streamed, allocations counted
STREAM_OBJS  := r7z_stream_test.o $(SAMPLE_OBJDIR)/counted/r7z_archive.o \
                $(filter-out r7z_archive_detach_test.o %/r7z_archive.o,$(ARCHIVE_OBJS))

# stored folders, from memory and through reads
STORED_OBJS  := r7z_stored_test.o \
                $(filter-out r7z_archive_detach_test.o,$(ARCHIVE_OBJS))

# PPMd, BZip2, Deflate and Deflate64
METHODS_OBJS := r7z_methods_test.o \
                $(filter-out r7z_archive_detach_test.o,$(ARCHIVE_OBJS))

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -I$(LIBRETRO_COMM_DIR)/include -DHAVE_RBZIP2

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET_TEST4) $(TARGET_TEST5) $(TARGET_TEST6)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET_TEST2): $(ARCHIVE_OBJS)
	$(CC) $(ARCHIVE_OBJS) $(LDFLAGS) -o $@

r7z_bcj2_test.o: r7z_bcj2_test.c r7z_bcj2_fixtures.h

$(SAMPLE_OBJDIR)/counted/r7z_archive.o: $(LIBRETRO_COMM_DIR)/formats/7z/r7z_archive.c r7z_counted_alloc.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -include r7z_counted_alloc.h -DR7Z_STREAM_SLICE=4096 \
	   -DR7Z_STREAM_CHUNK=256 -DR7Z_STREAM_KEEP=1100 \
	   -DR7Z_BZIP2_RING=2560 -DR7Z_BCJ2_SIDE_PIECE=16 -c $< -o $@

$(TARGET_TEST3): $(BCJ2_OBJS)
	$(CC) $(BCJ2_OBJS) $(LDFLAGS) -o $@

r7z_stored_test.o: r7z_stored_test.c r7z_stored_fixtures.h

$(TARGET_TEST4): $(STORED_OBJS)
	$(CC) $(STORED_OBJS) $(LDFLAGS) -o $@

r7z_methods_test.o: r7z_methods_test.c r7z_methods_fixtures.h

r7z_stream_test.o: r7z_stream_test.c ../../file/archive_7z/archive_7z_fixtures.h \
   r7z_filter_fixtures.h r7z_methods_fixtures.h

$(TARGET_TEST6): $(STREAM_OBJS)
	$(CC) $(STREAM_OBJS) $(LDFLAGS) -o $@

$(TARGET_TEST5): $(METHODS_OBJS)
	$(CC) $(METHODS_OBJS) $(LDFLAGS) -o $@

check: $(TARGET) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET_TEST4) $(TARGET_TEST5) $(TARGET_TEST6)
	./$(TARGET)
	./$(TARGET_TEST2)
	./$(TARGET_TEST3)
	./$(TARGET_TEST4)
	./$(TARGET_TEST5)
	./$(TARGET_TEST6)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET_TEST4) $(TARGET_TEST5) $(TARGET_TEST6) $(OBJS) $(ARCHIVE_OBJS) r7z_bcj2_test.o r7z_stored_test.o r7z_methods_test.o r7z_stream_test.o

.PHONY: all check clean
