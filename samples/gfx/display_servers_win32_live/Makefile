TARGET := display_servers_win32_live_test.exe

# Cross-builds the real Win32 display server, its vendor timing paths
# and the modeline engine with mingw-w64, and runs the result under
# Wine on an X display.  Wine backs ChangeDisplaySettingsExA and
# EnumDisplaySettingsExA with the X server's RandR modes, so the
# listed-mode part of the Windows path - device lookup, PCI id parse,
# mode enumeration with the desktop flagged, CDS switch and restore -
# is a real switch.  The vendor paths (ADL, ATI legacy, PowerStrip)
# are linked and asked to open; with no AMD driver and no PowerStrip
# they must decline, which is the behaviour on a plain Windows box.
REPO_ROOT := ../../..
CC        := x86_64-w64-mingw32-gcc

SOURCES := display_servers_win32_live_test.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_win32.c \
           $(REPO_ROOT)/gfx/display_servers/win32_modeline_adl.c \
           $(REPO_ROOT)/gfx/display_servers/win32_modeline_ati.c \
           $(REPO_ROOT)/gfx/display_servers/win32_modeline_pstrip.c \
           $(REPO_ROOT)/gfx/display_servers/win32_modeline_resync.c \
           $(REPO_ROOT)/gfx/modeline/modeline_core.c \
           $(REPO_ROOT)/gfx/modeline/modeline_monitor.c \
           $(REPO_ROOT)/gfx/modeline/modeline_list.c \
           $(REPO_ROOT)/gfx/modeline/modeline_ini.c \
           $(REPO_ROOT)/libretro-common/compat/compat_strl.c \
           $(REPO_ROOT)/libretro-common/rthreads/rthreads.c \
           $(REPO_ROOT)/libretro-common/features/features_cpu.c \
           $(REPO_ROOT)/libretro-common/time/rtime.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -DHAVE_MODELINE -DHAVE_THREADS -DRARCH_INTERNAL -DHAVE_MENU \
           -DMODELINE_STANDALONE -include live_log.h \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include

LDLIBS  += -lgdi32 -luser32 -lole32 -lm

# Objects live under obj/ here, never next to the sources: the other
# display server samples compile some of the same files with other
# flags, and a stale object in the tree would be picked up.
OBJS := $(addprefix obj/,$(notdir $(SOURCES:.c=.o)))
VPATH := $(sort $(dir $(SOURCES)))

# Only the engine objects take the standalone logging macros; the
# driver, the vendor paths and the test use the verbosity.h functions
# the test defines.
NO_MACROS := $(filter-out -include live_log.h -DMODELINE_STANDALONE,$(CFLAGS))
obj/dispserv_win32.o obj/win32_modeline_adl.o obj/win32_modeline_ati.o obj/win32_modeline_pstrip.o obj/win32_modeline_resync.o: CFLAGS := $(NO_MACROS)
obj/compat_strl.o obj/rthreads.o obj/features_cpu.o obj/rtime.o: CFLAGS := $(NO_MACROS)
obj/display_servers_win32_live_test.o: CFLAGS := $(NO_MACROS)

all: $(TARGET)

obj/%.o: %.c
	@mkdir -p obj
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(TARGET) obj

.PHONY: clean
