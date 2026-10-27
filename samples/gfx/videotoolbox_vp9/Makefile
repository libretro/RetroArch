CC ?= cc
CFLAGS += -O2 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -fstack-usage -I../../../libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: config_test$(EXE) packet_test$(EXE)
config_test$(EXE): config_test.c ../../../gfx/common/videotoolbox_vp9_config.h ../../../libretro-common/include/formats/rvp9_color.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
packet_test$(EXE): packet_test.c ../../../gfx/common/videotoolbox_vp9_packet.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
check: all
	./config_test$(EXE)
	./packet_test$(EXE)
	CC="$(CC)" CFLAGS="$(CFLAGS) $(LDFLAGS)" python3 run.py
clean:
	rm -f config_test config_test.exe packet_test packet_test.exe *.su
.PHONY: all check clean
