compiler     := gcc
TARGET       := pipeline_wakeups_test

CORE_DIR = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) \
	-I$(CORE_DIR)/deps -I$(CORE_DIR)/deps/stb

CC      := $(compiler)
flags   += -std=gnu99 -g

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
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

%.o: %.c
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

check-tempo: $(TARGET)
	@for tempo in 0.25 0.5 1 2 4 8 16 32; do \
		WRAPPER=1 DEVICE_INT16=1 TRANSPORT=stretch TEMPO=$$tempo ./$(TARGET) 0.25 || exit 1; \
		WRAPPER=1 SOURCE_FLOAT=1 TRANSPORT=stretch TEMPO=$$tempo ./$(TARGET) 0.25 || exit 1; \
	done

check-live: $(TARGET)
	WRAPPER=1 LIVE_CONTROLS=1 TRANSPORT=stretch DEVICE_INT16=1 ./$(TARGET) 0.25
	WRAPPER=1 LIVE_CONTROLS=1 TRANSPORT=stretch SOURCE_FLOAT=1 ./$(TARGET) 0.25
	WRAPPER=1 LIVE_CONTROLS=1 TRANSPORT=stretch ./$(TARGET) 0.25
	WRAPPER=1 LIVE_CONTROLS=1 TRANSPORT=stretch SOURCE_FLOAT=1 DEVICE_INT16=1 ./$(TARGET) 0.25

check-layout: $(TARGET)
	LIVE_LAYOUTS=1 LAYOUT=7.1 $(MAKE) check-live

check-speed-lpf: $(TARGET)
	SPEED_LPF=1 $(MAKE) check-live
	SPEED_LPF=1 $(MAKE) check-layout

check-runloop: $(TARGET)
	RUNLOOP_POLICY=1 $(MAKE) check-speed-lpf

check-auto: $(TARGET)
	AUTO_RUNLOOP=1 $(MAKE) check-runloop

check-wide: $(TARGET)
	@for layout in 5.1 7.1; do \
		for mode in dry lpf stretch; do \
			LAYOUT=$$layout WRAPPER=1 TRANSPORT=$$mode DEVICE_INT16=1 ./$(TARGET) 0.25 || exit 1; \
			LAYOUT=$$layout WRAPPER=1 TRANSPORT=$$mode SOURCE_FLOAT=1 ./$(TARGET) 0.25 || exit 1; \
			LAYOUT=$$layout WRAPPER=1 TRANSPORT=$$mode ./$(TARGET) 0.25 || exit 1; \
			LAYOUT=$$layout WRAPPER=1 TRANSPORT=$$mode SOURCE_FLOAT=1 DEVICE_INT16=1 ./$(TARGET) 0.25 || exit 1; \
		done; \
		LAYOUT=$$layout $(MAKE) check-live || exit 1; \
	done

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean all check check-consumer-exit check-transport check-wrapper check-tempo check-live check-wide check-layout check-speed-lpf check-runloop check-auto

check-consumer-exit: $(TARGET)
	CONSUMER_EXIT=1 WRAPPER=1 TRANSPORT=dry ./$(TARGET) 0.25
	CONSUMER_EXIT=1 WRAPPER=1 TRANSPORT=dry SOURCE_FLOAT=1 ./$(TARGET) 0.25

check-pause: $(TARGET)
	PAUSE_BOUNDARY=1 WRAPPER=1 TRANSPORT=stretch DEVICE_INT16=1 ./$(TARGET) 0.25
	PAUSE_BOUNDARY=1 WRAPPER=1 TRANSPORT=stretch ./$(TARGET) 0.25
	PAUSE_BOUNDARY=1 WRAPPER=1 TRANSPORT=dry SOURCE_FLOAT=1 ./$(TARGET) 0.25
