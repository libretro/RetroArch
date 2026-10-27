CC ?= cc
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I../../../libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
export CC CFLAGS LDFLAGS
all: audio_edit_test$(EXE)
audio_edit_test$(EXE): audio_edit_test.c ../../../record/record_mp4_audio_edit.c ../../../record/record_mp4_audio_edit.h
	$(CC) $(CFLAGS) audio_edit_test.c ../../../record/record_mp4_audio_edit.c -o $@ $(LDFLAGS)
check: all
	python3 run.py ./audio_edit_test$(EXE)
clean:
	rm -f audio_edit_test audio_edit_test.exe
.PHONY: all check clean
