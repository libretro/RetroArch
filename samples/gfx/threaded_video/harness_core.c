/* Minimal libretro core for the threaded video harness: supports
 * running without content, so the frontend treats it as a real core
 * (the menu can be closed over it, unlike the dummy core), and hands
 * the frontend a frame per retro_run, sometimes duplicated and
 * sometimes taller than the geometry it declared. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libretro.h>

#define W 320
#define H 240
#define H_OVERSIZE 600

static retro_video_refresh_t video_cb;
static retro_environment_t   environ_cb;
/* Room for a 32-bit oversize frame; the pixel size follows the
 * format chosen at set_environment. */
static uint8_t  frame[W * H_OVERSIZE * 4];
static unsigned bpp = 2;
static enum retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_RGB565;
static unsigned runs;

void retro_set_environment(retro_environment_t cb)
{
   bool no_content = true;
   environ_cb = cb;
   /* RGB565 by default; XRGB8888 with HARNESS_CORE_XRGB8888 set. The
    * two take different paths through a driver - Vulkan converts
    * RGB565 with a compute shader and samples a linear XRGB8888
    * image directly, and D3D12 lends its framebuffer only for a row
    * pitch on a 256-byte boundary, which 320 pixels reach at 4 bytes
    * and not at 2 - so the CI legs run the harness under both. */
   {
      const char *xrgb = getenv("HARNESS_CORE_XRGB8888");
      if (xrgb && *xrgb)
      {
         pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
         bpp          = 4;
      }
   }
   {
      /* Say what the frames below are, as a real core must. */
      enum retro_pixel_format fmt = pixel_format;
      cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   }
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void retro_init(void) { }
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name    = "threaded_video_harness";
   info->library_version = "1";
   info->valid_extensions = "";
}
void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps         = 60.0;
   info->timing.sample_rate = 48000.0;
   info->geometry.base_width   = W;
   info->geometry.base_height  = H;
   info->geometry.max_width    = W;
   info->geometry.max_height   = H;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
}
void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
void retro_reset(void) { }
/* Set by the harness through the core's own export below: when on,
 * retro_run asks the frontend for a framebuffer and renders into it,
 * exercising the wrapper's zero-copy lend. Counts how often the ask
 * was granted so the harness can check the lend actually happened.
 * Mode 2 renders the whole loan but pushes a cropped window into it -
 * a pointer past the start with the loan's pitch, the way a core that
 * crops overscan by offset does - which must still be a lend. */
#define CROP_X 8
#define CROP_Y 4
static int      harness_use_fb;
static unsigned harness_fb_granted;

/* RETRO_API, like the core's own entry points: a Windows DLL exports
 * only what is marked, and the harness looks these two up by name. */
RETRO_API void harness_core_use_framebuffer(int on) { harness_use_fb = on; }
RETRO_API unsigned harness_core_fb_granted(void)   { return harness_fb_granted; }

void retro_run(void)
{
   unsigned h = (runs % 61 == 60) ? H_OVERSIZE : H;
   unsigned i;
   uint8_t *dst   = frame;
   size_t   pitch = W * bpp;
   unsigned out_w = W, out_h = h;
   const uint8_t *push = NULL;
   runs++;

   if (harness_use_fb && h == H)
   {
      struct retro_framebuffer fb;
      memset(&fb, 0, sizeof(fb));
      fb.width        = W;
      fb.height       = H;
      /* Read as well as write, as a core that snapshots its frame
       * for a wipe asks: the lend must not be refused for it. */
      fb.access_flags = RETRO_MEMORY_ACCESS_WRITE
                      | RETRO_MEMORY_ACCESS_READ;
      if (     environ_cb(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &fb)
            && fb.format == pixel_format)
      {
         dst   = (uint8_t*)fb.data;
         pitch = fb.pitch;
         harness_fb_granted++;
      }
   }

   /* Pixel i carries (run + i) in its low bits, at either size, so a
    * lane reading the first pixel as 16-bit sees the run number
    * move whichever format is on. Rows at the pitch: a loan may pad
    * them. */
   for (i = 0; i < W * h; i++)
   {
      uint8_t *px = dst + (i / W) * pitch + (i % W) * bpp;
      if (bpp == 4)
         *(uint32_t*)px = (uint32_t)(runs + i);
      else
         *(uint16_t*)px = (uint16_t)(runs + i);
   }
   push = dst;
   if (harness_use_fb == 2 && h == H)
   {
      /* Cropped window: CROP_Y rows down and CROP_X pixels in, at the
       * full pitch. Same for a loan and for the core's own buffer. */
      push  = dst + CROP_Y * pitch + CROP_X * bpp;
      out_w = W - 2 * CROP_X;
      out_h = H - 2 * CROP_Y;
   }
   if (runs % 3 == 0)
      video_cb(NULL, out_w, out_h, pitch);
   else
      video_cb(push, out_w, out_h, pitch);
}
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }
bool retro_load_game(const struct retro_game_info *game) { (void)game; return true; }
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) { (void)type; (void)info; (void)num; return false; }
void retro_unload_game(void) { }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
