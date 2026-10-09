REPO_ROOT := ../../..
LRC := $(REPO_ROOT)/libretro-common
CFLAGS += -Wall -Wextra -Wno-unused-function -std=gnu89 -O1 -g \
          -I$(REPO_ROOT) -I$(LRC)/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
COMMON := $(REPO_ROOT)/input/common/output_writer.c \
          $(LRC)/rthreads/rthreads.c $(LRC)/time/rtime.c
LDLIBS += -lpthread
all: output_writer_test output_writer_test_nothreads
output_writer_test: output_writer_test.c $(COMMON)
	$(CC) $(CFLAGS) -DHAVE_THREADS $(LDFLAGS) -o $@ $^ $(LDLIBS)
output_writer_test_nothreads: output_writer_test.c $(COMMON)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
check: all
	./output_writer_test
	./output_writer_test_nothreads
clean:
	rm -f output_writer_test output_writer_test_nothreads \
	      output_writer_test.exe output_writer_test_nothreads.exe
.PHONY: all check clean
