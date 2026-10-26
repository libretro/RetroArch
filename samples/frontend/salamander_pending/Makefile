# frontend/frontend_salamander.c, the static-platform launcher, linked
# with the libretro-common units a salamander build links (see
# Makefile.vita.salamander) and a stand-in frontend driver, plus the
# frontend's salamander config functions taken out of configuration.c
# by ../android_second_surface/extract.awk. Builds for
# Linux and, with CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for
# Windows.
TARGET   := salamander_pending_test$(EXE)
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common

SOURCES := salamander_pending_test.c \
           $(CORE_DIR)/frontend/frontend_salamander.c \
           $(CORE_DIR)/verbosity.c \
           $(LRC)/file/file_path.c \
           $(LRC)/file/file_path_io.c \
           $(LRC)/string/rstrtod.c \
           $(LRC)/string/stdstring.c \
           $(LRC)/lists/string_list.c \
           $(LRC)/lists/dir_list.c \
           $(LRC)/file/retro_dirent.c \
           $(LRC)/encodings/encoding_utf.c \
           $(LRC)/compat/compat_strl.c \
           $(LRC)/compat/compat_strldup.c \
           $(LRC)/compat/compat_strcasestr.c \
           $(LRC)/compat/fopen_utf8.c \
           $(LRC)/file/config_file.c \
           $(LRC)/file/config_file_io.c \
           $(LRC)/streams/file_stream.c \
           $(LRC)/vfs/vfs_implementation.c \
           $(LRC)/time/rtime.c

CFLAGS  += -std=gnu89 -Wall -g1 -O1 -DIS_SALAMANDER -DRARCH_CONSOLE \
           -DHAVE_MAIN -I$(CORE_DIR) -I$(LRC)/include
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

# The frontend's salamander config functions, out of configuration.c.
salamander_config.h: ../android_second_surface/extract.awk $(CORE_DIR)/configuration.c
	awk -v FUNCS=config_file_salamander_get_path,config_save_file_salamander,config_clear_salamander_pending \
	   -f ../android_second_surface/extract.awk $(CORE_DIR)/configuration.c > $@.tmp
	mv $@.tmp $@

$(TARGET): $(SOURCES) salamander_config.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -rf salamander_pending_test salamander_pending_test.exe \
	       salamander_pending.tmp salamander_config.h salamander_config.h.tmp

.PHONY: all check clean
