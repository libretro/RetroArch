TARGET := rsx_alloc_test

# Compiles gfx/drivers/rsx_gfx.c in whole (the test #includes it, to
# reach its static functions) against the tree's PSL1GHT stand-ins,
# with the RSX calls it makes defined by the test: no console, no
# toolchain. The defines are the PSL1GHT build's (Makefile.psl1ght)
# that the driver tests.
REPO_ROOT := ../../..

SOURCES := rsx_alloc_test.c

CFLAGS  += -Wall -Wno-unused-function -std=gnu99 -g -O0 \
           -D__PS3__ -D__PSL1GHT__ -DRARCH_INTERNAL \
           -DRARCH_CONSOLE -DHAVE_THREADS -DHAVE_MENU -DHAVE_MENU_BUFFER \
           -DHAVE_OVERLAY -DHAVE_GFX_WIDGETS -DHAVE_SYSUTILS \
           -I$(REPO_ROOT)/tools/platform_stubs/ps3 \
           -I$(REPO_ROOT)/tools/platform_stubs/psl1ght \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include \
           -I$(REPO_ROOT)/deps

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

LDFLAGS += -lm

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean
