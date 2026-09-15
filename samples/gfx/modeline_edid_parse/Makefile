TARGET := modeline_edid_parse_test

# Path back to the repo root from this sample dir.  The test includes
# the modeline engine sources directly, the way tools/modeline_gold.c
# does, so the EDID reader under test is the shipped one.
REPO_ROOT := ../../..

SOURCES := modeline_edid_parse_test.c \
           $(REPO_ROOT)/libretro-common/compat/compat_strl.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include \
           -I$(REPO_ROOT)/deps

LDFLAGS += -lm

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
