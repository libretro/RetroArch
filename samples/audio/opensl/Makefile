TARGET := opensl_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The mock <SLES/*.h> is found here, ahead of any system one.
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

SOURCES_C := \
	opensl_test.c \
	opensl_mock.c \
	stubs_retroarch.c \
	../../../audio/drivers/opensl.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_OPENSL -DANDROID -D__ANDROID_API__=21 -DHAVE_CONFIGFILE -DHAVE_MENU -D_GNU_SOURCE
LDFLAGS += -lm -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
