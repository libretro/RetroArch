/* The core for the frontend-overhead harness: the cheapest thing a
 * real core does every frame, and nothing else. One 320x240 RGB565
 * frame, one 735-frame stereo audio batch (NTSC at 44.1 kHz), one
 * input poll, one input read. Every other cost in the run is the
 * frontend's. */
#include <string.h>
#include <stdint.h>
#include <libretro.h>

#define W 320
#define H 240
#define AUDIO_FRAMES 735

static retro_video_refresh_t       video_cb;
static retro_audio_sample_batch_t  audio_cb;
static retro_input_poll_t          poll_cb;
static retro_input_state_t         state_cb;
static retro_environment_t         environ_cb;
static uint16_t frame[W * H];
static int16_t  audio[AUDIO_FRAMES * 2];
static unsigned runs;

void retro_set_environment(retro_environment_t cb)
{
   bool no_content = true;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
}
void retro_set_video_refresh(retro_video_refresh_t cb)      { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)        { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb)            { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb)          { state_cb = cb; }
void retro_init(void) { }
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "frontend_overhead_harness";
   info->library_version  = "1";
   info->valid_extensions = "";
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 44100.0;
   info->geometry.base_width   = W;
   info->geometry.base_height  = H;
   info->geometry.max_width    = W;
   info->geometry.max_height   = H;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
}
void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
void retro_reset(void) { }

void retro_run(void)
{
   /* One pixel changes per frame so nothing upstream can treat the
    * frame as a duplicate. */
   frame[runs % (W * H)] ^= 0xffff;
   runs++;
   poll_cb();
   state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
   video_cb(frame, W, H, W * 2);
   audio_cb(audio, AUDIO_FRAMES);
}

bool   retro_load_game(const struct retro_game_info *game) { (void)game; return true; }
void   retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
bool   retro_load_game_special(unsigned t, const struct retro_game_info *i, size_t n) { (void)t; (void)i; (void)n; return false; }
size_t retro_serialize_size(void) { return sizeof(frame); }
bool   retro_serialize(void *d, size_t n)   { if (n < sizeof(frame)) return false; memcpy(d, frame, sizeof(frame)); return true; }
bool   retro_unserialize(const void *d, size_t n) { if (n < sizeof(frame)) return false; memcpy(frame, d, sizeof(frame)); return true; }
void   retro_cheat_reset(void) { }
void   retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }
void  *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
