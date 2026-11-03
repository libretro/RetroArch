include ../../sample_objdir.mk

compiler     := gcc
TARGET       := pipeline_wakeups_test

CORE_DIR = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) \
	-I$(CORE_DIR)/deps -I$(CORE_DIR)/deps/stb

CC      := $(compiler)
flags   += -std=gnu99 -g1

SOURCES_C := \
	pipeline_wakeups_test.c \
	../../../audio/audio_upmix.c \
	../../../audio/audio_stretch.c \
	../../../audio/audio_pipeline_stretch.c \
	../../../audio/audio_speed_lpf.c \
	../../../audio/audio_thread_wrapper.c \
	../../../frontend/thread_elevation.c \
	../../../frontend/thread_elevation/eevdf.c \
	../../../audio/audio_binaural.c \
	stubs_retroarch.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/lists/dir_list.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/audio/conversion/float_to_s16.c \
	$(LIBRETRO_COMM_DIR)/audio/conversion/s16_to_float.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler.c \
 $(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler_int16.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler_int16.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/nearest_resampler_int16.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/file/config_file.c \
	$(LIBRETRO_COMM_DIR)/file/config_file_userdata.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/nearest_resampler.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS += -Wall $(INCDIRS)
CFLAGS += -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_AUDIOMIXER -DHAVE_MENU \
	-DHAVE_CONFIGFILE -D_GNU_SOURCE
LDFLAGS += -lm -lpthread

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))

all: $(TARGET)

%.o: %.c
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -c -o$@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -c -o$@ $<

$(TARGET): $(OBJS)
	$(CC) -o$@ $(OBJS) $(LDFLAGS)

pipeline_wakeups_test.o: $(CORE_DIR)/audio/audio_driver.c $(CORE_DIR)/audio/audio_driver.h

# One second a setting keeps the suite quick; it is enough to show the
# shape and not enough to trust the last digit. A real before/after
# comparison wants the default four, and the other two modes:
#   ./pipeline_wakeups_test 4
#   SPREAD=1 ./pipeline_wakeups_test 4
#   SPREAD=1 NOTIFY_PER_PUBLISH=1 ./pipeline_wakeups_test 4
#   TRANSPORT=dry ./pipeline_wakeups_test 4
#   TRANSPORT=lpf ./pipeline_wakeups_test 4
#   TRANSPORT=stretch SPREAD=1 ./pipeline_wakeups_test 4
check: $(TARGET)
	./$(TARGET) 1

check-transport: $(TARGET)
	TRANSPORT=dry ./$(TARGET) 1
	TRANSPORT=lpf ./$(TARGET) 1
	TRANSPORT=stretch ./$(TARGET) 1

check-wrapper: $(TARGET)
	WRAPPER=1 TRANSPORT=dry ./$(TARGET) 1
	WRAPPER=1 TRANSPORT=lpf ./$(TARGET) 1
	WRAPPER=1 TRANSPORT=stretch ./$(TARGET) 1
	WRAPPER=1 SOURCE_FLOAT=1 TRANSPORT=dry ./$(TARGET) 1
	WRAPPER=1 SOURCE_FLOAT=1 TRANSPORT=lpf ./$(TARGET) 1
	WRAPPER=1 SOURCE_FLOAT=1 TRANSPORT=stretch ./$(TARGET) 1
	WRAPPER=1 DEVICE_INT16=1 TRANSPORT=dry ./$(TARGET) 1
	WRAPPER=1 DEVICE_INT16=1 TRANSPORT=lpf ./$(TARGET) 1
	WRAPPER=1 DEVICE_INT16=1 TRANSPORT=stretch ./$(TARGET) 1
	WRAPPER=1 SOURCE_FLOAT=1 DEVICE_INT16=1 TRANSPORT=dry ./$(TARGET) 1
	WRAPPER=1 SOURCE_FLOAT=1 DEVICE_INT16=1 TRANSPORT=lpf ./$(TARGET) 1
	WRAPPER=1 SOURCE_FLOAT=1 DEVICE_INT16=1 TRANSPORT=stretch ./$(TARGET) 1

check-lend: $(TARGET)
	$(MAKE) $(PAR) run-lend-i16 run-lend-f32i16 run-lend-f32
run-lend-i16: $(TARGET)
	WRAPPER=1 LEND=1 TRANSPORT=dry ./$(TARGET) 1
run-lend-f32i16: $(TARGET)
	WRAPPER=1 LEND=1 SOURCE_FLOAT=1 DEVICE_INT16=1 TRANSPORT=dry ./$(TARGET) 1
run-lend-f32: $(TARGET)
	WRAPPER=1 LEND=1 SOURCE_FLOAT=1 TRANSPORT=stretch ./$(TARGET) 1

# The runs below each wait on a clocked device for a quarter second of
# audio and use next to no CPU, so a check runs its runs at once: one
# make target per run, under a sub-make with PAR (-j and the output of
# each run kept together). PAR= runs them one after another.
PAR ?= -j16 -Otarget

# A run: $(1) its name, $(2) its environment
define RUN
run-$(1): $$(TARGET)
	$(2) ./$$(TARGET) 0.25
.PHONY: run-$(1)
endef

TEMPOS := 0.25 0.5 1 2 4 8 16 32
$(foreach t,$(TEMPOS),$(eval $(call RUN,tempo-i16-$(t),WRAPPER=1 DEVICE_INT16=1 TRANSPORT=stretch TEMPO=$(t))))
$(foreach t,$(TEMPOS),$(eval $(call RUN,tempo-f32-$(t),WRAPPER=1 SOURCE_FLOAT=1 TRANSPORT=stretch TEMPO=$(t))))
check-tempo: $(TARGET)
	$(MAKE) $(PAR) $(foreach t,$(TEMPOS),run-tempo-i16-$(t) run-tempo-f32-$(t))

# The live runs inherit LAYOUT, LIVE_LAYOUTS, SPEED_LPF, RUNLOOP_POLICY
# and AUTO_RUNLOOP from the environment the check sets
$(eval $(call RUN,live-i16,WRAPPER=1 LIVE_CONTROLS=1 TRANSPORT=stretch DEVICE_INT16=1))
$(eval $(call RUN,live-f32,WRAPPER=1 LIVE_CONTROLS=1 TRANSPORT=stretch SOURCE_FLOAT=1))
$(eval $(call RUN,live-mix,WRAPPER=1 LIVE_CONTROLS=1 TRANSPORT=stretch))
$(eval $(call RUN,live-f32i16,WRAPPER=1 LIVE_CONTROLS=1 TRANSPORT=stretch SOURCE_FLOAT=1 DEVICE_INT16=1))
LIVE_RUNS := run-live-i16 run-live-f32 run-live-mix run-live-f32i16
check-live: $(TARGET)
	$(MAKE) $(PAR) $(LIVE_RUNS)

check-layout: $(TARGET)
	LIVE_LAYOUTS=1 LAYOUT=7.1 $(MAKE) $(PAR) $(LIVE_RUNS)

# the two live sets of each, at once
check-speed-lpf: $(TARGET)
	$(MAKE) $(PAR) speed-lpf-live speed-lpf-layout
speed-lpf-live: $(TARGET)
	SPEED_LPF=1 $(MAKE) $(PAR) $(LIVE_RUNS)
speed-lpf-layout: $(TARGET)
	SPEED_LPF=1 LIVE_LAYOUTS=1 LAYOUT=7.1 $(MAKE) $(PAR) $(LIVE_RUNS)

check-runloop: $(TARGET)
	RUNLOOP_POLICY=1 $(MAKE) check-speed-lpf

check-auto: $(TARGET)
	AUTO_RUNLOOP=1 $(MAKE) check-runloop

# Every format through each layout, and the live set on each
WIDE_RUNS :=
define WIDE
$(eval $(call RUN,wide-$(1)-$(2)-i16,LAYOUT=$(1) WRAPPER=1 TRANSPORT=$(2) DEVICE_INT16=1))
$(eval $(call RUN,wide-$(1)-$(2)-f32,LAYOUT=$(1) WRAPPER=1 TRANSPORT=$(2) SOURCE_FLOAT=1))
$(eval $(call RUN,wide-$(1)-$(2)-mix,LAYOUT=$(1) WRAPPER=1 TRANSPORT=$(2)))
$(eval $(call RUN,wide-$(1)-$(2)-f32i16,LAYOUT=$(1) WRAPPER=1 TRANSPORT=$(2) SOURCE_FLOAT=1 DEVICE_INT16=1))
WIDE_RUNS += run-wide-$(1)-$(2)-i16 run-wide-$(1)-$(2)-f32 run-wide-$(1)-$(2)-mix run-wide-$(1)-$(2)-f32i16
endef
$(foreach l,5.1 7.1,$(foreach m,dry lpf stretch,$(eval $(call WIDE,$(l),$(m)))))
wide-live-5.1: $(TARGET)
	LAYOUT=5.1 $(MAKE) $(PAR) $(LIVE_RUNS)
wide-live-7.1: $(TARGET)
	LAYOUT=7.1 $(MAKE) $(PAR) $(LIVE_RUNS)
check-wide: $(TARGET)
	$(MAKE) $(PAR) $(WIDE_RUNS) wide-live-5.1 wide-live-7.1

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean all check check-consumer-exit check-kept-device check-transport check-wrapper check-lend check-tempo check-live check-wide check-layout check-speed-lpf check-runloop check-auto speed-lpf-live speed-lpf-layout wide-live-5.1 wide-live-7.1 run-lend-i16 run-lend-f32i16 run-lend-f32

check-consumer-exit: $(TARGET)
	$(MAKE) $(PAR) run-consumer-exit-1 run-consumer-exit-2
run-consumer-exit-1: $(TARGET)
	CONSUMER_EXIT=1 WRAPPER=1 TRANSPORT=dry ./$(TARGET) 0.25
run-consumer-exit-2: $(TARGET)
	CONSUMER_EXIT=1 WRAPPER=1 TRANSPORT=dry SOURCE_FLOAT=1 ./$(TARGET) 0.25

check-kept-device: $(TARGET)
	$(MAKE) $(PAR) run-kept-device-1 run-kept-device-2 run-kept-device-3 run-kept-device-4
run-kept-device-1: $(TARGET)
	KEPT_DEVICE=1 WRAPPER=1 ./$(TARGET) 0.25
run-kept-device-2: $(TARGET)
	KEPT_DEVICE=1 WRAPPER=1 SOURCE_FLOAT=1 ./$(TARGET) 0.25
run-kept-device-3: $(TARGET)
	KEPT_DROP=1 WRAPPER=1 ./$(TARGET) 0.25
run-kept-device-4: $(TARGET)
	KEPT_DROP=1 WRAPPER=1 SOURCE_FLOAT=1 ./$(TARGET) 0.25

check-pause: $(TARGET)
	$(MAKE) $(PAR) run-pause-1 run-pause-2 run-pause-3
run-pause-1: $(TARGET)
	PAUSE_BOUNDARY=1 WRAPPER=1 TRANSPORT=stretch DEVICE_INT16=1 ./$(TARGET) 0.25
run-pause-2: $(TARGET)
	PAUSE_BOUNDARY=1 WRAPPER=1 TRANSPORT=stretch ./$(TARGET) 0.25
run-pause-3: $(TARGET)
	PAUSE_BOUNDARY=1 WRAPPER=1 TRANSPORT=dry SOURCE_FLOAT=1 ./$(TARGET) 0.25

check-menu: $(TARGET)
	$(MAKE) $(PAR) run-menu-1 run-menu-2 run-menu-3 run-menu-4
run-menu-1: $(TARGET)
	MENU_SOURCE=1 WRAPPER=1 ./$(TARGET) 0.25
run-menu-2: $(TARGET)
	MENU_SOURCE=1 WRAPPER=1 SOURCE_FLOAT=1 ./$(TARGET) 0.25
run-menu-3: $(TARGET)
	MENU_SOURCE=1 WRAPPER=1 TRANSPORT=dry ./$(TARGET) 0.25
run-menu-4: $(TARGET)
	MENU_SOURCE=1 WRAPPER=1 TRANSPORT=dry DEVICE_INT16=1 ./$(TARGET) 0.25

.PHONY: check-menu check-pause run-consumer-exit-1 run-consumer-exit-2 run-kept-device-1 run-kept-device-2 run-kept-device-3 run-kept-device-4 run-pause-1 run-pause-2 run-pause-3 run-menu-1 run-menu-2 run-menu-3 run-menu-4
