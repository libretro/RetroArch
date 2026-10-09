/* Minimal libretro core for the core info page harness: runs without
 * content and pushes a black frame per retro_run. Its info file is
 * written by the harness. */
#include <string.h>
#include <stdint.h>
#include <libretro.h>

static retro_video_refresh_t video_cb;
static uint16_t frame[64 * 64];

RETRO_API void retro_set_environment(retro_environment_t cb)
{
   bool no_game = true;
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
   info->library_name     = "Info Page Core";
   info->library_version  = "1";
   info->valid_extensions = "";
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width   = 64;
   info->geometry.base_height  = 64;
   info->geometry.max_width    = 64;
   info->geometry.max_height   = 64;
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 48000.0;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port; (void)device;
}
RETRO_API void retro_reset(void) { }
RETRO_API void retro_run(void) { video_cb(frame, 64, 64, 64 * sizeof(uint16_t)); }
RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void *data, size_t len) { (void)data; (void)len; return false; }
RETRO_API bool retro_unserialize(const void *data, size_t len) { (void)data; (void)len; return false; }
RETRO_API void retro_cheat_reset(void) { }
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index; (void)enabled; (void)code;
}
RETRO_API bool retro_load_game(const struct retro_game_info *game) { (void)game; return true; }
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
