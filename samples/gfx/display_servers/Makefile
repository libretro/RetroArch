include ../../sample_objdir.mk

TARGET := display_servers_test

# Path back to the repo root from this sample dir.  Unlike most
# samples here this one links the real driver rather than mirroring
# its shape: gfx/display_servers/dispserv_x11.c refers to only four
# RetroArch-side symbols, which the test supplies, so linking it is
# cheap and means the test actually regresses if the code changes.
REPO_ROOT := ../../..

# The test stubs libXrandr and the handful of Xlib entry points the
# driver reaches, so no X server, display or RandR extension is needed
# to run it -- only the headers.  Nothing links against -lXrandr or
# -lX11 on purpose: a working display is precisely the environment
# that cannot produce the failing queries under test.
SOURCES := display_servers_test.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_x11.c \
           $(REPO_ROOT)/gfx/gfx_media_display.c

CFLAGS  += -Wall -std=gnu99 -g1 -O0 \
           -DHAVE_XRANDR \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

PYTHON ?= python3
ifeq ($(OS),Windows_NT)
all: error-scope-check output-info-check resolution-result-check x11-refresh-check
else
all: $(TARGET) error-scope-check output-info-check resolution-result-check x11-refresh-check
endif

error-scope-check:
	$(PYTHON) error_scope_test.py --cc "$(CC)"

output-info-check:
	$(PYTHON) output_info_test.py --cc "$(CC)" $(if $(SANITIZER),--sanitizer "$(SANITIZER)")

x11-refresh-check:
	$(PYTHON) x11_refresh_lease_test.py --cc "$(CC)" $(if $(SANITIZER),--sanitizer "$(SANITIZER)")

resolution-result-check:
	$(PYTHON) resolution_result_test.py --cc "$(CC)" $(if $(SANITIZER),--sanitizer "$(SANITIZER)")

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean error-scope-check output-info-check resolution-result-check x11-refresh-check
