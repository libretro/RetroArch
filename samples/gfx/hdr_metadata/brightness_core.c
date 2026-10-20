/* Fixed SDR patches; the unused XRGB byte is deliberately zero. */
#include <string.h>
#include <stdint.h>
#include <libretro.h>
static retro_video_refresh_t video;
static retro_input_poll_t poll;
static uint32_t pixels[256*192];
void retro_set_environment(retro_environment_t cb)
{
   bool no_game=true;
   enum retro_pixel_format fmt=RETRO_PIXEL_FORMAT_XRGB8888;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME,&no_game);
   cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&fmt);
}
void retro_set_video_refresh(retro_video_refresh_t cb){video=cb;}
void retro_set_audio_sample(retro_audio_sample_t cb){(void)cb;}
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb){(void)cb;}
void retro_set_input_poll(retro_input_poll_t cb){poll=cb;}
void retro_set_input_state(retro_input_state_t cb){(void)cb;}
void retro_init(void){}
void retro_deinit(void){}
unsigned retro_api_version(void){return RETRO_API_VERSION;}
void retro_get_system_info(struct retro_system_info *i)
{
   memset(i,0,sizeof(*i));i->library_name="HDR brightness conformance";
   i->library_version="1";i->valid_extensions="";
}
void retro_get_system_av_info(struct retro_system_av_info *i)
{
   memset(i,0,sizeof(*i));i->geometry.base_width=i->geometry.max_width=256;
   i->geometry.base_height=i->geometry.max_height=192;
   i->timing.fps=60.0;i->timing.sample_rate=48000.0;
}
void retro_set_controller_port_device(unsigned p,unsigned d){(void)p;(void)d;}
void retro_reset(void){}
void retro_run(void)
{
   static const uint32_t colors[8]={0xe0e0e0,0xf0f0f0,0xf8f8f8,0xffffff,0xff0000,0x00ff00,0x0000ff,0xffff00};
   unsigned x,y;
   if(poll)poll();
   for(y=0;y<192;y++)for(x=0;x<256;x++)pixels[y*256+x]=colors[x/32];
   video(pixels,256,192,256*sizeof(*pixels));
}
size_t retro_serialize_size(void){return 0;}
bool retro_serialize(void *p,size_t n){(void)p;(void)n;return false;}
bool retro_unserialize(const void *p,size_t n){(void)p;(void)n;return false;}
void retro_cheat_reset(void){}
void retro_cheat_set(unsigned i,bool b,const char *c){(void)i;(void)b;(void)c;}
bool retro_load_game(const struct retro_game_info *g){(void)g;return true;}
bool retro_load_game_special(unsigned t,const struct retro_game_info *g,size_t n){(void)t;(void)g;(void)n;return false;}
void retro_unload_game(void){}
unsigned retro_get_region(void){return RETRO_REGION_NTSC;}
void *retro_get_memory_data(unsigned i){(void)i;return NULL;}
size_t retro_get_memory_size(unsigned i){(void)i;return 0;}
