TARGET := x11_monitors_test

# Links the real X11 monitor follower against libX11, libXinerama and
# libXrandr.  run.sh starts a fresh Xvfb for each case.
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

SOURCES := x11_monitors_test.c \
           $(REPO_ROOT)/gfx/common/x11_monitors.c \
           $(LRC)/rthreads/rthreads.c \
           $(LRC)/compat/compat_strl.c

CFLAGS  += -Wall -std=gnu99 -g1 -O0 \
           -D_GNU_SOURCE -DHAVE_THREADS -DHAVE_XINERAMA -DHAVE_XRANDR -DHAVE_VIDEO_WINDOWS \
           -I$(REPO_ROOT) -I$(LRC)/include

LDLIBS  += -lXrandr -lXinerama -lX11 -lpthread

# Objects under obj/, never next to the sources: other samples build
# some of the same files with other flags.
OBJS  := $(addprefix obj/,$(notdir $(SOURCES:.c=.o)))
VPATH := $(sort $(dir $(SOURCES)))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

obj/%.o: %.c
	@mkdir -p obj
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(TARGET) obj

.PHONY: clean
