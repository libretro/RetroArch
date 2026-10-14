include ../../sample_objdir.mk

TARGET ?= video_windows_test
RARCH_DIR := ../../..
SOURCES := video_windows_test.c $(RARCH_DIR)/gfx/video_windows.c $(RARCH_DIR)/gfx/video_views.c
OBJS := $(addprefix $(SAMPLE_OBJDIR)/,$(subst ../,__/,$(SOURCES:.c=.o)))
CFLAGS += -Wall -std=c89 -pedantic -Werror=declaration-after-statement -g -O1 \
          -I$(RARCH_DIR)/libretro-common/include -I$(RARCH_DIR)
LDFLAGS += -lm

ifneq ($(SANITIZER),)
CFLAGS := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	$(abspath $(TARGET))

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

clean:
	@sample_objdir="$(SAMPLE_OBJDIR)"; rm -rf "$${sample_objdir:?}"
	rm -f "$(TARGET)"

.PHONY: all check clean
