TARGET := crt_switch_modeline_test

# Path back to the repo root from this sample dir.  Like
# samples/gfx/display_servers this links the real files rather than
# mirroring their shape: the consumer and the whole modeline engine
# are compiled in, and the test supplies the RetroArch-side symbols
# they reach for.  The display server is the only stub, because a
# server with no modeline ops is what the test is about.
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

SOURCES := crt_switch_modeline_test.c \
           $(REPO_ROOT)/gfx/video_crt_switch.c \
           $(REPO_ROOT)/gfx/modeline/modeline_core.c \
           $(REPO_ROOT)/gfx/modeline/modeline_edid.c \
           $(REPO_ROOT)/gfx/modeline/modeline_ini.c \
           $(REPO_ROOT)/gfx/modeline/modeline_list.c \
           $(REPO_ROOT)/gfx/modeline/modeline_monitor.c \
           $(LRC)/compat/compat_strl.c \
           $(LRC)/compat/fopen_utf8.c \
           $(LRC)/encodings/encoding_utf.c \
           $(LRC)/file/file_path.c \
           $(LRC)/file/file_path_io.c \
           $(LRC)/streams/file_stream.c \
           $(LRC)/string/rstrtod.c \
           $(LRC)/string/stdstring.c \
           $(LRC)/rthreads/rthreads.c \
           $(LRC)/time/rtime.c \
           $(LRC)/vfs/vfs_implementation.c

CFLAGS  += -Wall -std=gnu89 -g -O0 \
           -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_MENU -DHAVE_MODELINE \
           -DHAVE_CONFIGFILE -DHAVE_DYNAMIC -D_GNU_SOURCE \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/deps \
           -I$(LRC)/include

LDFLAGS += -lm -lpthread

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

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean
