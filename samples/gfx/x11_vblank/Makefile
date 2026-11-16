TARGET := x11_vblank_test

# Links the real X display server against libX11 and libxcb and reads
# the vblanks of a window's CRTC on whatever $DISPLAY points at. Any
# server with the Present extension will do: Xvfb and the Xorg 'dummy'
# driver both count vblanks off a timer.
REPO_ROOT := ../../..

SOURCES := x11_vblank_test.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_x11.c \
           $(REPO_ROOT)/gfx/gfx_media_display.c \
           $(REPO_ROOT)/libretro-common/features/features_cpu.c \
           $(REPO_ROOT)/libretro-common/compat/compat_strl.c

CFLAGS  += -Wall -g1 -O0 -DHAVE_XCB \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include

obj/x11_vblank_test.o: CFLAGS += -std=gnu89 -Werror=declaration-after-statement

LDLIBS  += -lX11 -lxcb -lm

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

.PHONY: all clean
