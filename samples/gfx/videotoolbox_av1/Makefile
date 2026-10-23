CC ?= cc
CFLAGS += -O2 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I../../../libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: config_test$(EXE)
config_test$(EXE): config_test.c ../../../gfx/common/videotoolbox_av1_config.h ../vulkan_video_caps/av1_session_data.h $(wildcard ../../../libretro-common/include/formats/rav1*.h)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
check: all
	./config_test$(EXE)
	CC="$(CC)" CFLAGS="$(CFLAGS) $(LDFLAGS)" python3 run.py
clean:
	rm -f config_test config_test.exe
.PHONY: all check clean
