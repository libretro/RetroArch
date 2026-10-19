compiler ?= gcc
ROOT := ../../..
TARGET := command_options_test
TEARDOWN_TARGET := command_teardown_test
SOURCES := command_options_test.c $(ROOT)/command.c $(ROOT)/runloop.c \
   $(ROOT)/libretro-common/string/stdstring.c \
   $(ROOT)/libretro-common/compat/compat_strl.c \
   $(ROOT)/libretro-common/features/features_cpu.c
CFLAGS += -std=gnu89 -Wall -g1 -O1 -flto -ffunction-sections -fdata-sections \
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
   TEARDOWN_TARGET := $(TEARDOWN_TARGET).exe
endif

all: $(TARGET) $(TEARDOWN_TARGET)
$(TARGET): $(SOURCES) $(ROOT)/runloop.h $(ROOT)/command.h
	$(compiler) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)
$(TEARDOWN_TARGET): command_teardown_test.c $(ROOT)/input/input_driver.c $(ROOT)/input/input_driver.h
	$(compiler) $(CFLAGS) command_teardown_test.c $(ROOT)/input/input_driver.c -o $@ $(LDFLAGS)
check: all
	$(RUNNER) ./$(TARGET)
	$(RUNNER) ./$(TEARDOWN_TARGET)
clean:
	rm -f command_options_test command_options_test.exe
	rm -f command_teardown_test command_teardown_test.exe
.PHONY: all check clean
