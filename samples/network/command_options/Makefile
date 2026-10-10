compiler ?= gcc
ROOT := ../../..
TARGET := command_options_test
SOURCES := command_options_test.c $(ROOT)/command.c $(ROOT)/runloop.c \
   $(ROOT)/libretro-common/string/stdstring.c \
   $(ROOT)/libretro-common/compat/compat_strl.c
CFLAGS += -std=gnu89 -Wall -g -O1 -flto -ffunction-sections -fdata-sections \
   -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps \
   -I$(ROOT)/deps/rcheevos/include -I$(ROOT)/input/include \
   -DRARCH_INTERNAL -DHAVE_COMMAND -DHAVE_NETWORKING -DHAVE_CHEEVOS
LDFLAGS += -flto -Wl,--gc-sections -lm
ifneq ($(SANITIZER),)
   CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifneq ($(findstring mingw,$(compiler)),)
   TARGET := $(TARGET).exe
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(ROOT)/runloop.h $(ROOT)/command.h
	$(compiler) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f command_options_test command_options_test.exe
.PHONY: all check clean
