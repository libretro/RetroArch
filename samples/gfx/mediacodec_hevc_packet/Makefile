CC ?= cc
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I../../../libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: test
test: test.c ../../../gfx/common/mediacodec_hevc_packet.c
	$(CC) $(CFLAGS) test.c ../../../gfx/common/mediacodec_hevc_packet.c -o $@ $(LDFLAGS)
check: all
	./test
clean:
	rm -f test
.PHONY: all check clean
