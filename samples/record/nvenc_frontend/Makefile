CC ?= cc
RA := ../../..
CFLAGS += -std=gnu89 -O1 -g1 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
CORE := frontend_core$(if $(filter .exe,$(EXE)),.dll,.so)
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(CORE) rate_test$(EXE) audio_test$(EXE)
rate_fragment.c: $(RA)/record/drivers/record_nvenc.c
	sed -n '/^static bool record_nvenc_rate/,/^static bool record_nvenc_config/p' $< | sed '$$d' > $@
audio_fragment.c: $(RA)/record/drivers/record_nvenc.c
	sed -n '/^static bool record_nvenc_audio/,/^const record_driver_t record_nvenc/p' $< | sed '$$d' > $@
video_fragment.c: $(RA)/record/drivers/record_nvenc.c
	sed -n '/^void record_nvenc_gpu_capture/,/^void record_nvenc_stop/{ /^void record_nvenc_stop/q; p; }' $< > $@
audio_test$(EXE): audio_test.c audio_fragment.c video_fragment.c
	$(CC) $(CFLAGS) -o $@ audio_test.c $(LDFLAGS) -lm
rate_test$(EXE): rate_test.c rate_fragment.c
	$(CC) $(CFLAGS) -o $@ rate_test.c $(LDFLAGS) -lm
$(CORE): core.c
	$(CC) $(CFLAGS) $(if $(filter .exe,$(EXE)),,-fPIC) -shared -o $@ $< $(LDFLAGS) -lm
check: all
	$(RUNNER) ./rate_test$(EXE)
	$(RUNNER) ./audio_test$(EXE)
	python3 run.py --core ./$(CORE) $(if $(RETROARCH),--retroarch "$(RETROARCH)",)
clean:
	rm -f frontend_core.dll frontend_core.so rate_fragment.c rate_test rate_test.exe audio_fragment.c video_fragment.c audio_test audio_test.exe
.PHONY: all check clean
