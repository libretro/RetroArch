TARGET  := display_servers_win32_vendor_test.exe
MOCKDLL := atiadlxx.dll

# The three Windows custom-timing paths - AMD ADL, ATI legacy
# registry, PowerStrip - cross-built with mingw-w64 and exercised
# under Wine without the hardware: a mock atiadlxx.dll next to the
# exe stands in for the AMD driver (and re-plugs the monitor the way
# the driver does after a table refresh, so the resync helper's wait
# is real), a scratch HKLM key stands in for the adapter's registry
# key, and a window of class TPShidden answering the developer
# messages stands in for a running PowerStrip.
REPO_ROOT := ../../..
CC        := x86_64-w64-mingw32-gcc

SOURCES := display_servers_win32_vendor_test.c \
           $(REPO_ROOT)/gfx/display_servers/win32_modeline_adl.c \
           $(REPO_ROOT)/gfx/display_servers/win32_modeline_ati.c \
           $(REPO_ROOT)/gfx/display_servers/win32_modeline_pstrip.c \
           $(REPO_ROOT)/gfx/display_servers/win32_modeline_resync.c \
           $(REPO_ROOT)/gfx/modeline/modeline_core.c \
           $(REPO_ROOT)/gfx/modeline/modeline_monitor.c \
           $(REPO_ROOT)/libretro-common/compat/compat_strl.c \
           $(REPO_ROOT)/libretro-common/rthreads/rthreads.c \
           $(REPO_ROOT)/libretro-common/features/features_cpu.c \
           $(REPO_ROOT)/libretro-common/time/rtime.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -DHAVE_MODELINE -DHAVE_THREADS -DRARCH_INTERNAL -DHAVE_MENU \
           -DMODELINE_STANDALONE -include live_log.h \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include

LDLIBS  += -lgdi32 -luser32 -ladvapi32 -lm

OBJS := $(addprefix obj/,$(notdir $(SOURCES:.c=.o)))
VPATH := $(sort $(dir $(SOURCES)))

NO_MACROS := $(filter-out -include live_log.h -DMODELINE_STANDALONE,$(CFLAGS))
obj/win32_modeline_adl.o obj/win32_modeline_ati.o obj/win32_modeline_pstrip.o obj/win32_modeline_resync.o: CFLAGS := $(NO_MACROS)
obj/compat_strl.o obj/rthreads.o obj/features_cpu.o obj/rtime.o: CFLAGS := $(NO_MACROS)
obj/display_servers_win32_vendor_test.o: CFLAGS := $(NO_MACROS)

all: $(TARGET) $(MOCKDLL)

obj/%.o: %.c
	@mkdir -p obj
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(MOCKDLL): mock_atiadlxx.c mock_adl.h
	$(CC) -shared -Wall -o $@ mock_atiadlxx.c

clean:
	rm -rf $(TARGET) $(MOCKDLL) obj

.PHONY: clean
