TARGET := overlay_textures_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The real page/texture code and the real gfx_surface; the video
# driver is the stub in stubs_retroarch.c. Built without HAVE_THREADS:
# the synchronous upload is the path that regressed.
SOURCES := \
	overlay_textures_test.c \
	stubs_retroarch.c \
	$(REPO_ROOT)/input/input_overlay_textures.c \
	$(REPO_ROOT)/input/input_overlay_alpha.c \
	$(REPO_ROOT)/gfx/gfx_surface.c \
	$(LIBRETRO_COMM_DIR)/formats/image_texture.c

OBJS := $(SOURCES:.c=.o)

# image_texture.c is here for image_texture_free() alone; its loaders
# are dropped at the link rather than chased through the transfer code.
CFLAGS  += -Wall -std=gnu99 -g -O1 -DHAVE_OVERLAY -ffunction-sections \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -Wl,--gc-sections -lm

# THREADS=1 puts the threaded wrapper's stub in (stubs_retroarch.c) and
# adds the lanes that decide the race between the posts and the poll.
ifeq ($(THREADS),1)
   CFLAGS += -DHAVE_THREADS
endif

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
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
