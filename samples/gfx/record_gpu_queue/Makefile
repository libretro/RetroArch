CC ?= cc
RA := ../../..
TEST_FRAMES ?= 200000
CFLAGS += -DTEST_FRAMES=$(TEST_FRAMES) $(SANITIZE) -O1 -g1 -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
LDLIBS += $(SANITIZE)
SRC := queue_test.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c
ifeq ($(THREADS),1)
CFLAGS += -DHAVE_THREADS
SRC += $(RA)/libretro-common/rthreads/rthreads.c
LDLIBS += $(if $(filter .exe,$(EXE)),,-lpthread)
endif
all: queue_test$(EXE)
queue_test$(EXE): $(SRC) $(RA)/record/record_gpu_queue.h
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@
check: all
	$(RUNNER) ./queue_test$(EXE)
clean:
	rm -f queue_test queue_test.exe
.PHONY: all check clean
