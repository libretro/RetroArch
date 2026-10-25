include ../../sample_objdir.mk

TARGET := ffmpeg_hw_context_test$(EXE)
CORE_DIR := ../../..
FF_CORE_DIR := $(CORE_DIR)/cores/libretro-ffmpeg
FF_CFLAGS := $(shell pkg-config --cflags libavcodec libavformat libavutil libswscale libswresample)
SOURCES_C := hw_context_test.c
OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))

CFLAGS += -O1 -g1 -std=gnu99 -Wall -Wextra -Wno-unused-parameter \
   -Werror=declaration-after-statement -flto -fwhole-program \
   -ffunction-sections -fdata-sections \
   -DRARCH_INTERNAL -DRETRO_API= -DHAVE_THREADS -DHAVE_SWRESAMPLE -D_GNU_SOURCE \
   -I$(CORE_DIR)/libretro-common/include \
   -I$(CORE_DIR)/libretro-common/include/compat -I$(FF_CORE_DIR) $(FF_CFLAGS)
LDFLAGS += -flto -Wl,--gc-sections
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): $(FF_CORE_DIR)/ffmpeg_core.c

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean
