include ../../sample_objdir.mk

TARGET := gfx_thumbnail_match_test

# The harness includes gfx/gfx_thumbnail_match.c, so it reaches the
# engine's tiers and records as well as its API.
REPO_ROOT := ../../..

SOURCES := gfx_thumbnail_match_test.c

ifneq ($(findstring mingw,$(CC)),)
   EXE := .exe
else
   EXE :=
endif

CFLAGS += -Wall -Wextra -std=c89 -pedantic -Werror=declaration-after-statement \
          -Wno-long-long -g1 -O0 \
          -I$(REPO_ROOT) \
          -I$(REPO_ROOT)/libretro-common/include

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)$(EXE)

gfx_thumbnail_match_test.o: $(REPO_ROOT)/gfx/gfx_thumbnail_match.c \
                            $(REPO_ROOT)/gfx/gfx_thumbnail_match.h

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET)$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)$(EXE) fixtures cases.txt

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJS)
	rm -rf $(SAMPLE_OBJDIR)

.PHONY: all check clean
