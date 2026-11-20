/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <libretro.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#endif

static retro_environment_t fixture_environment;
static retro_video_refresh_t fixture_video;
static retro_audio_sample_batch_t fixture_audio;
static uint32_t fixture_pixels[160 * 120];
static int16_t fixture_pcm[800 * 2];
static bool fixture_ran;

void retro_set_environment(retro_environment_t cb)
{
   bool enabled = true;
   fixture_environment = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &enabled);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { fixture_video = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { fixture_audio = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void retro_init(void)
{
   unsigned i;
   fixture_ran = false;
   fprintf(stderr, "FRONTEND_FIXTURE_INIT\n");
   for (i = 0; i < 160 * 120; i++) fixture_pixels[i] = 0x004080c0;
   for (i = 0; i < 800 * 2; i++) fixture_pcm[i] = (i & 16) ? 4096 : -4096;
}
void retro_deinit(void) { fprintf(stderr, "FRONTEND_FIXTURE_DEINIT\n"); }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name = "PyroWave frontend oracle";
   info->library_version = "1";
   info->valid_extensions = "bin";
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width = info->geometry.max_width = 160;
   info->geometry.base_height = info->geometry.max_height = 120;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
   info->timing.fps = 60.0; info->timing.sample_rate = 48000.0;
}
bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
   (void)game;
   return fixture_environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);
}
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t count)
{ (void)type; (void)info; (void)count; return false; }
void retro_unload_game(void) { }
void retro_run(void)
{
   if (!fixture_ran)
   {
      fixture_ran = true;
      fprintf(stderr, "FRONTEND_FIXTURE_RUN\n");
   }
   /* Offline oracle: an occluded test window has no display pacing. */
#ifdef _WIN32
   Sleep(5);
#else
   poll(NULL, 0, 5);
#endif
   fixture_video(fixture_pixels, 160, 120, 160 * sizeof(uint32_t));
   fixture_audio(fixture_pcm, 800);
}
void retro_reset(void) { }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{ (void)index; (void)enabled; (void)code; }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
