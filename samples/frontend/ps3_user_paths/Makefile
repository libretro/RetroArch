include ../../../libretro-common/samples/sample_objdir.mk

TARGET ?= ps3_user_paths_test
ROOT := ../../..
COMMON := $(ROOT)/libretro-common
SOURCES := ps3_user_paths_test.c \
   $(ROOT)/frontend/drivers/platform_ps3_paths.c \
   $(COMMON)/compat/fopen_utf8.c \
   $(COMMON)/compat/compat_strl.c \
   $(COMMON)/compat/compat_strcasestr.c \
   $(COMMON)/compat/compat_posix_string.c \
   $(COMMON)/encodings/encoding_utf.c \
   $(COMMON)/file/file_path.c \
   $(COMMON)/file/file_path_io.c \
   $(COMMON)/file/config_file.c \
   $(COMMON)/file/config_file_io.c \
   $(COMMON)/lists/string_list.c \
   $(COMMON)/streams/file_stream.c \
   $(COMMON)/string/rstrtod.c \
   $(COMMON)/vfs/vfs_implementation.c \
   $(COMMON)/time/rtime.c
OBJS := $(call sample_objs,$(SOURCES:.c=.o))
CFLAGS += -O2 -Wall -Wextra -std=gnu89 -DIS_SALAMANDER -I$(ROOT) -I$(COMMON)/include

ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f ps3_user_paths_test.o $(TARGET)

.PHONY: all clean
