CC ?= cc
RA := ../../..
CFLAGS += -DHAVE_THREADS -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I$(RA)/libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
# The project boolean.h uses clang's _Bool spelling in C89 as well.
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: slots_test$(EXE) session_test$(EXE) worker_test$(EXE)
worker_test$(EXE): worker_test.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_worker_vt.h $(RA)/record/record_gpu_queue.c
	$(CC) $(CFLAGS) -o $@ worker_test.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
session_test$(EXE): session_test.c $(RA)/gfx/common/videotoolbox_encode.h $(RA)/gfx/common/videotoolbox_encode_slots.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
slots_test$(EXE): slots_test.c $(RA)/gfx/common/videotoolbox_encode_slots.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
thread_test: slots_test.c $(RA)/gfx/common/videotoolbox_encode_slots.h
	$(CC) $(CFLAGS) -DVT_ENCODE_THREAD_TEST -pthread -o $@ $< $(LDFLAGS)
worker_thread_test: worker_thread_test.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_worker_vt.h $(RA)/record/record_gpu_queue.c
	$(CC) $(CFLAGS) -pthread -o $@ worker_thread_test.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
check: all
	$(RUNNER) ./slots_test$(EXE)
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./worker_test$(EXE)
clean:
	rm -f slots_test slots_test.exe session_test session_test.exe worker_test worker_test.exe thread_test worker_thread_test
.PHONY: all check clean
