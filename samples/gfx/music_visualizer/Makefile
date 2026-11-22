CC ?= cc
CFLAGS ?= -O2
CFLAGS += -std=c89 -pedantic -Wall -Wextra -Werror -Wframe-larger-than=4096
ROOT := ../../..

.PHONY: all test clean
all: visualizer_test visualizer_thread_test visualizer_lifetime_test
visualizer_test: visualizer_test.c $(ROOT)/gfx/gfx_music_visualizer.c $(ROOT)/gfx/gfx_music_visualizer.h $(ROOT)/libretro-common/audio/dsp_filters/fft/fft.c $(ROOT)/libretro-common/queues/retro_spsc.c
	$(CC) $(CFLAGS) -I$(ROOT)/libretro-common/include $< -lm -o $@
visualizer_thread_test: thread_test.c $(ROOT)/gfx/gfx_music_visualizer.c $(ROOT)/gfx/gfx_music_visualizer.h $(ROOT)/libretro-common/audio/dsp_filters/fft/fft.c $(ROOT)/libretro-common/queues/retro_spsc.c
	$(CC) $(CFLAGS) -I$(ROOT)/libretro-common/include $(filter %.c,$^) -lm -pthread -o $@
visualizer_lifetime_test: lifetime_test.c $(ROOT)/gfx/gfx_music_player.c $(ROOT)/gfx/gfx_music_player.h $(ROOT)/gfx/gfx_music_visualizer.c $(ROOT)/gfx/gfx_music_visualizer.h $(ROOT)/libretro-common/audio/dsp_filters/fft/fft.c $(ROOT)/libretro-common/queues/retro_spsc.c
	$(CC) $(CFLAGS) -DHAVE_MENU -DHAVE_AUDIOMIXER -ffunction-sections -I$(ROOT)/libretro-common/include $(filter %.c,$^) -Wl,--gc-sections -Wl,--wrap=gfx_music_visualizer_free -lm -pthread -o $@
test: all
	./visualizer_test
	./visualizer_thread_test
	./visualizer_lifetime_test
clean:
	rm -f visualizer_test visualizer_test.exe
	rm -f visualizer_thread_test visualizer_thread_test.exe
	rm -f visualizer_lifetime_test visualizer_lifetime_test.exe
