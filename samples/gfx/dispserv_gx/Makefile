TARGET := dispserv_gx_test

# Links the real display server and its mode rules. dispserv_gx.c
# reaches libogc for four VIDEO_* queries and three SYSCONF ones; the
# stubs/ headers declare them in libogc's shapes and the test defines
# them, so it can put the console on any TV standard and cable.
REPO_ROOT := ../../..

SOURCES := dispserv_gx_test.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_gx.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_gx_modes.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -DGEKKO -DHW_RVL \
           -Istubs \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include

# Objects stay here: the tree's own gfx/display_servers/*.o belong
# to the console build (Makefile.wii/ngc put them there).
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
