# input/input_driver.c's Mapped Port sharing and Plays As, with
# command.c telling the core its controllers, against a stand-in
# frontend.
# Only what the tests reach is linked (-flto, --gc-sections).
CC     ?= cc
ROOT   := ../../..
TARGET := mapped_port_test$(EXE)
SOURCES := mapped_port_test.c $(ROOT)/input/input_driver.c $(ROOT)/command.c \
   $(ROOT)/libretro-common/file/config_file.c \
   $(ROOT)/libretro-common/string/stdstring.c \
   $(ROOT)/libretro-common/encodings/encoding_utf.c \
   $(ROOT)/libretro-common/compat/compat_strl.c \
   $(ROOT)/libretro-common/features/features_cpu.c
HEADERS := $(ROOT)/input/input_driver.h $(ROOT)/input/input_remapping.h \
   $(ROOT)/command.h
CFLAGS += -std=gnu89 -Wall -g1 -O1 -flto -ffunction-sections -fdata-sections \
   -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps \
   -I$(ROOT)/input/include \
   -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_NETWORKING
LDFLAGS += -flto -Wl,--gc-sections -lm

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -f mapped_port_test mapped_port_test.exe

.PHONY: all check clean
