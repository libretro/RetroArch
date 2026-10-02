/* A core with nothing but save RAM, for cloudsync_unload_test.  The
 * harness finds the RAM through harness_core_sram() and writes a
 * pattern into it, which only reaches disk when RetroArch flushes the
 * content's save files. */
#include <stdint.h>
#include <string.h>
#include <libretro.h>

#define SRAM_SIZE 256

static uint8_t sram[SRAM_SIZE];
static uint32_t frame[16 * 16];
static retro_video_refresh_t video_cb;
static retro_environment_t   environ_cb;

uint8_t *harness_core_sram(void) { return sram; }
size_t   harness_core_sram_size(void) { return SRAM_SIZE; }

void retro_set_environment(retro_environment_t cb)
{
   bool no_game = false;
   environ_cb   = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void retro_init(void) { memset(sram, 0, sizeof(sram)); }
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "sram_core";
   info->library_version  = "1";
   info->valid_extensions = "bin";
   info->need_fullpath    = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width   = 16;
   info->geometry.base_height  = 16;
   info->geometry.max_width    = 16;
   info->geometry.max_height   = 16;
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 48000.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
void retro_reset(void) { }
void retro_run(void) { video_cb(frame, 16, 16, 16 * sizeof(uint32_t)); }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }

bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   (void)game;
   return environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
}
bool retro_load_game_special(unsigned t, const struct retro_game_info *g, size_t n)
{ (void)t; (void)g; (void)n; return false; }
void retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

void *retro_get_memory_data(unsigned id)
{
   return id == RETRO_MEMORY_SAVE_RAM ? sram : NULL;
}
size_t retro_get_memory_size(unsigned id)
{
   return id == RETRO_MEMORY_SAVE_RAM ? SRAM_SIZE : 0;
}
