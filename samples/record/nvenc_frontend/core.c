/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>
#include <stdint.h>
#include <libretro.h>
#define WIDTH 320
#define HEIGHT 192
static retro_video_refresh_t video;
static uint32_t pixels[WIDTH * HEIGHT];
static unsigned frames;
void retro_set_environment(retro_environment_t cb)
{
   bool yes = true;
   enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &yes);
   cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void retro_init(void) { frames = 0; }
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
   info->timing.fps = 60000.0 / 1001.0; info->timing.sample_rate = 48000.0;
   info->geometry.base_width = info->geometry.max_width = WIDTH;
   info->geometry.base_height = info->geometry.max_height = HEIGHT;
   info->geometry.aspect_ratio = (float)WIDTH / HEIGHT;
}
void retro_run(void)
{
   static const uint32_t colors[] = {0xffff2020u, 0xff20ff20u, 0xff2020ffu, 0xffc0c0c0u};
   unsigned i;
   for (i = 0; i < WIDTH * HEIGHT; i++) pixels[i] = colors[(frames / 15) % 4];
   frames++;
   video(pixels, WIDTH, HEIGHT, WIDTH * sizeof(*pixels));
}
void retro_set_controller_port_device(unsigned p, unsigned d) { (void)p; (void)d; }
void retro_reset(void) { frames = 0; }
bool retro_load_game(const struct retro_game_info *info) { (void)info; return true; }
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
