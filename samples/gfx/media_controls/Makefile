CC ?= cc
THREADS ?= 1
TARGET := media_controls_test
CFLAGS += -O2 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wno-unused-parameter -Wframe-larger-than=4096 -D_GNU_SOURCE -I../../.. -I../../../libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
LRC := ../../../libretro-common
SOURCES := media_controls_test.c ../../../gfx/gfx_media_display.c ../../../gfx/gfx_media_controls.c ../../../menu/menu_media_controls.c \
 ../../../menu/menu_media_browser.c ../../../libretro-common/lists/file_list.c \
 ../../../libretro-common/compat/compat_strldup.c ../../../libretro-common/compat/compat_strl.c \
 ../../../libretro-common/compat/compat_strcasestr.c
SOURCES += ../../../gfx/gfx_media_subtitle.c $(LRC)/formats/subtitle/rsubtitle_srt.c $(LRC)/formats/subtitle/rsubtitle_vtt.c \
           $(LRC)/formats/subtitle/rsubtitle_vtt_css.c $(LRC)/formats/subtitle/rsubtitle_vtt_css_tree.c \
           $(LRC)/formats/subtitle/rsubtitle_vtt_selector.c \
           $(LRC)/string/stdstring.c $(LRC)/encodings/encoding_utf.c
ifeq ($(THREADS),1)
CFLAGS += -DHAVE_THREADS
SOURCES += ../../../libretro-common/rthreads/rthreads.c
ifneq ($(OS),Windows_NT)
LDFLAGS += -lpthread
endif
endif
ifeq ($(OS),Windows_NT)
TARGET := $(TARGET).exe
LDFLAGS += -lwinmm
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
LDFLAGS += -lm
all: $(TARGET)
$(TARGET): $(SOURCES) ../../../gfx/gfx_media_controls.h \
           $(LRC)/formats/subtitle/rsubtitle_vtt_internal.h \
           $(LRC)/formats/subtitle/rsubtitle_languages.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)
check: all
	./$(TARGET)
clean:
	rm -f media_controls_test media_controls_test.exe
.PHONY: all check clean
