/* Test core for gl1_upload_test.py.
 *
 * Emits a fixed pattern in RGB565 or XRGB8888 (GL1_UPLOAD_FMT=565 or
 * 8888) at an odd size, from a buffer whose rows are padded with
 * white pixels that must never reach the screen.  With
 * GL1_UPLOAD_RESIZE set it alternates every seven frames between the
 * full size and a smaller one with a different power-of-two texture
 * size, so the driver has to respecify its texture storage. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>

#define FRAME_W  317
#define FRAME_H  223
#define SMALL_W  150
#define SMALL_H  120
#define ROW_PAD  11
#define ROW_PX   (FRAME_W + ROW_PAD)

static retro_video_refresh_t video_cb;
static retro_environment_t   environ_cb;
static int                   fmt_565;
static int                   resize;
static unsigned              frame_count;
static uint16_t              buf16[FRAME_H][ROW_PX];
static uint32_t              buf32[FRAME_H][ROW_PX];

/* The same hash the test recomputes for its expected image. */
static uint32_t pattern(unsigned x, unsigned y)
{
   uint32_t v = (uint32_t)(x * 7 + y * 13 + x * y);
   return v * 2654435761u;
}

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

RETRO_API void retro_init(void)
{
   unsigned x, y;
   const char *fmt = getenv("GL1_UPLOAD_FMT");
   fmt_565         = fmt && !strcmp(fmt, "565");
   resize          = getenv("GL1_UPLOAD_RESIZE") != NULL;

   for (y = 0; y < FRAME_H; y++)
   {
      for (x = 0; x < ROW_PX; x++)
      {
         uint32_t p   = pattern(x, y);
         int      pad = (x >= FRAME_W);
         buf16[y][x]  = pad ? 0xffff : (uint16_t)(p >> 16);
         buf32[y][x]  = pad ? 0xffffffffu : (0xff000000u | (p & 0xffffff));
      }
   }
}

RETRO_API void retro_deinit(void) { }
RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }

RETRO_API void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name    = "gl1_upload";
   info->library_version = "1";
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width   = FRAME_W;
   info->geometry.base_height  = FRAME_H;
   info->geometry.max_width    = FRAME_W;
   info->geometry.max_height   = FRAME_H;
   info->geometry.aspect_ratio = (float)FRAME_W / FRAME_H;
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 48000.0;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

RETRO_API void retro_reset(void) { }

RETRO_API void retro_run(void)
{
   unsigned w = FRAME_W;
   unsigned h = FRAME_H;
   if (resize && ((frame_count / 7) & 1))
   {
      w = SMALL_W;
      h = SMALL_H;
   }
   frame_count++;
   if (fmt_565)
      video_cb(buf16, w, h, sizeof(buf16[0]));
   else
      video_cb(buf32, w, h, sizeof(buf32[0]));
}

RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
RETRO_API bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
RETRO_API void retro_cheat_reset(void) { }
RETRO_API void retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }

RETRO_API bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format pf = fmt_565
      ? RETRO_PIXEL_FORMAT_RGB565 : RETRO_PIXEL_FORMAT_XRGB8888;
   (void)game;
   return environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pf);
}

RETRO_API bool retro_load_game_special(unsigned type,
      const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

RETRO_API void retro_unload_game(void) { }
RETRO_API unsigned retro_get_region(void) { return 0; }
RETRO_API void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
RETRO_API size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
