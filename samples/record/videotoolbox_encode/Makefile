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
all: session_gpu_refusal_test$(EXE) av_session_test$(EXE) crop_test$(EXE) av_worker_test$(EXE) audio_queue_test$(EXE) slots_test$(EXE) session_test$(EXE) worker_test$(EXE) audio_rate_test$(EXE) audio_rate_native_test$(EXE)
av_session_thread_test: av_session_thread_test.c $(RA)/record/record_avfoundation_session.c
	$(CC) $(CFLAGS) -pthread -o $@ av_session_thread_test.c $(RA)/record/record_avfoundation_session.c $(RA)/record/record_avfoundation_worker.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_queue.c $(RA)/record/record_audio_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
av_session_test$(EXE): av_worker_test.c $(RA)/record/record_avfoundation_session.c $(RA)/record/record_avfoundation_session.h
	$(CC) $(CFLAGS) -Wno-unused-function -DRECORD_AVF_SESSION_TEST -o $@ av_worker_test.c $(RA)/record/record_avfoundation_session.c $(RA)/record/record_avfoundation_worker.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_queue.c $(RA)/record/record_audio_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
av_worker_test$(EXE): av_worker_test.c $(RA)/record/record_avfoundation_worker.c $(RA)/record/record_avfoundation_worker.h
	$(CC) $(CFLAGS) -o $@ av_worker_test.c $(RA)/record/record_avfoundation_worker.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_queue.c $(RA)/record/record_audio_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
audio_queue_test$(EXE): audio_queue_test.c $(RA)/record/record_audio_queue.c $(RA)/record/record_audio_queue.h
	$(CC) $(CFLAGS) -o $@ audio_queue_test.c $(RA)/record/record_audio_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
audio_thread_test: audio_queue_test.c $(RA)/record/record_audio_queue.c $(RA)/record/record_audio_queue.h
	$(CC) $(CFLAGS) -DRECORD_AUDIO_THREAD_TEST -pthread -o $@ audio_queue_test.c $(RA)/record/record_audio_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
audio_rate_test$(EXE): audio_rate_test.c mock/AudioToolbox/AudioToolbox.h $(RA)/record/drivers/record_avfoundation_audio.h
	$(CC) $(CFLAGS) -Imock -o $@ $< $(LDFLAGS)
audio_rate_native_test$(EXE): audio_rate_native_test.c $(RA)/record/drivers/record_avfoundation_audio.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(if $(filter Darwin,$(shell uname -s)),-framework AudioToolbox,)
native-audio-check: audio_rate_native_test$(EXE)
	$(RUNNER) ./audio_rate_native_test$(EXE)
crop_test$(EXE): crop_test.c $(RA)/libretro-common/formats/h265/rh265_sps_crop.c $(RA)/libretro-common/include/formats/rh265_sps_crop.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
worker_test$(EXE): worker_test.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_worker_vt.h $(RA)/record/record_gpu_queue.c
	$(CC) $(CFLAGS) -o $@ worker_test.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
session_gpu_refusal_test$(EXE): session_test.c $(RA)/gfx/common/videotoolbox_encode.h
	$(CC) $(CFLAGS) -DVIDEOTOOLBOX_ENCODE_TEST_NO_GPU_SELECTION -o $@ $< $(LDFLAGS)
session_test$(EXE): session_test.c $(RA)/gfx/common/videotoolbox_encode.h $(RA)/gfx/common/videotoolbox_encode_slots.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
slots_test$(EXE): slots_test.c $(RA)/gfx/common/videotoolbox_encode_slots.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
thread_test: slots_test.c $(RA)/gfx/common/videotoolbox_encode_slots.h
	$(CC) $(CFLAGS) -DVT_ENCODE_THREAD_TEST -pthread -o $@ $< $(LDFLAGS)
worker_thread_test: worker_thread_test.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_worker_vt.h $(RA)/record/record_gpu_queue.c
	$(CC) $(CFLAGS) -pthread -o $@ worker_thread_test.c $(RA)/record/record_gpu_worker_vt.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
check: all legacy-check
	$(RUNNER) ./session_gpu_refusal_test$(EXE)
	$(RUNNER) ./av_session_test$(EXE)
	$(RUNNER) ./crop_test$(EXE)
	$(RUNNER) ./av_worker_test$(EXE)
	$(RUNNER) ./audio_queue_test$(EXE)
	$(RUNNER) ./slots_test$(EXE)
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./worker_test$(EXE)
	$(RUNNER) ./audio_rate_test$(EXE)
clean:
	rm -f session_gpu_refusal_test session_gpu_refusal_test.exe
	rm -f av_session_test av_session_test.exe av_session_thread_test
	rm -f crop_test crop_test.exe
	rm -f av_worker_test av_worker_test.exe
	rm -f audio_queue_test audio_queue_test.exe audio_thread_test
	rm -f slots_test slots_test.exe session_test session_test.exe worker_test worker_test.exe thread_test worker_thread_test
	rm -f audio_rate_test audio_rate_test.exe
	rm -f audio_rate_native_test audio_rate_native_test.exe
legacy-check: legacy_append_run.py legacy_append_test.c $(RA)/record/drivers/record_avfoundation.m
	CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" RUNNER="$(RUNNER)" python3 legacy_append_run.py
.PHONY: all check clean legacy-check native-audio-check
