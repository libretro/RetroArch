include ../../sample_objdir.mk

TARGET := cheat_user_dir_test

REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

SOURCES := cheat_user_dir_test.c \
           $(REPO_ROOT)/cheat_manager.c \
           $(LRC)/compat/fopen_utf8.c \
           $(LRC)/compat/compat_strl.c \
           $(LRC)/compat/compat_strcasestr.c \
           $(LRC)/compat/compat_posix_string.c \
           $(LRC)/encodings/encoding_utf.c \
           $(LRC)/file/file_path.c \
           $(LRC)/file/file_path_io.c \
           $(LRC)/file/config_file.c \
           $(LRC)/file/config_file_io.c \
           $(LRC)/lists/string_list.c \
           $(LRC)/streams/file_stream.c \
           $(LRC)/string/rstrtod.c \
           $(LRC)/vfs/vfs_implementation.c \
           $(LRC)/time/rtime.c

# cheat_manager.h reaches setting_list.h as ../setting_list.h
CFLAGS += -Wall -std=gnu99 -g1 -O0 -DRARCH_INTERNAL -DHAVE_CHEATS \
          -I$(REPO_ROOT) -I$(REPO_ROOT)/menu -I$(LRC)/include

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)$(EXE)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET)$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)$(EXE)
	$(RUNNER) ./$(TARGET)$(EXE)

clean:
	rm -rf $(SAMPLE_OBJDIR) cheat_user_dir_tmp
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean
