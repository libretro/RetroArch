CC ?= cc
RA := ../../..
CFLAGS += -std=gnu89 -O1 -g1 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
MUX := $(RA)/record/record_mux_mp4.c $(RA)/record/record_mux_mp4.h
FRAME := $(RA)/libretro-common/formats/ac3/rac3_frame.c
ENCODE := $(RA)/libretro-common/formats/ac3/rac3_encode.c
OUTPUT := $(RA)/record/record_file_win32.c $(RA)/libretro-common/encodings/encoding_utf.c $(RA)/libretro-common/compat/compat_strl.c
all: mux_test$(EXE) mux_file$(EXE)
mux_test$(EXE): mux_test.c $(MUX) $(FRAME)
	$(CC) $(CFLAGS) -o $@ mux_test.c $(FRAME) $(LDFLAGS)
mux_file$(EXE): mux_file.c $(MUX) $(FRAME) $(ENCODE) $(OUTPUT)
	$(CC) $(CFLAGS) -o $@ mux_file.c $(RA)/record/record_mux_mp4.c $(FRAME) $(ENCODE) $(OUTPUT) $(LDFLAGS) -lm
check: all
	$(RUNNER) ./mux_test$(EXE)
	python3 run.py --runner "$(RUNNER)" ./mux_file$(EXE)
clean:
	rm -f mux_test mux_test.exe mux_file mux_file.exe
.PHONY: all check clean
