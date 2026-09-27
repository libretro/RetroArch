# A device list entry "/dev/dsp0 (card)" comes back to init as the
# device string; the path is what gets opened. oss and audioio,
# compiled whole with open() intercepted, and the helper on its own.

compiler := gcc
TARGET   := device_label_path_test

CORE_DIR          = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common

CC      := $(compiler)
CFLAGS  += -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter \
           -Wno-missing-field-initializers -g -O1 -DRARCH_INTERNAL \
           -I$(CORE_DIR) -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)/deps \
           -Ishim -U_FORTIFY_SOURCE
SANITIZER ?=
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

SOURCES := device_label_path_test.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
