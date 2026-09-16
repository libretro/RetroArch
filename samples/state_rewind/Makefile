compiler     := gcc
TARGET       := state_rewind_test

CORE_DIR = ../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -I$(CORE_DIR)/deps

CC      := $(compiler)
flags   += -std=gnu99

# The harness includes state_manager.c so it can drive the codec and the
# ring directly; the frontend it calls into is stubbed.
SOURCES_C := \
	state_rewind_test.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c

CFLAGS += -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_REWIND -DHAVE_THREADS -DHAVE_CONFIGFILE -DHAVE_MENU -D_GNU_SOURCE
LDFLAGS += -lm -lpthread

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
