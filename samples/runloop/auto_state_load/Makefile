TARGET := auto_state_load_test
ROOT := ../../..
RUNTIME_TARGET ?= runtime_state_slot_test
SOURCES := auto_state_load_test.c $(ROOT)/command.c \
           $(ROOT)/libretro-common/compat/compat_strl.c
CFLAGS += -O2 -g1 -std=gnu89 -Wall -flto \
          -ffunction-sections -fdata-sections \
          -I$(ROOT) -I$(ROOT)/menu -I$(ROOT)/libretro-common/include $(EXTRA_CFLAGS)
LDFLAGS += -flto -Wl,--gc-sections
RUNTIME_SOURCES := runtime_state_slot_test.c \
                  $(ROOT)/libretro-common/formats/json/rjson.c \
                  $(ROOT)/libretro-common/compat/compat_strl.c \
                  $(ROOT)/libretro-common/encodings/encoding_utf.c \
                  $(ROOT)/libretro-common/string/stdstring.c
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(RUNTIME_TARGET)

$(TARGET): $(SOURCES) $(ROOT)/runloop.c $(ROOT)/runloop.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

$(RUNTIME_TARGET): $(RUNTIME_SOURCES) $(ROOT)/runtime_file.c $(ROOT)/runtime_file.h
	$(CC) $(CFLAGS) $(RUNTIME_SOURCES) -o $@ $(LDFLAGS)

check: all
	./$(TARGET)
	./$(RUNTIME_TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(RUNTIME_TARGET) $(RUNTIME_TARGET).exe

.PHONY: all check clean
