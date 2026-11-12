CC ?= cc
RA := ../../..
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wframe-larger-than=2048 -I$(RA)/libretro-common/include -Imock -DRECORD_AT_AUDIO_TEST
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
# Match boolean.h's intentional clang _Bool spelling in C89 mode.
CFLAGS += -Wno-c99-extensions -Wno-c11-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: contract_test$(EXE) resample_test$(EXE)
resample_test$(EXE): resample_test.c mock/AudioToolbox/AudioToolbox.h $(RA)/record/record_at_resample.c $(RA)/record/record_at_resample.h $(RA)/record/record_audio_queue.c
	$(CC) $(CFLAGS) -DRECORD_AT_RESAMPLE_TEST -o $@ resample_test.c $(RA)/record/record_at_resample.c $(RA)/record/record_audio_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
contract_test$(EXE): contract_test.c mock/AudioToolbox/AudioToolbox.h $(RA)/record/record_at_audio.c $(RA)/record/record_at_audio.h
	$(CC) $(CFLAGS) -o $@ contract_test.c $(RA)/record/record_at_audio.c $(LDFLAGS)
check: all
	$(RUNNER) ./contract_test$(EXE)
	$(RUNNER) ./resample_test$(EXE)
encode_test: diagnostic.c encode_test.c $(RA)/record/record_at_audio.c $(RA)/record/record_at_audio.h
	$(CC) $(filter-out -Imock -DRECORD_AT_AUDIO_TEST,$(CFLAGS)) -DHAVE_AVF -o $@ diagnostic.c $(LDFLAGS) -framework AudioToolbox -lm
native-check: encode_test
	python3 run.py ./encode_test
resample_encode_test: resample_encode_test.c encode_test.c $(RA)/record/record_at_resample.c $(RA)/record/record_at_audio.c $(RA)/record/record_audio_queue.c
	$(CC) $(filter-out -Imock -DRECORD_AT_AUDIO_TEST,$(CFLAGS)) -DHAVE_AVF -o $@ resample_encode_test.c $(RA)/record/record_at_resample.c $(RA)/record/record_at_audio.c $(RA)/record/record_audio_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS) -framework AudioToolbox -lm
native-src-check: resample_encode_test
	python3 resample_run.py ./resample_encode_test
clean:
	rm -f contract_test contract_test.exe encode_test resample_test resample_test.exe resample_encode_test
.PHONY: all check native-check native-src-check clean
