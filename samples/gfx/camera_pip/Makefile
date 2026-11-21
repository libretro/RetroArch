include ../../sample_objdir.mk

TARGET := camera_pip_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The widget's source is included by the test; the camera preview,
# the live thumbnail and the draw are stood in for there.
SOURCES := camera_pip_test.c

CFLAGS  += -Wall -std=gnu99 -g1 -O1 \
           -DHAVE_THREADS -DHAVE_MENU -DHAVE_GFX_WIDGETS \
           -I$(REPO_ROOT) \
           -I$(LIBRETRO_COMM_DIR)/include

LDFLAGS += -lm

CFLAGS += $(EXTRA_CFLAGS)

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean
