# audio/drivers/jack.c against a fake libjack behind dylib_load: the
# link carries no JACK library, so a direct jack_* reference fails it.
# Builds where JACK is not installed (jack/ holds its public API) and
# with mingw-w64:  make CC=x86_64-w64-mingw32-gcc check

include ../../sample_objdir.mk

CC       ?= gcc
TARGET   := jack_runtime_test

CORE_DIR          = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

SOURCES_C := \
	jack_runtime_test.c \
	stubs_retroarch.c \
	$(CORE_DIR)/audio/drivers/jack.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -std=gnu99 -Wall -Werror=incompatible-pointer-types \
           -Werror=implicit-function-declaration
CFLAGS  += -DRARCH_INTERNAL -DHAVE_JACK -DHAVE_DYLIB -DHAVE_THREADS -D_GNU_SOURCE

ifeq ($(findstring mingw,$(CC)),)
   LDFLAGS += -lpthread
   RUN     := ./$(TARGET)
else
   TARGET  := $(TARGET).exe
   RUN     := wine ./$(TARGET)
endif
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))

all: $(TARGET)

%.o: %.c
	$(CC) $(INCDIRS) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(INCDIRS) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: $(TARGET)
	$(RUN)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) jack_runtime_test jack_runtime_test.exe

.PHONY: all clean check
