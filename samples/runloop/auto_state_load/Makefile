TARGET := auto_state_load_test
ROOT := ../../..
RUNTIME_TARGET ?= runtime_state_slot_test
SCAN_TARGET ?= rolling_state_scan_test
SOURCES := auto_state_load_test.c $(ROOT)/command.c \
           $(ROOT)/libretro-common/compat/compat_strl.c \
           $(ROOT)/libretro-common/file/file_path.c \
           $(ROOT)/libretro-common/string/stdstring.c
CFLAGS += -O2 -g1 -std=gnu89 -Wall -flto \
          -ffunction-sections -fdata-sections \
          -I$(ROOT) -I$(ROOT)/menu -I$(ROOT)/libretro-common/include $(EXTRA_CFLAGS)
LDFLAGS += -flto -Wl,--gc-sections
RUNTIME_SOURCES := runtime_state_slot_test.c \
                  $(ROOT)/libretro-common/formats/json/rjson.c \
                  $(ROOT)/libretro-common/compat/compat_strl.c \
                  $(ROOT)/libretro-common/encodings/encoding_utf.c \
                  $(ROOT)/libretro-common/string/stdstring.c
SCAN_SOURCES := rolling_state_scan_test.c $(ROOT)/command.c \
                $(ROOT)/libretro-common/compat/compat_strl.c \
                $(ROOT)/libretro-common/compat/compat_strldup.c \
                $(ROOT)/libretro-common/compat/fopen_utf8.c \
                $(ROOT)/libretro-common/encodings/encoding_utf.c \
                $(ROOT)/libretro-common/file/file_path.c \
                $(ROOT)/libretro-common/file/file_path_io.c \
                $(ROOT)/libretro-common/file/retro_dirent.c \
                $(ROOT)/libretro-common/lists/dir_list.c \
                $(ROOT)/libretro-common/lists/string_list.c \
                $(ROOT)/libretro-common/streams/file_stream.c \
                $(ROOT)/libretro-common/string/stdstring.c \
                $(ROOT)/libretro-common/vfs/vfs_implementation.c
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(RUNTIME_TARGET) $(SCAN_TARGET)

$(TARGET): $(SOURCES) $(ROOT)/runloop.c $(ROOT)/runloop.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

$(RUNTIME_TARGET): $(RUNTIME_SOURCES) $(ROOT)/runtime_file.c $(ROOT)/runtime_file.h
	$(CC) $(CFLAGS) $(RUNTIME_SOURCES) -o $@ $(LDFLAGS)

$(SCAN_TARGET): $(SCAN_SOURCES)
	$(CC) $(CFLAGS) $(SCAN_SOURCES) -o $@ $(LDFLAGS)

check: all
	./$(TARGET)
	./$(RUNTIME_TARGET)
	./$(SCAN_TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(RUNTIME_TARGET) $(RUNTIME_TARGET).exe $(SCAN_TARGET) $(SCAN_TARGET).exe

.PHONY: all check clean
