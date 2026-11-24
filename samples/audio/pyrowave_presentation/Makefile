RA := ../../..
COMMON := $(RA)/libretro-common
TARGET := wasapi_presentation_test.exe
CC ?= cc
CFLAGS += -O2 -std=gnu89 -Wall -Wextra -I$(COMMON)/include -I$(RA)/deps -I$(RA) \
          -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_PYROWAVE
WINDOWS := $(if $(findstring mingw,$(CC)),1,$(if $(filter Windows_NT,$(OS)),1,0))
ifeq ($(WINDOWS),1)
CFLAGS += -DHAVE_WASAPI -D_WIN32_WINNT=0x0602
LDLIBS := -lole32 -lksuser -luuid -lwinmm -lm
SOURCES := wasapi_presentation_test.c wasapi_presentation_oracle.c stubs_retroarch.c \
           $(RA)/audio/common/mmdevice_common.c \
           $(RA)/audio/audio_upmix.c $(RA)/audio/audio_binaural.c \
           $(RA)/record/record_pyrowave_clock.c \
           $(COMMON)/formats/ac3/rac3_frame.c $(COMMON)/formats/ac3/rac3_decode.c \
           $(COMMON)/formats/ac3/rac3_encode.c $(COMMON)/formats/iec61937/iec61937.c \
           $(COMMON)/lists/string_list.c $(COMMON)/queues/fifo_queue.c \
           $(COMMON)/queues/retro_spsc.c $(COMMON)/rthreads/rthreads.c \
           $(COMMON)/rthreads/retro_eventcount.c $(COMMON)/features/features_cpu.c \
           $(COMMON)/time/rtime.c $(COMMON)/compat/compat_strl.c \
           $(COMMON)/compat/compat_strldup.c $(COMMON)/compat/compat_posix_string.c \
           $(COMMON)/string/stdstring.c \
           $(COMMON)/encodings/encoding_utf.c
else
TARGET := alsa_presentation_test
CFLAGS += -DHAVE_ALSA -D_GNU_SOURCE
SOURCES := alsa_presentation_test.c stubs_retroarch.c \
           $(RA)/audio/drivers/alsa.c $(RA)/audio/common/alsa.c \
           $(RA)/audio/audio_upmix.c $(RA)/audio/audio_binaural.c \
           $(COMMON)/lists/string_list.c $(COMMON)/rthreads/rthreads.c \
           $(COMMON)/compat/compat_strl.c $(COMMON)/compat/compat_strldup.c \
           $(COMMON)/string/stdstring.c $(COMMON)/encodings/encoding_utf.c
LDLIBS := -lasound -lm -lpthread \
          -Wl,--wrap=snd_pcm_state -Wl,--wrap=snd_pcm_delay \
          -Wl,--wrap=snd_pcm_writei -Wl,--wrap=snd_pcm_recover
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) $(RA)/audio/audio_presentation.h $(RA)/audio/audio_driver.h $(RA)/audio/drivers/wasapi.c
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

check: $(TARGET)
	./$(TARGET)
ifeq ($(WINDOWS),1)
	./$(TARGET) --exclusive
endif

unit-check: $(TARGET)
ifeq ($(WINDOWS),1)
	$(RUNNER) ./$(TARGET) --unit
else
	$(RUNNER) ./$(TARGET)
endif

clean:
	rm -f wasapi_presentation_test.exe alsa_presentation_test

.PHONY: all check unit-check clean
