/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (pattern_core.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* A libretro core that draws one deterministic test pattern: the
 * image the shader image check feeds through a preset under each
 * slang compiler.  Every frame is the same picture (the frame counter
 * is not drawn), so two captures of any frame compare exactly, and
 * the picture has what shader differences show up in - ramps for
 * precision, a checkerboard for sampling and filtering, hard edges
 * for derivatives, saturated primaries and a grey wedge for colour
 * math.  XRGB8888, 320x240, 60 Hz, no audio, no input.  No content
 * is needed: the core reports RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>

#define PATTERN_W 320
#define PATTERN_H 240

static uint32_t                     pattern_fb[PATTERN_W * PATTERN_H];
static retro_video_refresh_t        pattern_video_cb;
static retro_environment_t          pattern_environ_cb;
static retro_input_poll_t           pattern_input_poll_cb;
static retro_input_state_t          pattern_input_state_cb;
static retro_audio_sample_t         pattern_audio_cb;
static retro_audio_sample_batch_t   pattern_audio_batch_cb;

static uint32_t pattern_pixel(unsigned x, unsigned y)
{
   unsigned r, g, b;

   if (y < 60)
   {
      /* saturated bars: R G B C M Y W K, each 40 wide */
      static const uint32_t bars[8] = {
         0xff0000, 0x00ff00, 0x0000ff, 0x00ffff,
         0xff00ff, 0xffff00, 0xffffff, 0x000000 };
      return bars[(x / 40) & 7];
   }
   if (y < 120)
   {
      /* horizontal ramps: red, green, blue, grey by row band */
      unsigned v    = (x * 255) / (PATTERN_W - 1);
      unsigned band = (y - 60) / 15;
      r = band == 0 || band == 3 ? v : 0;
      g = band == 1 || band == 3 ? v : 0;
      b = band == 2 || band == 3 ? v : 0;
      return (r << 16) | (g << 8) | b;
   }
   if (y < 180)
   {
      /* checkerboards of 1, 2, 4 and 8 texel cells across the width */
      unsigned cell = 1u << ((x / 80) & 3);
      unsigned on   = ((x / cell) ^ (y / cell)) & 1;
      unsigned v    = on ? 0xe0 : 0x20;
      return (v << 16) | (v << 8) | v;
   }
   /* diagonal edges and a grey wedge */
   if (x < 160)
   {
      unsigned on = ((x + y) / 12) & 1;
      return on ? 0xff8000 : 0x0040c0;
   }
   r = ((x - 160) * 255) / 159;
   g = ((y - 180) * 255) / 59;
   b = 128;
   return (r << 16) | (g << 8) | b;
}

static void pattern_fill(void)
{
   unsigned x, y;
   for (y = 0; y < PATTERN_H; y++)
      for (x = 0; x < PATTERN_W; x++)
         pattern_fb[y * PATTERN_W + x] = pattern_pixel(x, y);
}

RETRO_API void retro_set_environment(retro_environment_t cb)
{
   bool no_game = true;
   pattern_environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) { pattern_video_cb = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) { pattern_audio_cb = cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { pattern_audio_batch_cb = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb) { pattern_input_poll_cb = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb) { pattern_input_state_cb = cb; }

RETRO_API void retro_init(void) { pattern_fill(); }
RETRO_API void retro_deinit(void) { }
RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }

RETRO_API void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "rslc pattern";
   info->library_version  = "1";
   info->valid_extensions = "";
   info->need_fullpath    = false;
   info->block_extract    = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 48000.0;
   info->geometry.base_width   = PATTERN_W;
   info->geometry.base_height  = PATTERN_H;
   info->geometry.max_width    = PATTERN_W;
   info->geometry.max_height   = PATTERN_H;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

RETRO_API void retro_reset(void) { }

RETRO_API void retro_run(void)
{
   if (pattern_input_poll_cb)
      pattern_input_poll_cb();
   pattern_video_cb(pattern_fb, PATTERN_W, PATTERN_H, PATTERN_W * sizeof(uint32_t));
}

RETRO_API bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   (void)game;
   return pattern_environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
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
RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void *data, size_t len) { (void)data; (void)len; return false; }
RETRO_API bool retro_unserialize(const void *data, size_t len) { (void)data; (void)len; return false; }
RETRO_API void retro_cheat_reset(void) { }
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}
RETRO_API void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
RETRO_API size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
