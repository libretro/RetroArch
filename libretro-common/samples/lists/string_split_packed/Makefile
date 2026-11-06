ROOT := ../../..
CC ?= cc
CFLAGS += -std=gnu89 -Wall -Wextra -Werror -Wdeclaration-after-statement -D_GNU_SOURCE -I$(ROOT)/include
ifneq ($(SANITIZER),)
CFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: string_split_packed_test$(EXE)

string_split_packed_test$(EXE): string_split_packed_test.c $(ROOT)/lists/string_list.c
	$(CC) $(CFLAGS) $< $(ROOT)/lists/string_list.c $(ROOT)/compat/compat_strl.c $(LDFLAGS) -o $@

check: string_split_packed_test$(EXE)
	$(RUNNER) ./string_split_packed_test$(EXE)

clean:
	rm -f string_split_packed_test string_split_packed_test.exe

.PHONY: all check clean
