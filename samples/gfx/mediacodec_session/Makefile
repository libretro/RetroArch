CC ?= cc
JAVA_HOME ?= $(shell dirname $$(dirname $$(readlink -f $$(command -v javac))))
CFLAGS += -DHAVE_DYLIB -DMEDIACODEC_SESSION_TEST -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I. -I../../../libretro-common/include -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: test
test: test.c mediacodec_session_test_types.h ../../../gfx/common/mediacodec_session.c
	$(CC) $(CFLAGS) test.c ../../../gfx/common/mediacodec_session.c -o $@ $(LDFLAGS)
check: all
	./test
clean:
	rm -f test
.PHONY: all check clean
