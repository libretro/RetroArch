CC ?= cc
PYTHON ?= python3
RA := ../../..
ifeq ($(EXE),.exe)
PROFILE_PLATFORM_FLAGS := -Wno-cast-function-type
endif
CFLAGS += -O1 -g1 -std=gnu89 -Wall -Wextra -Werror -Wframe-larger-than=2048 \
          -DHAVE_RECORD_METAL -DHAVE_THREADS \
          -DHAVE_CONFIGFILE -I$(RA)/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: capture_test$(EXE) profile_test$(EXE) core_clock_test$(EXE) init_test$(EXE) menu_test$(EXE)
PROFILE_SOURCES := profile_test.c \
  $(RA)/libretro-common/file/config_file.c $(RA)/libretro-common/file/file_path.c \
  $(RA)/libretro-common/string/stdstring.c $(RA)/libretro-common/string/rstrtod.c \
  $(RA)/libretro-common/compat/compat_strl.c $(RA)/libretro-common/compat/compat_strldup.c \
  $(RA)/libretro-common/compat/compat_posix_string.c $(RA)/libretro-common/encodings/encoding_utf.c \
  $(RA)/libretro-common/time/rtime.c
capture_fragment.c: $(RA)/record/drivers/record_metal.c
	sed -n '/^typedef struct record_metal_context/,/^} record_metal_context_t;/p' $< > $@
	sed -n '/^void record_metal_gpu_capture/,/^void record_metal_stop/{ /^void record_metal_stop/q; p; }' $< >> $@
	sed -n '/^static bool record_metal_audio/,/^const record_driver_t record_metal/{ /^const record_driver_t record_metal/q; p; }' $< >> $@
capture_test$(EXE): capture_test.c capture_fragment.c
	$(CC) $(CFLAGS) capture_test.c -o $@ $(LDFLAGS) -lm
profile_fragment.c: $(RA)/record/drivers/record_metal.c
	sed -n '/^static bool record_metal_config/,/^static bool record_metal_color/{ /^static bool record_metal_color/q; p; }' $< > $@
profile_test$(EXE): $(PROFILE_SOURCES) profile_fragment.c $(RA)/record/record_gpu_dispatch_metal.h $(RA)/libretro-common/include/file/config_file.h
	$(CC) $(CFLAGS) $(PROFILE_PLATFORM_FLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections $(PROFILE_SOURCES) \
	  -o $@ $(LDFLAGS) -Wl,--gc-sections -lm
init_fragment.c: $(RA)/record/drivers/record_metal.c
	sed -n '/^typedef struct record_metal_context/,/^static bool record_metal_finalize/{ /^static bool record_metal_finalize/q; p; }' $< > $@
init_test$(EXE): init_test.c init_fragment.c $(filter-out profile_test.c,$(PROFILE_SOURCES)) $(RA)/record/record_vt_packet.h
	$(CC) $(CFLAGS) $(PROFILE_PLATFORM_FLAGS) -DHAVE_NETWORKING -DHAVE_RTMP -DHAVE_RFLV -DHAVE_CRYPTO -Wno-unused-parameter -ffunction-sections -fdata-sections init_test.c $(filter-out profile_test.c,$(PROFILE_SOURCES)) \
	  -o $@ $(LDFLAGS) -Wl,--gc-sections -lm
menu_fragment.c: menu_fragments.py $(RA)/record/record_driver.c $(RA)/menu/menu_displaylist.c $(RA)/menu/menu_setting.c
	$(PYTHON) menu_fragments.py
menu_test$(EXE): menu_test.c menu_fragment.c $(RA)/record/record_driver.h $(RA)/menu/menu_displaylist.h
	$(CC) $(CFLAGS) -DHAVE_MENU -Wno-unused-parameter $< -o $@ $(LDFLAGS)
check: all
	$(RUNNER) ./capture_test$(EXE)
	$(RUNNER) ./profile_test$(EXE)
	$(RUNNER) ./core_clock_test$(EXE)
	$(RUNNER) ./init_test$(EXE)
	$(RUNNER) ./menu_test$(EXE)
core_clock_test$(EXE): core_clock_test.c ../nvenc_frontend/core.c $(RA)/libretro-common/include/libretro.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lm
clean:
	rm -f capture_test capture_test.exe capture_fragment.c profile_test profile_test.exe profile_fragment.c
	rm -f menu_test menu_test.exe menu_fragment.c
	rm -f init_test init_test.exe init_fragment.c
	rm -f core_clock_test core_clock_test.exe
.PHONY: all check clean
