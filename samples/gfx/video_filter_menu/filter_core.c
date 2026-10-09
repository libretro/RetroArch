/* Minimal libretro core for the video filter menu harness: runs
 * without content and pushes a 240x160 RGB565 frame per retro_run, the
 * shape of a handheld core a filter is put on. The picture is the
 * same every frame. */
#include <string.h>
#include <stdint.h>
#include <libretro.h>

#define W 240
#define H 160

static retro_video_refresh_t video_cb;
static retro_environment_t   environ_cb;
static uint16_t frame[W * H];

RETRO_API void retro_set_environment(retro_environment_t cb)
{
   bool no_game = true;
   environ_cb   = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
}
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
RETRO_API void retro_init(void) { }
RETRO_API void retro_deinit(void) { }
RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }

RETRO_API void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "video_filter_menu_core";
   info->library_version  = "1";
   info->valid_extensions = "";
   info->need_fullpath    = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width   = W;
   info->geometry.base_height  = H;
   info->geometry.max_width    = W;
   info->geometry.max_height   = H;
   info->geometry.aspect_ratio = 3.0f / 2.0f;
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 48000.0;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port; (void)device;
}
RETRO_API void retro_reset(void) { }

RETRO_API void retro_run(void)
{
   /* The same picture every frame, so frames can be compared */
   unsigned x, y;
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
         frame[y * W + x] = (uint16_t)(((x * 31 / W) << 11)
               | ((y * 63 / H) << 5) | ((x + y) & 31));
   video_cb(frame, W, H, W * sizeof(uint16_t));
}

RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void *data, size_t len) { (void)data; (void)len; return false; }
RETRO_API bool retro_unserialize(const void *data, size_t len) { (void)data; (void)len; return false; }
RETRO_API void retro_cheat_reset(void) { }
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index; (void)enabled; (void)code;
}

RETRO_API bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   (void)game;
   return environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
}
RETRO_API bool retro_load_game_special(unsigned type,
      const struct retro_game_info *info, size_t num)
{
   (void)type; (void)info; (void)num;
   return false;
}
RETRO_API void retro_unload_game(void) { }
RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
RETRO_API void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
RETRO_API size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
