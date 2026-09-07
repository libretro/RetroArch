TARGET := display_servers_x11_live_test

# Links the real X11 display server and the real modeline engine
# against the real libX11 and libXrandr, and drives a mode switch on
# whatever $DISPLAY points at.  Meant for an Xorg 'dummy' driver
# server (xserver-xorg-video-dummy), which accepts XRRCreateMode /
# XRRAddOutputMode / XRRSetCrtcConfig with arbitrary timings; Xvfb's
# RandR shim silently drops created modes and is not a valid target.
REPO_ROOT := ../../..

SOURCES := display_servers_x11_live_test.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_x11.c \
           $(REPO_ROOT)/gfx/common/xinerama_common.c \
           $(REPO_ROOT)/gfx/modeline/modeline_core.c \
           $(REPO_ROOT)/gfx/modeline/modeline_monitor.c \
           $(REPO_ROOT)/gfx/modeline/modeline_list.c \
           $(REPO_ROOT)/gfx/modeline/modeline_ini.c \
           $(REPO_ROOT)/libretro-common/compat/compat_strl.c

# MODELINE_STANDALONE keeps the engine off verbosity.h and the
# filestream VFS; the logging macros come from live_log.h so the
# engine and the driver share one sink.
CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -DHAVE_XRANDR -DHAVE_XINERAMA -DMODELINE_STANDALONE \
           -include live_log.h \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include

LDLIBS  += -lXrandr -lXinerama -lX11 -lm

# Objects live under obj/ here, never next to the sources: the other
# display server samples compile some of the same files with other
# flags, and a stale object in the tree would be picked up.
OBJS := $(addprefix obj/,$(notdir $(SOURCES:.c=.o)))
VPATH := $(sort $(dir $(SOURCES)))

# The driver and the test take the verbosity.h functions the test
# defines, not the standalone macros; -include would rewrite the
# prototypes. Only the engine objects get live_log.h.
NO_MACROS := $(filter-out -include live_log.h -DMODELINE_STANDALONE,$(CFLAGS))
obj/dispserv_x11.o obj/xinerama_common.o: CFLAGS := $(NO_MACROS)
obj/display_servers_x11_live_test.o: CFLAGS := $(NO_MACROS)
obj/compat_strl.o: CFLAGS := $(NO_MACROS)

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
