CC ?= cc
RA := ../../..
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wframe-larger-than=2048 -I$(RA)/libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions -Wno-c11-extensions
endif
ifneq ($(findstring mingw,$(shell $(CC) -dumpmachine)),)
CFLAGS += -DHAVE_MF_AUDIO -D_WIN32_WINNT=0x0400 -D_WIN32_WINDOWS=0x0410
# Exercise the shipping GUID definitions without a bulk SDK GUID library.
CFLAGS += -DHAVE_D3D12
GUIDS := $(RA)/gfx/common/dx_guids.c
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
LDLIBS += -lm

all: encode_test$(EXE)
encode_test$(EXE): encode_test.c $(RA)/record/record_mf_audio.c $(RA)/record/record_mf_audio.h $(GUIDS)
	$(CC) $(CFLAGS) -o $@ encode_test.c $(RA)/record/record_mf_audio.c $(GUIDS) $(LDFLAGS) $(LDLIBS)
native-check: all
	python3 run.py ./encode_test$(EXE)
clean:
	rm -f encode_test encode_test.exe imports.txt
.PHONY: all native-check clean
