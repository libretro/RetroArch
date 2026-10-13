include ../../sample_objdir.mk

TARGET := cdrom_cuesheet_overflow_test

LIBRETRO_COMM_DIR := ../../..

# vfs_implementation_cdrom.c pulls in cdrom.c transitively for its
# helper functions (cdrom_read, cdrom_write_cue, etc.).  We only
# exercise the cuesheet read path, but the source file must link
# against the full cdrom helper library plus file_path for
# path_get_extension.
SOURCES := \
	cdrom_cuesheet_overflow_test.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/cdrom/cdrom.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/lists/dir_list.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_fnmatch.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation_cdrom.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -DHAVE_CDROM -I$(LIBRETRO_COMM_DIR)/include

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir.  Without this block the variable was accepted and
# ignored, so these targets built unsanitized while the workflow
# reported them as checked.
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif


LDLIBS += -lm

DVD_SOURCES := dvd_blocks_test.c $(filter-out cdrom_cuesheet_overflow_test.c $(LIBRETRO_COMM_DIR)/cdrom/cdrom.c $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation_cdrom.c,$(SOURCES))
DVD_OBJS := $(SAMPLE_OBJDIR)/dvd_blocks_test.o $(call sample_objs,$(filter-out dvd_blocks_test.o,$(DVD_SOURCES:.c=.o)))

ifeq ($(OS),Windows_NT)
   LDLIBS += -lwinmm
endif

all: $(TARGET)

$(SAMPLE_OBJDIR)/dvd_blocks_test.o: dvd_blocks_test.c $(LIBRETRO_COMM_DIR)/cdrom/cdrom.c $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation_cdrom.c $(LIBRETRO_COMM_DIR)/include/vfs/vfs.h
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

dvd_blocks_test$(EXE): $(DVD_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)


%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) dvd_blocks_test$(EXE) $(OBJS) $(DVD_OBJS)

.PHONY: clean
