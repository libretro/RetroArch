TARGET := keymaps_lut_test

REPO_ROOT := ../../..

# input/input_keymaps.c is compiled into the test (it reaches the
# file's static table); no platform's key headers are pulled in.
CFLAGS += -Wall -Wno-unused-function -std=gnu99 -g -O1 -DRARCH_INTERNAL \
   -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)

SOURCES := keymaps_lut_test.c \
           $(REPO_ROOT)/libretro-common/compat/compat_strl.c

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) $(REPO_ROOT)/input/input_keymaps.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
