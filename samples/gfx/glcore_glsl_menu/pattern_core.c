/* A test core for run.py: a 320x240 frame, its top quarter blue and the
 * rest red on the left, green on the right - so a frame drawn upside
 * down, cropped, stretched or not at all shows. With PATTERN_HW set it
 * draws on a GL 3.2 core-profile hardware context, by scissored clears
 * with every GL entry point from the frontend; otherwise it hands over
 * an XRGB8888 software frame. With PATTERN_BIG set the frame is 640x480,
 * larger than the 512 texels a 320x240 frame fits in. With PATTERN_FLIP
 * set a software frame swaps its red and green halves every frame, for
 * shaders that read the frame before. It needs no content. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libretro.h>

#define MAX_W 640
#define MAX_H 480
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_SCISSOR_TEST 0x0C11

static retro_video_refresh_t video_cb;
static retro_environment_t env_cb;
static retro_input_poll_t poll_cb;
static struct retro_hw_render_callback hw;
static int use_hw, have_ctx, flip;
static unsigned frames;
static uint32_t fb_flipped[MAX_W * MAX_H];
static uint32_t fb[MAX_W * MAX_H];
static unsigned W = 320, H = 240;

typedef void (*f_bind)(unsigned, unsigned);
typedef void (*f_vp)(int, int, int, int);
typedef void (*f_cc)(float, float, float, float);
typedef void (*f_clear)(unsigned);
typedef void (*f_en)(unsigned);
static f_bind gbind; static f_vp gvp, gsc; static f_cc gcc; static f_clear gclear;
static f_en gen, gdis;

static void reset(void)
{
   gbind  = (f_bind)hw.get_proc_address("glBindFramebuffer");
   gvp    = (f_vp)hw.get_proc_address("glViewport");
   gsc    = (f_vp)hw.get_proc_address("glScissor");
   gcc    = (f_cc)hw.get_proc_address("glClearColor");
   gclear = (f_clear)hw.get_proc_address("glClear");
   gen    = (f_en)hw.get_proc_address("glEnable");
   gdis   = (f_en)hw.get_proc_address("glDisable");
   have_ctx = gbind && gvp && gsc && gcc && gclear && gen && gdis;
   fprintf(stderr, "pattern_core: context %s\n", have_ctx ? "ok" : "missing");
}
static void destroy(void) { have_ctx = 0; }

void retro_set_environment(retro_environment_t cb)
{
   bool no_game = true;
   env_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void retro_init(void)
{
   use_hw = getenv("PATTERN_HW") != NULL;
   flip   = getenv("PATTERN_FLIP") != NULL;
   if (getenv("PATTERN_BIG"))
   {
      W = MAX_W;
      H = MAX_H;
   }
}
void retro_deinit(void) {}
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *i)
{
   memset(i, 0, sizeof(*i));
   i->library_name = "pattern"; i->library_version = "1";
   i->need_fullpath = false; i->valid_extensions = "";
}
void retro_get_system_av_info(struct retro_system_av_info *i)
{
   memset(i, 0, sizeof(*i));
   i->geometry.base_width = W; i->geometry.base_height = H;
   i->geometry.max_width = W; i->geometry.max_height = H;
   i->geometry.aspect_ratio = (float)W / H;
   i->timing.fps = 60.0; i->timing.sample_rate = 48000.0;
}
void retro_set_controller_port_device(unsigned p, unsigned d) { (void)p; (void)d; }
void retro_reset(void) {}
bool retro_load_game(const struct retro_game_info *g)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   (void)g;
   if (!env_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
      return false;
   if (use_hw)
   {
      memset(&hw, 0, sizeof(hw));
      hw.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
      hw.version_major = 3; hw.version_minor = 2;
      hw.context_reset = reset; hw.context_destroy = destroy;
      hw.bottom_left_origin = true;
      if (!env_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw))
      {
         fprintf(stderr, "pattern_core: no hw render\n");
         return false;
      }
   }
   else
   {
      unsigned x, y;
      for (y = 0; y < H; y++)
         for (x = 0; x < W; x++)
         {
            fb[y * W + x] = y < H / 4 ? 0x0000FFu
               : (x < W / 2 ? 0xFF0000u : 0x00FF00u);
            fb_flipped[y * W + x] = y < H / 4 ? 0x0000FFu
               : (x < W / 2 ? 0x00FF00u : 0xFF0000u);
         }
   }
   return true;
}
void retro_run(void)
{
   poll_cb();
   if (!use_hw)
   {
      video_cb((flip && (frames++ & 1)) ? fb_flipped : fb, W, H, W * 4);
      return;
   }
   if (!have_ctx)
   {
      video_cb(NULL, W, H, 0);
      return;
   }
   gbind(GL_FRAMEBUFFER, (unsigned)hw.get_current_framebuffer());
   gvp(0, 0, W, H);
   gen(GL_SCISSOR_TEST);
   gsc(0, 0, W / 2, H);
   gcc(1, 0, 0, 1);
   gclear(GL_COLOR_BUFFER_BIT);
   gsc(W / 2, 0, W - W / 2, H);
   gcc(0, 1, 0, 1);
   gclear(GL_COLOR_BUFFER_BIT);
   /* The top quarter blue: bottom-left origin, so the high rows */
   gsc(0, H - H / 4, W, H / 4);
   gcc(0, 0, 1, 1);
   gclear(GL_COLOR_BUFFER_BIT);
   gdis(GL_SCISSOR_TEST);
   video_cb(RETRO_HW_FRAME_BUFFER_VALID, W, H, 0);
}
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *d, size_t s) { (void)d; (void)s; return false; }
bool retro_unserialize(const void *d, size_t s) { (void)d; (void)s; return false; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }
bool retro_load_game_special(unsigned t, const struct retro_game_info *g, size_t n)
{ (void)t; (void)g; (void)n; return false; }
void retro_unload_game(void) {}
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
