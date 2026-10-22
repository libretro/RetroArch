CC ?= cc
CFLAGS += -O2 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -fstack-usage -I../../../libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: config_test$(EXE)
config_test$(EXE): config_test.c ../../../gfx/common/videotoolbox_vp9_config.h ../../../gfx/common/vp9_color.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
check: all
	./config_test$(EXE)
clean:
	rm -f config_test config_test.exe *.su
.PHONY: all check clean
