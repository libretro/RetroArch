CC ?= cc
COMMON := ../../..
TARGET := rav1_test$(EXE)
SEQUENCE := rav1_sequence_test$(EXE)
CONTAINER := rav1_container_test$(EXE)
CFLAGS += -O1 -g1 -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement -I$(COMMON)/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(CONTAINER) $(SEQUENCE)

$(TARGET): rav1_test.c $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(SEQUENCE): rav1_sequence_test.c $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(CONTAINER): rav1_container_test.c $(COMMON)/include/formats/rav1.h \
      $(COMMON)/include/formats/rav1_sequence.h \
      $(COMMON)/include/formats/rmp4.h $(COMMON)/include/formats/rwebm.h \
      $(COMMON)/formats/mp4/rmp4.c $(COMMON)/formats/webm/rwebm.c
	$(CC) $(CFLAGS) -o $@ rav1_container_test.c \
	   $(COMMON)/formats/mp4/rmp4.c $(COMMON)/formats/webm/rwebm.c $(LDFLAGS) -lm

check: all
	$(RUNNER) ./$(TARGET)
	$(RUNNER) ./$(CONTAINER)
	$(RUNNER) ./$(SEQUENCE)

clean:
	rm -f rav1_sequence_test rav1_sequence_test.exe rav1_test rav1_test.exe rav1_container_test rav1_container_test.exe

.PHONY: all check clean
