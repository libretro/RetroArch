CC ?= cc
RA := ../../..
CFLAGS += -O1 -g1 -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifeq ($(EXE),.exe)
PLATFORM :=
SHIM :=
else
PLATFORM := -D_WIN32 -Iwin32 -pthread
SHIM := win32.o
endif
HEADERS := $(RA)/record/record_gpu_dispatch_d3d12.h $(RA)/record/record_gpu_session_d3d12.h
SOURCE := $(RA)/record/record_gpu_dispatch_d3d12.c
all: dispatch_test$(EXE) threadless_test$(EXE) session_integration$(EXE)
win32.o: win32.c win32/windows.h
	$(CC) $(CFLAGS) -pthread -c win32.c -o $@
dispatch_test$(EXE): dispatch_test.c $(SOURCE) $(HEADERS) $(SHIM)
	$(CC) $(CFLAGS) -DHAVE_THREADS $(PLATFORM) -o $@ dispatch_test.c $(SHIM) $(LDFLAGS)
threadless_test$(EXE): dispatch_test.c $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) -UHAVE_THREADS -o $@ dispatch_test.c $(LDFLAGS)
FIXTURE := ../gpu_av_output
SESSION_SOURCES := $(RA)/record/record_gpu_session_d3d12.c $(RA)/record/record_gpu_worker_d3d12.c $(RA)/record/record_gpu_queue.c $(RA)/record/record_gpu_output_d3d12.c $(RA)/record/record_mux_mp4.c $(RA)/libretro-common/formats/ac3/rac3_frame.c $(RA)/libretro-common/formats/ac3/rac3_encode.c $(RA)/libretro-common/queues/retro_spsc.c
dispatch_uut.o: $(SOURCE) $(HEADERS)
	$(CC) $(CFLAGS) -DHAVE_THREADS $(PLATFORM) -c $(SOURCE) -o $@
session_integration$(EXE): session_integration.c $(FIXTURE)/session_test.c $(FIXTURE)/mock_file.c $(FIXTURE)/mock_file.h $(SESSION_SOURCES) dispatch_uut.o $(SHIM)
	$(CC) $(CFLAGS) -DHAVE_THREADS $(if $(filter .exe,$(EXE)),,-pthread) -o $@ session_integration.c $(FIXTURE)/mock_file.c $(SESSION_SOURCES) dispatch_uut.o $(SHIM) $(LDFLAGS) -lm
check: all
	$(RUNNER) ./dispatch_test$(EXE)
	$(RUNNER) ./threadless_test$(EXE)
	$(RUNNER) ./session_integration$(EXE)
clean:
	rm -f dispatch_test dispatch_test.exe threadless_test threadless_test.exe session_integration session_integration.exe dispatch_uut.o win32.o
.PHONY: all check clean
