CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c89
CFLAGS += -I../../../include
TARGETS := rvp9_reference_test rvp9_hw_session_test

ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGETS)

rvp9_reference_test: rvp9_reference_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h
	$(CC) $(CFLAGS) rvp9_reference_test.c -o $@ $(LDFLAGS)

rvp9_hw_session_test: rvp9_hw_session_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h ../../../include/formats/rvp9_hw.h
	$(CC) $(CFLAGS) rvp9_hw_session_test.c ../../../formats/vp9/rvp9.c -o $@ $(LDFLAGS)

check: $(TARGETS)
	./rvp9_reference_test
	./rvp9_hw_session_test

stream-check: rvp9_hw_session_test
	python3 check_streams.py ./rvp9_hw_session_test

clean:
	rm -f $(TARGETS) $(addsuffix .exe,$(TARGETS))

.PHONY: all check stream-check clean
