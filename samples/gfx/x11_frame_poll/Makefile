TARGET := x11_frame_poll_test

# Links the real gfx/common/x11_common.c against libX11 and runs the
# per-frame window poll on whatever $DISPLAY points at (Xvfb in CI).
REPO_ROOT := ../../..

SOURCES := x11_frame_poll_test.c \
           $(REPO_ROOT)/gfx/common/x11_common.c \
           $(REPO_ROOT)/libretro-common/compat/compat_strl.c \
           $(REPO_ROOT)/libretro-common/encodings/encoding_utf.c

CFLAGS  += -Wall -g -O0 \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include

LDLIBS  += -lX11

OBJS  := $(addprefix obj/,$(notdir $(SOURCES:.c=.o)))
VPATH := $(sort $(dir $(SOURCES)))

obj/x11_frame_poll_test.o: CFLAGS += -std=c89 -pedantic -Wno-long-long

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
