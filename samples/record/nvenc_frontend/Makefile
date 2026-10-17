CC ?= cc
RA := ../../..
CFLAGS += -std=gnu89 -O1 -g1 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
CORE := frontend_core$(if $(filter .exe,$(EXE)),.dll,.so)
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(CORE) rate_test$(EXE) audio_test$(EXE) layout_test$(EXE) float_negotiation_test$(EXE)
float_fragment.c: $(RA)/runloop.c
	sed -n '/^      case RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_FLOAT:/,/^      case RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_MULTI:/{ /^      case RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_MULTI:/q; p; }' $< > $@
float_negotiation_test$(EXE): float_negotiation_test.c float_fragment.c
	$(CC) $(CFLAGS) -I$(RA)/deps -o $@ float_negotiation_test.c $(LDFLAGS) -lm
layout_fragment.c: $(RA)/record/drivers/record_nvenc.c
	sed -n '/^static bool record_nvenc_layout/,/^static void .record_nvenc_init/p' $< | sed '$$d' > $@
layout_test$(EXE): layout_test.c layout_fragment.c
	$(CC) $(CFLAGS) -o $@ layout_test.c $(RA)/audio/audio_upmix.c $(RA)/libretro-common/formats/ac3/rac3_encode.c $(RA)/libretro-common/formats/ac3/rac3_frame.c $(LDFLAGS) -lm
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
	$(RUNNER) ./layout_test$(EXE)
	$(RUNNER) ./float_negotiation_test$(EXE)
	python3 run.py --core ./$(CORE) $(if $(RETROARCH),--retroarch "$(RETROARCH)",)
clean:
	rm -f frontend_core.dll frontend_core.so rate_fragment.c rate_test rate_test.exe audio_fragment.c video_fragment.c audio_test audio_test.exe layout_fragment.c layout_test layout_test.exe float_fragment.c float_negotiation_test float_negotiation_test.exe
.PHONY: all check clean
