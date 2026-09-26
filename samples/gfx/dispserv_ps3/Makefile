TARGET := dispserv_ps3_test

# Links the real display server and its mode rules. dispserv_ps3.c
# reaches PSL1GHT for two video output queries; the tree's PS3 stub
# <sysutil/video.h> declares them in PSL1GHT's layouts and the test
# defines them, to plug the console into a display taking any modes.
REPO_ROOT := ../../..

SOURCES := dispserv_ps3_test.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_ps3.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_ps3_modes.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -D__PS3__ -D__PSL1GHT__ \
           -I$(REPO_ROOT)/tools/platform_stubs/ps3 \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include

# Objects stay here: the tree's own gfx/display_servers/*.o belong
# to the console build (griffin for PSL1GHT).
OBJS := $(notdir $(SOURCES:.c=.o))
vpath %.c $(sort $(dir $(SOURCES)))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

LDFLAGS += -lm

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
