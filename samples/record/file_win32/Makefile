CC ?= cc
RA := ../../..
CFLAGS += -std=gnu89 -O1 -g1 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SRC := file_test.c $(RA)/record/record_file_win32.c $(RA)/libretro-common/encodings/encoding_utf.c $(RA)/libretro-common/compat/compat_strl.c
all: file_test$(EXE)
file_test$(EXE): $(SRC) $(RA)/record/record_file_win32.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)
check: all
	$(RUNNER) ./file_test$(EXE)
clean:
	rm -f file_test file_test.exe
.PHONY: all check clean
