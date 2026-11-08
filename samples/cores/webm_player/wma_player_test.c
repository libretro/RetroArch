/* Copyright (C) 2026 The RetroArch team. MIT license. */
#include "../../../libretro-common/samples/formats/audio_transfer_wma/wma_fixture.h"
#include <libretro.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <streams/file_stream.h>
#include <string.h>

static unsigned float_mode, partial, shutdowns, videos, samples;
static unsigned xma_mode;
static unsigned source_rate, source_length, calls;
static const char *path = "wma-player-test-input.wma";

static void check(int valid, const char *what)
{
   if (!valid)
   {
      fprintf(stderr, "FAIL %s\n", what);
      exit(1);
   }
}
static double expected_sample(unsigned sample)
{
   const double pi = 3.14159265358979323846;
   double n = source_length;
   double transform = -cos(pi / n * (sample + 0.5 + n / 2) * 0.5);
   double window = sin((sample + 0.5) * pi / (2 * n));
   if (xma_mode) return 0;
   return (source_length / 8) * 100000.0 * (2.0 / n) / 32768 * transform * window;
}
static void interval(size_t frames)
{
   if (!partial)
   {
      unsigned expected = (calls + 1) * source_rate / 60 - calls * source_rate / 60;
      if (expected > source_length - samples)
         expected = source_length - samples;
      check(frames == expected, "rational source interval per run");
   }
   calls++;
}
static size_t audio_float(const float *data, size_t frames)
{
   size_t i, accepted = partial && frames > 120 ? 120 : frames;
   check(float_mode, "unexpected float callback");
   interval(frames);
   for (i = 0; i < accepted; i++)
   {
      double expected = expected_sample(samples + (unsigned)i);
      check(fabs(data[i * 2] - expected) < 0.000002, "consecutive float left");
      check(fabs(data[i * 2 + 1] - expected) < 0.000002, "consecutive float right");
   }
   samples += (unsigned)accepted;
   return accepted;
}
static size_t audio_s16(const int16_t *data, size_t frames)
{
   size_t i;
   check(!float_mode, "float delivery narrowed through legacy callback");
   interval(frames);
   for (i = 0; i < frames; i++)
   {
      double expected = expected_sample(samples + (unsigned)i) * 32768;
      check(fabs(data[i * 2] - expected) < 1.1, "legacy left");
      check(fabs(data[i * 2 + 1] - expected) < 1.1, "legacy right");
   }
   samples += (unsigned)frames;
   return frames;
}
static bool environment(unsigned cmd, void *data)
{
   if (cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT)
      return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_XRGB8888;
   if (cmd == RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_FLOAT && float_mode)
   {
      ((struct retro_audio_sample_float_callback *)data)->batch = audio_float;
      return true;
   }
   if (cmd == RETRO_ENVIRONMENT_SHUTDOWN)
   {
      shutdowns++;
      return true;
   }
   return false;
}
static void video(const void *data, unsigned width, unsigned height, size_t pitch)
{
   check(data != NULL && width == 1 && height == 1 && pitch == 4,
         "one audio-only video frame");
   videos++;
}
static void poll(void)
{
}
static int16_t input(unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)port;
   (void)device;
   (void)index;
   (void)id;
   return 0;
}

static void play(unsigned mode, unsigned fragmented, unsigned backpressure,
                 unsigned rate)
{
   struct retro_game_info game;
   struct retro_system_av_info av;
   RFILE *file;
   static uint8_t bytes[80+4096];
   size_t length;
   unsigned runs = 0;
   memset(&game, 0, sizeof(game));
   float_mode = mode;
   partial = backpressure;
   shutdowns = videos = samples = calls = 0;
   source_rate = rate;
   source_length = rate <= 16000 ? 512 : rate <= 22050 ? 1024 : 2048;
   if (xma_mode)
   { source_length=512; path="wma-player-test-input.xma"; length=xma_wave(bytes,3); }
   else length = fragmented ? asf(bytes, 1) : wave_rate(bytes, rate);
   check(!filestream_exists(path), "fixture path already exists");
   file = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
                          RETRO_VFS_FILE_ACCESS_HINT_NONE);
   check(file != NULL, "fixture VFS open");
   check(filestream_write(file, bytes, length) == (int64_t)length, "fixture VFS write");
   filestream_close(file);
   game.path = path;
#ifdef HAVE_RWMA
   check(retro_load_game(&game), "native media WMA load");
   retro_get_system_av_info(&av);
   check(av.timing.sample_rate == rate && av.timing.fps == 60,
         "source-rate media pacing");
   check(retro_serialize_size() == 0, "unsupported position state not advertised");
   retro_run();
   retro_reset();
   samples = videos = shutdowns = calls = 0;
   while (!shutdowns && runs < 100)
   {
      retro_run();
      runs++;
   }
   check(shutdowns == 1 && samples == source_length, "exact source sample delivery");
   check(videos == runs, "one video callback per run");
   retro_unload_game();
#else
   (void)av;
   (void)runs;
   check(!retro_load_game(&game), "disabled WMA media load");
#endif
   check(filestream_delete(path) == 0, "fixture VFS delete");
}
int main(void)
{
   retro_set_environment(environment);
   retro_set_video_refresh(video);
   retro_set_audio_sample_batch(audio_s16);
   retro_set_input_poll(poll);
   retro_set_input_state(input);
   retro_init();
   play(0, 0, 0, 48000);
   play(1, 0, 0, 48000);
   play(1, 1, 1, 48000);
   play(1, 0, 0, 8000);
   play(0, 0, 0, 44100);
   xma_mode=1;
   play(1,0,0,48000);
   play(0,0,1,48000);
   retro_deinit();
#ifdef HAVE_RWMA
   puts("PASS native WMA player float, legacy, reset and callback backpressure");
#else
   puts("PASS WMA player disabled with valid WMA input");
#endif
   return 0;
}
