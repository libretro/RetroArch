CC ?= cc
LRC := ../../..
HEADERS := $(LRC)/include/formats/rflv_mux.h $(LRC)/include/formats/video_color.h
HEADERS += $(LRC)/include/formats/rav1.h $(LRC)/include/formats/rav1_sequence.h
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wframe-larger-than=2048 -I$(LRC)/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions -Wno-c11-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: flv_mux_test$(EXE) flv_remux_test$(EXE) flv_color_test$(EXE)
flv_color_test$(EXE): flv_color_test.c $(LRC)/formats/flv/rflv_mux.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ flv_color_test.c $(LRC)/formats/flv/rflv_mux.c $(LDFLAGS)
flv_mux_test$(EXE): flv_mux_test.c $(LRC)/formats/flv/rflv_mux.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ flv_mux_test.c $(LRC)/formats/flv/rflv_mux.c $(LDFLAGS)
flv_remux_test$(EXE): flv_remux_test.c $(LRC)/formats/flv/rflv_mux.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ flv_remux_test.c $(LRC)/formats/flv/rflv_mux.c $(LDFLAGS)
check: all
	$(RUNNER) ./flv_mux_test$(EXE)
	python3 color.py --runner "$(RUNNER)" ./flv_color_test$(EXE)
stream-check: all
	python3 run.py --runner "$(RUNNER)" $(if $(filter 1,$(COLOR_METADATA_ORACLE)),--require-color-metadata,) ./flv_remux_test$(EXE)
clean:
	rm -f flv_color_test flv_color_test.exe
	rm -f flv_mux_test flv_mux_test.exe flv_remux_test flv_remux_test.exe
.PHONY: all check stream-check clean
