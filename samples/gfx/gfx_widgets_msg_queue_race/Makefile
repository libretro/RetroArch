TARGET := gfx_widgets_msg_queue_race_test

# The test includes gfx/gfx_widgets.c itself, so it drives the
# production ring code rather than a copy.  LTO with section garbage
# collection drops everything in that unit the test does not reach,
# the way tests-other/Makefile.widget_snapshot builds the same unit.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := \
	gfx_widgets_msg_queue_race_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

CFLAGS  += -std=gnu89 -Wall -g -O1 -flto -ffunction-sections -fdata-sections \
           -DHAVE_THREADS -DHAVE_MENU -DHAVE_GFX_WIDGETS \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -flto -Wl,--gc-sections -lpthread
# rthreads.c uses clock_gettime + CLOCK_REALTIME on Linux glibc; on
# older glibc those live in -lrt.  Harmless on newer glibc.
LDFLAGS += -lrt

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
