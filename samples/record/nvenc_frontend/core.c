/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <libretro.h>
#define WIDTH 320
#define HEIGHT 192
static retro_video_refresh_t video;
static retro_environment_t environment;
static struct retro_audio_sample_multi_callback multi;
static unsigned layout = RETRO_AUDIO_LAYOUT_STEREO, channels = 2;
static bool floating;
static uint32_t pixels[WIDTH * HEIGHT];
static unsigned frames;
static unsigned sample_rate = 48000;
static retro_audio_sample_batch_t audio;
static int16_t samples[16016];
static float float_samples[16016];
void retro_set_environment(retro_environment_t cb)
{
   bool yes = true;
   enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
   environment = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &yes);
   cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void retro_init(void)
{
   const char *rate = getenv("NATIVE_RECORD_RATE");
   const char *mask = getenv("NATIVE_RECORD_LAYOUT");
   unsigned bits;
   layout = mask ? (unsigned)strtoul(mask, NULL, 0) : RETRO_AUDIO_LAYOUT_STEREO;
   channels = 0;
   for (bits = layout; bits; bits >>= 1) channels += bits & 1;
   floating = getenv("NATIVE_RECORD_FLOAT") != NULL;
   frames = 0;
   sample_rate = rate ? (unsigned)strtoul(rate, NULL, 10) : 48000;
}
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name = "native_record_fixture";
   info->library_version = "1"; info->valid_extensions = "";
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps = 60000.0 / 1001.0; info->timing.sample_rate = sample_rate;
   info->geometry.base_width = info->geometry.max_width = WIDTH;
   info->geometry.base_height = info->geometry.max_height = HEIGHT;
   info->geometry.aspect_ratio = (float)WIDTH / HEIGHT;
}
void retro_run(void)
{
   static const uint32_t colors[] = {0xffff2020u, 0xff20ff20u, 0xff2020ffu, 0xffc0c0c0u};
   static const unsigned frequencies[] = {440, 660, 880, 80, 1000, 1200, 1400, 1600};
   unsigned i, c;
   uint64_t first = (uint64_t)frames * sample_rate * 1001 / 60000;
   size_t count = (size_t)((uint64_t)(frames + 1) * sample_rate * 1001 / 60000 - first);
   if (count <= 2002 && channels && channels <= 8)
   {
      for (i = 0; i < count; i++)
      {
         double phase = 6.283185307179586 * (double)(first + i) / sample_rate;
         for (c = 0; c < channels; c++)
         {
            float value = (float)(0.2 * sin(phase * frequencies[c]));
            samples[i * channels + c] = (int16_t)(value * 32768);
            float_samples[i * channels + c] = value;
         }
      }
      if (floating && multi.batch_float)
         multi.batch_float(float_samples, count, channels, layout);
      else if (multi.batch_int16)
         multi.batch_int16(samples, count, channels, layout);
      else if (audio) audio(samples, count);
   }
   for (i = 0; i < WIDTH * HEIGHT; i++) pixels[i] = colors[(frames / 15) % 4];
   frames++;
   video(pixels, WIDTH, HEIGHT, WIDTH * sizeof(*pixels));
}
void retro_set_controller_port_device(unsigned p, unsigned d) { (void)p; (void)d; }
void retro_reset(void) { frames = 0; }
bool retro_load_game(const struct retro_game_info *info)
{
   struct retro_audio_sample_float_callback cb;
   (void)info;
   if (floating && !environment(RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_FLOAT, &cb))
      return false;
   if (!environment(RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_MULTI, &multi)
         || (floating ? !multi.batch_float : !multi.batch_int16)) return false;
   fprintf(stderr, "[fixture] %s source, mask %x\n", floating ? "float" : "s16", layout);
   return true;
}
bool retro_load_game_special(unsigned n, const struct retro_game_info *i, size_t c)
{ (void)n; (void)i; (void)c; return false; }
void retro_unload_game(void) { }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *p, size_t n) { (void)p; (void)n; return false; }
bool retro_unserialize(const void *p, size_t n) { (void)p; (void)n; return false; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned i, bool e, const char *s) { (void)i; (void)e; (void)s; }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned i) { (void)i; return NULL; }
size_t retro_get_memory_size(unsigned i) { (void)i; return 0; }
