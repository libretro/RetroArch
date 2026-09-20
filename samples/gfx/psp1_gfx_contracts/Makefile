TARGET   := psp1_gfx_contracts_test

ROOT     := ../../..
STUBS    := $(ROOT)/tools/platform_stubs/psp

# fake_psp.c compiles gfx/drivers/psp1_gfx.c as it ships, against the
# same PSP stub headers the compile matrix builds it with.
SOURCES  := \
	psp1_gfx_contracts_test.c \
	fake_psp.c

OBJS     := $(SOURCES:.c=.o)
DEPS     := $(OBJS:.o=.d)

# fake_psp.c includes a file from the tree, so track that dependency.
CFLAGS  += -Wall -std=gnu99 -g -O1 -MMD -MP \
           -I$(STUBS) -I$(ROOT) -I$(ROOT)/libretro-common/include \
           -I$(ROOT)/deps -I$(ROOT)/deps/rcheevos/include \
           -I$(ROOT)/input/include \
           -DRARCH_INTERNAL -DHAVE_AUDIOMIXER -DHAVE_THREADS \
           -DHAVE_CONFIGFILE -DHAVE_MENU -DPSP \
           -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)

.PHONY: all check clean
