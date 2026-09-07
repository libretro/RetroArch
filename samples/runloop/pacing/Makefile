compiler     := gcc
TARGET       := pacing_test

CORE_DIR = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) \
	-I$(CORE_DIR)/deps -I$(CORE_DIR)/deps/stb

CC      := $(compiler)
flags   += -std=gnu99

# runloop.h and nothing else: the three decisions under test are inline
# functions of their arguments, with no state and no clock, so the test
# needs no stubs and links against nothing.
SOURCES_C := \
	pacing_test.c

CFLAGS  += -Wall -Wextra -Wno-unused-parameter

ifneq ($(DEBUG),)
   CFLAGS += -O0 -g
else
   CFLAGS += -O2
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
