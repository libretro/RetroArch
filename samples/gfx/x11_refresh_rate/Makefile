TARGET := x11_refresh_rate_test

# Links the real X display server and the real X event pump
# (gfx/common/x11_common.c) against libX11 and libXrandr, and changes
# the mode on whatever $DISPLAY points at from a second connection.
# Needs a server that honours XRRCreateMode / XRRSetCrtcConfig: the
# Xorg 'dummy' driver (samples/gfx/display_servers_x11_live has the
# config); Xvfb lists no modes to switch between.
REPO_ROOT := ../../..

SOURCES := x11_refresh_rate_test.c \
           $(REPO_ROOT)/gfx/common/x11_common.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_x11.c \
           $(REPO_ROOT)/gfx/common/xinerama_common.c \
           $(REPO_ROOT)/gfx/modeline/modeline_core.c \
           $(REPO_ROOT)/gfx/modeline/modeline_monitor.c \
           $(REPO_ROOT)/gfx/modeline/modeline_list.c \
           $(REPO_ROOT)/gfx/modeline/modeline_edid.c \
           $(REPO_ROOT)/gfx/modeline/modeline_ini.c \
           $(REPO_ROOT)/libretro-common/compat/compat_strl.c \
           $(REPO_ROOT)/libretro-common/encodings/encoding_utf.c \
           $(REPO_ROOT)/libretro-common/rthreads/rthreads.c

CFLAGS  += -Wall -g -O0 -DHAVE_XRANDR -DHAVE_XINERAMA -DHAVE_THREADS \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include

# The modeline engine logs through its standalone macros; everything
# else takes the verbosity.h functions the test defines.
obj/modeline_%.o: CFLAGS += -DMODELINE_STANDALONE -include ../display_servers_x11_live/live_log.h
obj/x11_refresh_rate_test.o: CFLAGS += -std=c89 -pedantic -Wno-long-long

LDLIBS  += -lXrandr -lXinerama -lX11 -lpthread -lm

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
