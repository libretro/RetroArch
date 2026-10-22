CC = gcc
ROOT = ../../..
COMMON = $(ROOT)/libretro-common
CPPFLAGS += -I$(COMMON)/include
CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O1
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SOURCES = romm_list_test.c $(ROOT)/tasks/romm_list.c \
 $(COMMON)/formats/json/rjson.c \
 $(COMMON)/string/stdstring.c \
 $(COMMON)/string/rstrtod.c \
 $(COMMON)/encodings/encoding_utf.c \
 $(COMMON)/compat/compat_strl.c
all: romm_list_test
romm_list_test: $(SOURCES) $(ROOT)/tasks/task_romm.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@
check: romm_list_test
	./romm_list_test
clean:
	rm -f romm_list_test romm_list_test.exe
.PHONY: all check clean
