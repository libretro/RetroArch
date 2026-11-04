# Lanes for menu/menu_content_downloads.c, the Content Downloader's
# record of finished downloads.  Files are written under cd_scratch/
# beside this Makefile.
#
#   make check
#   make check SANITIZER=address,undefined

TARGET  := content_downloads_test
CORE_DIR := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES := \
	content_downloads_test.c \
	$(CORE_DIR)/menu/menu_content_downloads.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

# Objects go under obj/, never beside the shared sources, where another
# sample's sanitizer build may have left its own.
OBJDIR := obj
OBJS   := $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(SOURCES)))

CFLAGS  += -Wall -Wsign-compare -std=gnu89 -g -O1 -fno-omit-frame-pointer \
           -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJDIR):
	mkdir -p $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -rf $(OBJDIR) cd_scratch

.PHONY: all check clean
