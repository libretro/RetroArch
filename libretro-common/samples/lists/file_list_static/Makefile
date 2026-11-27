ROOT := ../../..
CC ?= cc
CFLAGS += -std=gnu89 -Wall -Wextra -Werror -Wdeclaration-after-statement -D_GNU_SOURCE -I$(ROOT)/include
ifneq ($(SANITIZER),)
CFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: file_list_static_test$(EXE)

file_list_static_test$(EXE): file_list_static_test.c $(ROOT)/lists/file_list.c
	$(CC) $(CFLAGS) $< $(ROOT)/lists/file_list.c $(ROOT)/compat/compat_strcasestr.c $(LDFLAGS) -o $@

check: file_list_static_test$(EXE)
	$(RUNNER) ./file_list_static_test$(EXE)

clean:
	rm -f file_list_static_test file_list_static_test.exe

.PHONY: all check clean
