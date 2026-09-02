compiler     := gcc
TARGET       := tinyalsa_write_test

CORE_DIR = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

CC      := $(compiler)
flags   += -std=gnu99

# The test includes audio/drivers/tinyalsa.c so the shipping loops run;
# the kernel is reached only through ioctl() and poll(), which are
# scripted via --wrap.
SOURCES_C := \
	tinyalsa_write_test.c \
	stubs_retroarch.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS += -Wall $(INCDIRS)
CFLAGS += -DRARCH_INTERNAL -DHAVE_THREADS -D_GNU_SOURCE
LDFLAGS += -lpthread -Wl,--wrap=ioctl -Wl,--wrap=poll

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

%.o: %.c
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -c -o$@ $<

$(TARGET): $(OBJS)
	$(CC) -o$@ $(OBJS) $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean all check
