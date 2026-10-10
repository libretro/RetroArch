TARGET := auto_state_load_test
ROOT := ../../..
SOURCES := auto_state_load_test.c $(ROOT)/command.c \
           $(ROOT)/libretro-common/compat/compat_strl.c
CFLAGS += -O2 -g -std=gnu89 -Wall -flto \
          -ffunction-sections -fdata-sections \
          -I$(ROOT) -I$(ROOT)/menu -I$(ROOT)/libretro-common/include $(EXTRA_CFLAGS)
LDFLAGS += -flto -Wl,--gc-sections
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) $(ROOT)/runloop.c $(ROOT)/runloop.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
