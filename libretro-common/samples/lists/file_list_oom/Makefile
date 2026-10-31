ROOT := ../../..
CC ?= cc
CFLAGS += -std=gnu89 -Wall -Wextra -Werror -Wdeclaration-after-statement -D_GNU_SOURCE -I$(ROOT)/include
ifneq ($(SANITIZER),)
CFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

file_list_oom_test$(EXE): file_list_oom_test.c $(ROOT)/lists/file_list.c
	$(CC) $(CFLAGS) $< $(ROOT)/compat/compat_strcasestr.c $(LDFLAGS) -o $@

check: file_list_oom_test$(EXE)
	$(RUNNER) ./file_list_oom_test$(EXE)

clean:
	rm -f file_list_oom_test file_list_oom_test.exe

.PHONY: check clean
