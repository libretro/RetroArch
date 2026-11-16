CC ?= cc
LRC := ../../..
include ../../sample_objdir.mk
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wframe-larger-than=2048 -I$(LRC)/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions -Wno-c11-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
ifneq ($(findstring undefined,$(SANITIZER)),)
CFLAGS += -fno-sanitize-recover=undefined
endif
endif
all: net_wake_test$(EXE)
net_wake_test$(EXE): net_wake_test.c $(LRC)/net/net_wake.c $(LRC)/include/net/net_wake.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
check: all
	$(RUNNER) ./net_wake_test$(EXE)
clean:
	rm -f net_wake_test net_wake_test.exe
.PHONY: all check clean
