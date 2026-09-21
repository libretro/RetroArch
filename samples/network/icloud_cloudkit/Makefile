TARGET := icloud_cloudkit_test

CORE_DIR          = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

# The test includes network/cloud_sync/icloud.m so it can drive
# the driver as shipped; the C sources are what that translation unit
# needs to link.  Apple's clang with ARC, as the Xcode project builds
# the driver.
SOURCES_M := icloud_cloudkit_test.m
SOURCES_C := \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(SOURCES_M:.m=.o) $(SOURCES_C:.c=.o)

CFLAGS  += -Wall -g $(INCDIRS) -DHAVE_CLOUDSYNC
OBJCFLAGS := $(CFLAGS) -fobjc-arc
LDFLAGS += -framework Foundation -framework CloudKit

ifneq ($(SANITIZER),)
   CFLAGS    := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   OBJCFLAGS := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(OBJCFLAGS)
   LDFLAGS   := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.m
	$(CC) $(OBJCFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
