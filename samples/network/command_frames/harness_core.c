
#include <string.h>
#include <stdint.h>
#include <libretro.h>
static retro_video_refresh_t video;
static retro_input_poll_t poll;
static retro_input_state_t input;
static uint32_t frame;
static uint32_t pixels[64];
void retro_set_environment(retro_environment_t cb)
{
   bool no_game=true;
   enum retro_pixel_format fmt=RETRO_PIXEL_FORMAT_XRGB8888;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME,&no_game);
   cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&fmt);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video=cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll=cb; }
void retro_set_input_state(retro_input_state_t cb) { input=cb; }
void retro_init(void) { frame=0; }
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *i)
{
   memset(i,0,sizeof(*i)); i->library_name="command_frames_harness";
   i->library_version="1"; i->valid_extensions="";
}
void retro_get_system_av_info(struct retro_system_av_info *i)
{
   memset(i,0,sizeof(*i)); i->geometry.base_width=i->geometry.max_width=8;
   i->geometry.base_height=i->geometry.max_height=8;
   i->timing.fps=60.0; i->timing.sample_rate=48000.0;
}
void retro_set_controller_port_device(unsigned p,unsigned d) { (void)p; (void)d; }
void retro_reset(void) { frame=0; }
void retro_run(void)
{
   poll(); input(0,RETRO_DEVICE_JOYPAD,0,RETRO_DEVICE_ID_JOYPAD_B);
   frame++; pixels[0]=frame; video(pixels,8,8,8*sizeof(*pixels));
}
size_t retro_serialize_size(void) { return sizeof(frame); }
bool retro_serialize(void *p,size_t n) { if(n<sizeof(frame))return false; memcpy(p,&frame,sizeof(frame));return true; }
bool retro_unserialize(const void *p,size_t n) { if(n<sizeof(frame))return false; memcpy(&frame,p,sizeof(frame));return true; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned i,bool b,const char *c) { (void)i;(void)b;(void)c; }
bool retro_load_game(const struct retro_game_info *g) { (void)g;return true; }
bool retro_load_game_special(unsigned t,const struct retro_game_info *g,size_t n) { (void)t;(void)g;(void)n;return false; }
void retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { return id==RETRO_MEMORY_SYSTEM_RAM?&frame:NULL; }
size_t retro_get_memory_size(unsigned id) { return id==RETRO_MEMORY_SYSTEM_RAM?sizeof(frame):0; }
