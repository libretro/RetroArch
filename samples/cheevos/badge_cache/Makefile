TARGET := badge_cache_test

# Links the real cheevos/cheevos_badge.c; everything it reaches for
# (image task, texture upload, badge directory, download request) is
# stubbed in stubs.c, so the test can complete loads one step at a
# time and see exactly what the cache hands back.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := badge_cache_test.c \
           stubs.c \
           $(REPO_ROOT)/cheevos/cheevos_badge.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c

OBJECTS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 -DHAVE_MENU -DHAVE_CHEEVOS -DRARCH_INTERNAL \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include \
           -I$(REPO_ROOT)/deps -I$(REPO_ROOT)/deps/rcheevos/include
LDFLAGS += -lm

CFLAGS  += $(EXTRA_CFLAGS)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all check clean
