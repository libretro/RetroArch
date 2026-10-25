
#include <string.h>
#include <stdint.h>
#include <libretro.h>
static retro_video_refresh_t video;
static retro_input_poll_t poll;
static retro_input_state_t input;
static uint32_t frame;
static uint32_t pixels[64];
/* Memory for READ_CORE_MEMORY and GET_MEMORY_MAP to answer about, at a
 * system address, as a real core maps its RAM. ram[7] follows the frame
 * counter, so RUN_UNTIL has a byte that changes to wait on; ram[9]
 * counts retro_set_environment calls after retro_init, which the
 * frontend must not make; the rest stays as retro_init left it, which
 * the memory tests read. */
static uint8_t ram[16];
/* a second region straight after the first, at 1010, as a core that
 * splits its RAM maps it: a read may run across the two */
static uint8_t ram2[16];
static int inited;
void retro_set_environment(retro_environment_t cb)
{
   if (inited)
      ram[9]++;
   bool no_game=true;
   enum retro_pixel_format fmt=RETRO_PIXEL_FORMAT_XRGB8888;
   static struct retro_memory_descriptor desc[2];
   static struct retro_memory_map map;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME,&no_game);
   cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&fmt);
   memset(desc,0,sizeof(desc));
   desc[0].ptr=ram; desc[0].len=sizeof(ram); desc[0].start=0x1000;
   desc[0].select=0; desc[0].addrspace="ram";
   desc[0].flags=RETRO_MEMDESC_SYSTEM_RAM;
   desc[1].ptr=ram2; desc[1].len=sizeof(ram2); desc[1].start=0x1010;
   desc[1].select=0; desc[1].addrspace="ram";
   desc[1].flags=RETRO_MEMDESC_SYSTEM_RAM;
   map.descriptors=desc; map.num_descriptors=2;
   /* built with HARNESS_NO_MAP, it publishes no map, as snes9x does, and
    * hands its RAM over through retro_get_memory_data alone */
#ifndef HARNESS_NO_MAP
   cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS,&map);
#endif
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video=cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll=cb; }
void retro_set_input_state(retro_input_state_t cb) { input=cb; }
void retro_init(void)
{
   unsigned i;
   frame=0;
   /* 00 01 02 ... so a read's bytes say which ones they are */
   for (i=0;i<sizeof(ram);i++) ram[i]=(uint8_t)i;
   for (i=0;i<sizeof(ram2);i++) ram2[i]=(uint8_t)(0x10 + i);
   ram[9]=0; inited=1;
}
void retro_deinit(void) { inited=0; }
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
   ram[7]=(uint8_t)frame;
}
/* the frame counter, padded to 8 bytes: a state is never shorter than
 * the header the frontend looks for in one */
#define STATE_SIZE 8
size_t retro_serialize_size(void) { return STATE_SIZE; }
bool retro_serialize(void *p,size_t n) { if(n<STATE_SIZE)return false; memset(p,0,STATE_SIZE); memcpy(p,&frame,sizeof(frame));return true; }
bool retro_unserialize(const void *p,size_t n) { if(n<STATE_SIZE)return false; memcpy(&frame,p,sizeof(frame));return true; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned i,bool b,const char *c) { (void)i;(void)b;(void)c; }
bool retro_load_game(const struct retro_game_info *g) { (void)g;return true; }
bool retro_load_game_special(unsigned t,const struct retro_game_info *g,size_t n) { (void)t;(void)g;(void)n;return false; }
void retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
#ifdef HARNESS_NO_MAP
void *retro_get_memory_data(unsigned id) { return id==RETRO_MEMORY_SYSTEM_RAM?(void*)ram:id==RETRO_MEMORY_SAVE_RAM?(void*)ram2:NULL; }
size_t retro_get_memory_size(unsigned id) { return id==RETRO_MEMORY_SYSTEM_RAM?sizeof(ram):id==RETRO_MEMORY_SAVE_RAM?sizeof(ram2):0; }
#else
void *retro_get_memory_data(unsigned id) { return id==RETRO_MEMORY_SYSTEM_RAM?&frame:NULL; }
size_t retro_get_memory_size(unsigned id) { return id==RETRO_MEMORY_SYSTEM_RAM?sizeof(frame):0; }
#endif
