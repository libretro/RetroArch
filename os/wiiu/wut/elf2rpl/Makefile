# elf2rpl runs on the build host, so it is built with the host's C
# compiler (HOST_CC=... to pick another), never the devkitPPC one.
TARGET   := elf2rpl

LIBRETRO_COMM_DIR := ../../../../libretro-common

HOST_CC  ?= gcc

SOURCES  := main.c \
            $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
            $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
            $(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS   := -O2 -Wall -std=gnu89 -DNDEBUG -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS  :=

ifneq ($(findstring Linux,$(shell uname -a)),)
else ifneq ($(findstring Darwin,$(shell uname -a)),)
else
   TARGET   := $(TARGET).exe
endif

all: $(TARGET)

clean:
	@rm -f $(TARGET)

$(TARGET): $(SOURCES)
	$(HOST_CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

.PHONY: all clean
