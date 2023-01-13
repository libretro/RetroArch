/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2011-2017 - Higor Euripedes
 *  Copyright (C) 2019-2021 - James Leaver
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_video.h>

#include <gfx/video_frame.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../dingux/dingux_utils.h"

#include "../../verbosity.h"
#include "../../gfx/drivers_font_renderer/bitmap.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#if defined(DINGUX_BETA)
#include "../../driver.h"
#endif

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define SDL_DINGUX_MENU_WIDTH  320
#define SDL_DINGUX_MENU_HEIGHT 240

#define SDL_DINGUX_NUM_FONT_GLYPHS 256

typedef struct sdl_dingux_video
{
   retro_time_t last_frame_time;
   retro_time_t ff_frame_time_min;
   SDL_Surface *screen;
   bitmapfont_lut_t *osd_font;
   unsigned frame_width;
   unsigned frame_height;
   unsigned frame_padding_x;
   unsigned frame_padding_y;
   enum dingux_ipu_filter_type filter_type;
#if defined(DINGUX_BETA)
   enum dingux_refresh_rate refresh_rate;
#endif
   uint32_t font_colour32;
   uint16_t font_colour16;
   uint16_t menu_texture[SDL_DINGUX_MENU_WIDTH * SDL_DINGUX_MENU_HEIGHT];
   bool rgb32;
   bool vsync;
   bool keep_aspect;
   bool integer_scaling;
   bool menu_active;
   bool was_in_menu;
   bool quitting;
   bool mode_valid;
} sdl_dingux_video_t;

static void sdl_dingux_init_font_color(sdl_dingux_video_t *vid)
{
   settings_t *settings = config_get_ptr();
   uint32_t red         = 0xFF;
   uint32_t green       = 0xFF;
   uint32_t blue        = 0xFF;

   if (settings)
   {
      red   = (uint32_t)((settings->floats.video_msg_color_r * 255.0f) + 0.5f) & 0xFF;
      green = (uint32_t)((settings->floats.video_msg_color_g * 255.0f) + 0.5f) & 0xFF;
      blue  = (uint32_t)((settings->floats.video_msg_color_b * 255.0f) + 0.5f) & 0xFF;
   }

   /* Convert to XRGB8888 */
   vid->font_colour32 = (red << 16) | (green << 8) | blue;

   /* Convert to RGB565 */
   red   = red   >> 3;
   green = green >> 3;
   blue  = blue  >> 3;

   vid->font_colour16 = (red << 11) | (green << 6) | blue;
}

static void sdl_dingux_blit_text16(
      sdl_dingux_video_t *vid,
      unsigned x, unsigned y,
      const char *str)
{
   /* Note: Cannot draw text in padding region
    * (padding region is never cleared, so
    * any text pixels would remain as garbage) */
   uint16_t *screen_buf         = (uint16_t*)vid->screen->pixels;
   bool **font_lut              = vid->osd_font->lut;
   /* 16 bit - divide pitch by 2 */
   uint16_t screen_stride       = (uint16_t)(vid->screen->pitch >> 1);
   uint16_t screen_width        = vid->screen->w;
   uint16_t screen_height       = vid->screen->h;
   unsigned x_pos               = x + vid->frame_padding_x;
   unsigned y_pos               = (y > (screen_height >> 1)) ?
         (y - vid->frame_padding_y) : (y + vid->frame_padding_y);
   uint16_t shadow_color_buf[2] = {0};
   uint16_t color_buf[2];

   color_buf[0] = vid->font_colour16;
   color_buf[1] = 0;

   /* Check for out of bounds y coordinates */
   if (y_pos + FONT_HEIGHT + 1 >=
         screen_height - vid->frame_padding_y)
      return;

   while (!string_is_empty(str))
   {
      /* Check for out of bounds x coordinates */
      if (x_pos + FONT_WIDTH_STRIDE + 1 >=
            screen_width - vid->frame_padding_x)
         return;

      /* Deal with spaces first, for efficiency */
      if (*str == ' ')
         str++;
      else
      {
         uint16_t i, j;
         bool *symbol_lut;
         uint32_t symbol = utf8_walk(&str);

         /* Stupid hack: 'oe' ligatures are not really
          * standard extended ASCII, so we have to waste
          * CPU cycles performing a conversion from the
          * unicode values... */
         if (symbol == 339) /* Latin small ligature oe */
            symbol = 156;
         if (symbol == 338) /* Latin capital ligature oe */
            symbol = 140;

         if (symbol >= SDL_DINGUX_NUM_FONT_GLYPHS)
            continue;

         symbol_lut = font_lut[symbol];

         for (j = 0; j < FONT_HEIGHT; j++)
         {
            uint32_t buff_offset = ((y_pos + j) * screen_stride) + x_pos;

            for (i = 0; i < FONT_WIDTH; i++)
            {
               if (*(symbol_lut + i + (j * FONT_WIDTH)))
               {
                  uint16_t *screen_buf_ptr = screen_buf + buff_offset + i;

                  /* Text pixel + right shadow */
                  memcpy(screen_buf_ptr, color_buf, sizeof(uint16_t));

                  /* Bottom shadow */
                  screen_buf_ptr += screen_stride;
                  memcpy(screen_buf_ptr, shadow_color_buf, sizeof(uint16_t));
               }
            }
         }
      }

      x_pos += FONT_WIDTH_STRIDE;
   }
}

static void sdl_dingux_blit_text32(
      sdl_dingux_video_t *vid,
      unsigned x, unsigned y,
      const char *str)
{
   /* Note: Cannot draw text in padding region
    * (padding region is never cleared, so
    * any text pixels would remain as garbage) */
   uint32_t *screen_buf         = (uint32_t*)vid->screen->pixels;
   bool **font_lut              = vid->osd_font->lut;
   /* 32 bit - divide pitch by 4 */
   uint32_t screen_stride       = (uint32_t)(vid->screen->pitch >> 2);
   uint32_t screen_width        = vid->screen->w;
   uint32_t screen_height       = vid->screen->h;
   unsigned x_pos               = x + vid->frame_padding_x;
   unsigned y_pos               = (y > (screen_height >> 1)) ?
         (y - vid->frame_padding_y) : (y + vid->frame_padding_y);
   uint32_t shadow_color_buf[2] = {0};
   uint32_t color_buf[2];

   color_buf[0] = vid->font_colour32;
   color_buf[1] = 0;

   /* Check for out of bounds y coordinates */
   if (y_pos + FONT_HEIGHT + 1 >=
         screen_height - vid->frame_padding_y)
      return;

   while (!string_is_empty(str))
   {
      /* Check for out of bounds x coordinates */
      if (x_pos + FONT_WIDTH_STRIDE + 1 >=
            screen_width - vid->frame_padding_x)
         return;

      /* Deal with spaces first, for efficiency */
      if (*str == ' ')
         str++;
      else
      {
         uint32_t i, j;
         bool *symbol_lut;
         uint32_t symbol = utf8_walk(&str);

         /* Stupid hack: 'oe' ligatures are not really
          * standard extended ASCII, so we have to waste
          * CPU cycles performing a conversion from the
          * unicode values... */
         if (symbol == 339) /* Latin small ligature oe */
            symbol = 156;
         if (symbol == 338) /* Latin capital ligature oe */
            symbol = 140;

         if (symbol >= SDL_DINGUX_NUM_FONT_GLYPHS)
            continue;

         symbol_lut = font_lut[symbol];

         for (j = 0; j < FONT_HEIGHT; j++)
         {
            uint32_t buff_offset = ((y_pos + j) * screen_stride) + x_pos;

            for (i = 0; i < FONT_WIDTH; i++)
            {
               if (*(symbol_lut + i + (j * FONT_WIDTH)))
               {
                  uint32_t *screen_buf_ptr = screen_buf + buff_offset + i;

                  /* Text pixel + right shadow */
                  memcpy(screen_buf_ptr, color_buf, sizeof(uint32_t));

                  /* Bottom shadow */
                  screen_buf_ptr += screen_stride;
                  memcpy(screen_buf_ptr, shadow_color_buf, sizeof(uint32_t));
               }
            }
         }
      }

      x_pos += FONT_WIDTH_STRIDE;
   }
}

static void sdl_dingux_blit_video_mode_error_msg(sdl_dingux_video_t *vid)
{
   const char *error_msg = msg_hash_to_str(MSG_UNSUPPORTED_VIDEO_MODE);
   char display_mode[64];

   display_mode[0] = '\0';

   /* Zero out pixel buffer */
   memset(vid->screen->pixels, 0,
         vid->screen->pitch * vid->screen->h);

   /* Generate display mode string */
   snprintf(display_mode, sizeof(display_mode), "> %ux%u, %s",
         vid->frame_width, vid->frame_height,
         vid->rgb32 ? "XRGB8888" : "RGB565");

   /* Print error message */
   if (vid->rgb32)
   {
      sdl_dingux_blit_text32(vid,
            FONT_WIDTH_STRIDE, FONT_WIDTH_STRIDE,
            error_msg);

      sdl_dingux_blit_text32(vid,
            FONT_WIDTH_STRIDE, FONT_WIDTH_STRIDE + FONT_HEIGHT_STRIDE,
            display_mode);
   }
   else
   {
      sdl_dingux_blit_text16(vid,
            FONT_WIDTH_STRIDE, FONT_WIDTH_STRIDE,
            error_msg);

      sdl_dingux_blit_text16(vid,
            FONT_WIDTH_STRIDE, FONT_WIDTH_STRIDE + FONT_HEIGHT_STRIDE,
            display_mode);
   }
}

static void sdl_dingux_gfx_free(void *data)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

   if (!vid)
      return;

   /* It is good manners to leave IPU scaling
    * parameters in the default state when
    * shutting down */
#if defined(DINGUX_BETA)
   dingux_ipu_reset();
#else
   if (!vid->keep_aspect || vid->integer_scaling)
      dingux_ipu_set_scaling_mode(true, false);

   if (vid->filter_type != DINGUX_IPU_FILTER_BICUBIC)
      dingux_ipu_set_filter_type(DINGUX_IPU_FILTER_BICUBIC);
#endif

   if (vid->osd_font)
      bitmapfont_free_lut(vid->osd_font);

   free(vid);
}

static void sdl_dingux_input_driver_init(
      const char *input_drv_name, const char *joypad_drv_name,
      input_driver_t **input, void **input_data)
{
   /* Sanity check */
   if (!input || !input_data)
      return;

   *input      = NULL;
   *input_data = NULL;

   /* If input driver name is empty, cannot
    * initialise anything... */
   if (string_is_empty(input_drv_name))
      return;

   if (string_is_equal(input_drv_name, "sdl_dingux"))
   {
      *input_data = input_driver_init_wrap(&input_sdl_dingux,
            joypad_drv_name);

      if (*input_data)
         *input = &input_sdl_dingux;

      return;
   }

#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   if (string_is_equal(input_drv_name, "sdl"))
   {
      *input_data = input_driver_init_wrap(&input_sdl,
            joypad_drv_name);

      if (*input_data)
         *input = &input_sdl;

      return;
   }
#endif

#if defined(HAVE_UDEV)
   if (string_is_equal(input_drv_name, "udev"))
   {
      *input_data = input_driver_init_wrap(&input_udev,
            joypad_drv_name);

      if (*input_data)
         *input = &input_udev;

      return;
   }
#endif

#if defined(__linux__)
   if (string_is_equal(input_drv_name, "linuxraw"))
   {
      *input_data = input_driver_init_wrap(&input_linuxraw,
            joypad_drv_name);

      if (*input_data)
         *input = &input_linuxraw;

      return;
   }
#endif
}

static void *sdl_dingux_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   sdl_dingux_video_t *vid                       = NULL;
   uint32_t sdl_subsystem_flags                  = SDL_WasInit(0);
   settings_t *settings                          = config_get_ptr();
   bool ipu_keep_aspect                          = settings->bools.video_dingux_ipu_keep_aspect;
   bool ipu_integer_scaling                      = settings->bools.video_scale_integer;
#if defined(DINGUX_BETA)
   enum dingux_refresh_rate current_refresh_rate = DINGUX_REFRESH_RATE_60HZ;
   enum dingux_refresh_rate target_refresh_rate  = (enum dingux_refresh_rate)
         settings->uints.video_dingux_refresh_rate;
   bool refresh_rate_valid                       = false;
   float hw_refresh_rate                         = 0.0f;
#endif
   enum dingux_ipu_filter_type ipu_filter_type   = (enum dingux_ipu_filter_type)
         settings->uints.video_dingux_ipu_filter_type;
   const char *input_drv_name                    = settings->arrays.input_driver;
   const char *joypad_drv_name                   = settings->arrays.input_joypad_driver;
   uint32_t surface_flags                        = (video->vsync) ?
         (SDL_HWSURFACE | SDL_TRIPLEBUF | SDL_FULLSCREEN) :
         (SDL_HWSURFACE | SDL_FULLSCREEN);

   /* Initialise graphics subsystem, if required */
   if (sdl_subsystem_flags == 0)
   {
      if (SDL_Init(SDL_INIT_VIDEO) < 0)
         return NULL;
   }
   else if ((sdl_subsystem_flags & SDL_INIT_VIDEO) == 0)
   {
      if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
         return NULL;
   }

   vid = (sdl_dingux_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

   dingux_ipu_set_downscaling_enable(true);
   dingux_ipu_set_scaling_mode(ipu_keep_aspect, ipu_integer_scaling);
   dingux_ipu_set_filter_type(ipu_filter_type);
#if defined(DINGUX_BETA)
   /* Get current refresh rate */
   refresh_rate_valid = dingux_get_video_refresh_rate(&current_refresh_rate);

   /* Check if refresh rate needs to be updated */
   if (!refresh_rate_valid ||
       (current_refresh_rate != target_refresh_rate))
      hw_refresh_rate = dingux_set_video_refresh_rate(target_refresh_rate);
   else
   {
      /* Correct refresh rate is already set,
       * just convert to float */
      switch (current_refresh_rate)
      {
         case DINGUX_REFRESH_RATE_50HZ:
            hw_refresh_rate = 50.0f;
            break;
         default:
            hw_refresh_rate = 60.0f;
            break;
      }
   }

   if (hw_refresh_rate == 0.0f)
   {
      RARCH_ERR("[SDL1]: Failed to set video refresh rate\n");
      goto error;
   }

   vid->refresh_rate = target_refresh_rate;
   switch (target_refresh_rate)
   {
      case DINGUX_REFRESH_RATE_50HZ:
         vid->ff_frame_time_min = 20000;
         break;
      default:
         vid->ff_frame_time_min = 16667;
         break;
   }

   driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &hw_refresh_rate);
#else
   vid->ff_frame_time_min = 16667;
#endif

   vid->screen = SDL_SetVideoMode(
         SDL_DINGUX_MENU_WIDTH, SDL_DINGUX_MENU_HEIGHT,
         video->rgb32 ? 32 : 16,
         surface_flags);

   if (!vid->screen)
   {
      RARCH_ERR("[SDL1]: Failed to init SDL surface: %s\n", SDL_GetError());
      goto error;
   }

   vid->frame_width     = SDL_DINGUX_MENU_WIDTH;
   vid->frame_height    = SDL_DINGUX_MENU_HEIGHT;
   vid->rgb32           = video->rgb32;
   vid->vsync           = video->vsync;
   vid->keep_aspect     = ipu_keep_aspect;
   vid->integer_scaling = ipu_integer_scaling;
   vid->filter_type     = ipu_filter_type;
   vid->menu_active     = false;
   vid->was_in_menu     = false;
   vid->quitting        = false;
   vid->mode_valid      = true;
   vid->last_frame_time = 0;

   SDL_ShowCursor(SDL_DISABLE);

   sdl_dingux_input_driver_init(input_drv_name,
         joypad_drv_name, input, input_data);

   /* Initialise OSD font */
   sdl_dingux_init_font_color(vid);

   vid->osd_font = bitmapfont_get_lut();

   if (!vid->osd_font ||
       vid->osd_font->glyph_max <
            (SDL_DINGUX_NUM_FONT_GLYPHS - 1))
   {
      RARCH_ERR("[SDL1]: Failed to init OSD font\n");
      goto error;
   }

   return vid;

error:
   sdl_dingux_gfx_free(vid);
   return NULL;
}

/* Certain display resolutions are forbidden on
 * OpenDingux, due to incompatibilities with the
 * hardware IPU scaler. Invalid widths will
 * generate a kernel segfault. Invalid heights
 * will cause image distortion, or are entirely
 * unsupported on OpenDingux Beta releases. This
 * function 'sanitises' the requested resolution.
 * Note that this requires some unavoidable
 * hard-coded blacklisting... */
static void sdl_dingux_sanitize_frame_dimensions(
      sdl_dingux_video_t* vid,
      unsigned width, unsigned height,
      unsigned *sanitized_width, unsigned *sanitized_height)
{
   /*** WIDTH ***/

   /* SDL surface width must be rounded up to
    * the nearest multiple of 16 */
   *sanitized_width = (width + 0xF) & ~0xF;

   /* Blacklist */

   /* Neo Geo @ 304x224 */
   if (!vid->integer_scaling && (width == 304) && (height == 224))
      *sanitized_width = 320;
#if defined(DINGUX_BETA)
   else if (vid->keep_aspect && !vid->integer_scaling)
   {
      /* Neo Geo Pocket (x2) @ 320x304 */
      if ((width == 320) && (height == 304))
         *sanitized_width = 336;
      /* GB/GBC/GG @ 160x144 */
      else if ((width == 160) && (height == 144))
         *sanitized_width = 176;
      /* GB/GBC/GG (x2) @ 320x288 */
      else if ((width == 320) && (height == 288))
         *sanitized_width = 336;
      /* GB/GBC/GG (x3) @ 480x432 */
      else if ((width == 480) && (height == 432))
         *sanitized_width = 496;
      /* SNES/Genesis @ 256x224 */
      else if ((width == 256) && (height == 224))
         *sanitized_width = 288;
      /* SNES/Genesis (x2) @ 512x448 */
      else if ((width == 512) && (height == 448))
         *sanitized_width = 560;
   }
#endif

   /*** HEIGHT ***/
   *sanitized_height = height;

   /* Blacklist */
#if defined(DINGUX_BETA)
   /* Neo Geo Pocket @ 160x152 */
   if ((width == 160) && (height == 152))
      *sanitized_height = 154;
   /* TIC-80 @ 240x136 */
   else if ((width == 240) && (height == 136))
      *sanitized_height = 144;
   else if (vid->keep_aspect && !vid->integer_scaling)
   {
      /* GBA @ 240x160 */
      if ((width == 240) && (height == 160))
         *sanitized_height = 162;
      /* GBA (x2) @ 480x320 */
      else if ((width == 480) && (height == 320))
         *sanitized_height = 324;
   }
#else
   /* Neo Geo Pocket @ 160x152 */
   if (!vid->integer_scaling && (width == 160) && (height == 152))
      *sanitized_height = 160;
#endif
}

static void sdl_dingux_set_output(
      sdl_dingux_video_t* vid,
      unsigned width, unsigned height, bool rgb32)
{
   unsigned sanitized_width;
   unsigned sanitized_height;
   uint32_t surface_flags = (vid->vsync) ?
         (SDL_HWSURFACE | SDL_TRIPLEBUF | SDL_FULLSCREEN) :
         (SDL_HWSURFACE | SDL_FULLSCREEN);

   /* Cache set parameters */
   vid->frame_width  = width;
   vid->frame_height = height;

   /* Reset frame padding */
   vid->frame_padding_x = 0;
   vid->frame_padding_y = 0;

   /* Ensure we request valid surface dimensions */
   sdl_dingux_sanitize_frame_dimensions(vid,
         width, height, &sanitized_width, &sanitized_height);

   /* Attempt to change video mode */
   vid->screen = SDL_SetVideoMode(
         sanitized_width, sanitized_height,
         rgb32 ? 32 : 16,
         surface_flags);

   /* Check whether selected display mode is valid */
   if (unlikely(!vid->screen))
   {
      RARCH_ERR("[SDL1]: Failed to init SDL surface: %s\n", SDL_GetError());

      /* We must have a valid SDL surface
       * > Use known good fallback display mode
       *   (i.e. menu resolution)
       * > Other than logging a message, we do not
       *   handle errors here, because this cannot
       *   fail - and if it did, there is nothing
       *   we can do about it anyway... */
      vid->screen = SDL_SetVideoMode(
            SDL_DINGUX_MENU_WIDTH, SDL_DINGUX_MENU_HEIGHT,
            rgb32 ? 32 : 16,
            surface_flags);

      if (unlikely(!vid->screen))
         RARCH_ERR("[SDL1]: Critical - Failed to init fallback SDL surface: %s\n", SDL_GetError());

      vid->mode_valid = false;
   }
   else
   {
      /* Determine whether frame padding is required */
      if ((sanitized_width  != width) ||
          (sanitized_height != height))
      {
         vid->frame_padding_x = (sanitized_width  - width)  >> 1;
         vid->frame_padding_y = (sanitized_height - height) >> 1;

         /* To prevent garbage pixels in the padding
          * region, must zero out pixel buffer */
         if (SDL_MUSTLOCK(vid->screen))
            SDL_LockSurface(vid->screen);

         memset(vid->screen->pixels, 0,
               vid->screen->pitch * vid->screen->h);

         if (SDL_MUSTLOCK(vid->screen))
            SDL_UnlockSurface(vid->screen);
      }

      vid->mode_valid = true;
   }
}

static void sdl_dingux_blit_frame16(sdl_dingux_video_t *vid,
      uint16_t* src, unsigned width, unsigned height,
      unsigned src_pitch)
{
   unsigned dst_pitch = vid->screen->pitch;
   uint16_t *in_ptr   = src;
   uint16_t *out_ptr  = (uint16_t*)(vid->screen->pixels +
         (vid->frame_padding_y * dst_pitch));

   /* If source and destination buffers have the
    * same pitch, perform fast copy of raw pixel data */
   if (src_pitch == dst_pitch)
      memcpy(out_ptr, in_ptr, src_pitch * height);
   else
   {
      /* Otherwise copy pixel data line-by-line */

      /* 16 bit - divide pitch by 2 */
      uint16_t in_stride  = (uint16_t)(src_pitch >> 1);
      uint16_t out_stride = (uint16_t)(dst_pitch >> 1);
      size_t y;

      /* If SDL surface has horizontal padding,
       * shift output image to the right */
      out_ptr += vid->frame_padding_x;

      for (y = 0; y < height; y++)
      {
         memcpy(out_ptr, in_ptr, width * sizeof(uint16_t));
         in_ptr  += in_stride;
         out_ptr += out_stride;
      }
   }
}

static void sdl_dingux_blit_frame32(sdl_dingux_video_t *vid,
      uint32_t* src, unsigned width, unsigned height,
      unsigned src_pitch)
{
   unsigned dst_pitch = vid->screen->pitch;
   uint32_t *in_ptr   = src;
   uint32_t *out_ptr  = (uint32_t*)(vid->screen->pixels +
         (vid->frame_padding_y * dst_pitch));

   /* If source and destination buffers have the
    * same pitch, perform fast copy of raw pixel data */
   if (src_pitch == dst_pitch)
      memcpy(out_ptr, in_ptr, src_pitch * height);
   else
   {
      /* Otherwise copy pixel data line-by-line */

      /* 32 bit - divide pitch by 4 */
      uint32_t in_stride  = (uint32_t)(src_pitch >> 2);
      uint32_t out_stride = (uint32_t)(dst_pitch >> 2);
      size_t y;

      /* If SDL surface has horizontal padding,
       * shift output image to the right */
      out_ptr += vid->frame_padding_x;

      for (y = 0; y < height; y++)
      {
         memcpy(out_ptr, in_ptr, width * sizeof(uint32_t));
         in_ptr  += in_stride;
         out_ptr += out_stride;
      }
   }
}

static bool sdl_dingux_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   sdl_dingux_video_t* vid = (sdl_dingux_video_t*)data;

   /* Return early if:
    * - Input sdl_dingux_video_t struct is NULL
    *   (cannot realistically happen)
    * - Menu is inactive and input 'content' frame
    *   data is NULL (may happen when e.g. a running
    *   core skips a frame) */
   if (unlikely(!vid || (!frame && !vid->menu_active)))
      return true;

   /* If fast forward is currently active, we may
    * push frames at an 'unlimited' rate. Since the
    * display has a fixed refresh rate of 60 Hz (or
    * potentially 50 Hz on OpenDingux Beta), this
    * represents wasted effort. We therefore drop any
    * 'excess' frames in this case.
    * (Note that we *only* do this when fast forwarding.
    * Attempting this trick while running content normally
    * will cause bad frame pacing) */
   if (unlikely(video_info->input_driver_nonblock_state))
   {
      retro_time_t current_time = cpu_features_get_time_usec();

      if ((current_time - vid->last_frame_time) <
            vid->ff_frame_time_min)
         return true;

      vid->last_frame_time = current_time;
   }

#ifdef HAVE_MENU
   menu_driver_frame(video_info->menu_is_alive, video_info);
#endif

   if (likely(!vid->menu_active))
   {
      /* Update video mode if we were in the menu on
       * the previous frame, or width/height have changed */
      if (unlikely(
            vid->was_in_menu ||
            (vid->frame_width  != width) ||
            (vid->frame_height != height)))
         sdl_dingux_set_output(vid, width, height, vid->rgb32);

      /* Must always lock SDL surface before
       * manipulating raw pixel buffer */
      if (SDL_MUSTLOCK(vid->screen))
         SDL_LockSurface(vid->screen);

      if (likely(vid->mode_valid))
      {
         /* Blit frame to SDL surface */
         if (vid->rgb32)
            sdl_dingux_blit_frame32(vid, (uint32_t*)frame,
                  width, height, pitch);
         else
            sdl_dingux_blit_frame16(vid, (uint16_t*)frame,
                  width, height, pitch);
      }
      /* If current display mode is invalid,
       * just display an error message */
      else
         sdl_dingux_blit_video_mode_error_msg(vid);

      vid->was_in_menu = false;
   }
   else
   {
      /* If this is the first frame that the menu
       * is active, update video mode */
      if (!vid->was_in_menu)
      {
         sdl_dingux_set_output(vid,
               SDL_DINGUX_MENU_WIDTH, SDL_DINGUX_MENU_HEIGHT, false);

         vid->was_in_menu = true;
      }

      if (SDL_MUSTLOCK(vid->screen))
         SDL_LockSurface(vid->screen);

      /* Blit menu texture to SDL surface */
      sdl_dingux_blit_frame16(vid, vid->menu_texture,
            SDL_DINGUX_MENU_WIDTH, SDL_DINGUX_MENU_HEIGHT,
            SDL_DINGUX_MENU_WIDTH * sizeof(uint16_t));
   }

   /* Print OSD text, if required */
   if (msg)
   {
      /* If menu is active, colour depth is overridden
       * to 16 bit */
      if (vid->rgb32 && !vid->menu_active)
         sdl_dingux_blit_text32(vid, FONT_WIDTH_STRIDE,
               vid->screen->h - (FONT_HEIGHT + FONT_WIDTH_STRIDE), msg);
      else
         sdl_dingux_blit_text16(vid, FONT_WIDTH_STRIDE,
               vid->screen->h - (FONT_HEIGHT + FONT_WIDTH_STRIDE), msg);
   }

   /* Pixel manipulation complete - unlock
    * SDL surface */
   if (SDL_MUSTLOCK(vid->screen))
      SDL_UnlockSurface(vid->screen);

   SDL_Flip(vid->screen);

   return true;
}

static void sdl_dingux_set_texture_enable(void *data, bool state, bool full_screen)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

   if (unlikely(!vid))
      return;

   vid->menu_active = state;
}

static void sdl_dingux_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

   if (unlikely(
         !vid ||
         rgb32 ||
         (width > SDL_DINGUX_MENU_WIDTH) ||
         (height > SDL_DINGUX_MENU_HEIGHT)))
      return;

   memcpy(vid->menu_texture, frame, width * height * sizeof(uint16_t));
}

static void sdl_dingux_gfx_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;
   bool vsync              = !toggle;

   if (unlikely(!vid))
      return;

   /* Check whether vsync status has changed */
   if (vid->vsync != vsync)
   {
      unsigned current_width  = vid->frame_width;
      unsigned current_height = vid->frame_height;
      vid->vsync              = vsync;

      /* Update video mode */

      /* Note that a tedious workaround is required...
       * - Calling SDL_SetVideoMode() with the currently
       *   set width, height and pixel format can randomly
       *   become a noop even if the surface flags change.
       * - Since all we are doing here is changing the VSYNC
       *   parameter (which just modifies surface flags), this
       *   means the VSYNC toggle may not be registered...
       * - This is a huge problem when enabling fast forward,
       *   because VSYNC ON effectively limits maximum frame
       *   rate - if we push frames too rapidly, the OS chokes
       *   and the display freezes.
       * We have to ensure that the VSYNC state change is
       * applied in all cases. We can only do this by forcing
       * a 'real' video mode update, which means adjusting the
       * video resolution. We therefore end up calling
       * sdl_dingux_set_output() *twice*, setting the dimensions
       * to an arbitrary value before restoring the actual
       * desired width/height */
      sdl_dingux_set_output(vid,
            current_width,
            (current_height > 4) ? (current_height - 2) : 16,
            vid->rgb32);

      sdl_dingux_set_output(vid,
            current_width, current_height, vid->rgb32);
   }
}

static void sdl_dingux_gfx_check_window(sdl_dingux_video_t *vid)
{
   SDL_Event event;

   SDL_PumpEvents();
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUITMASK))
   {
      if (event.type != SDL_QUIT)
         continue;

      vid->quitting = true;
      break;
   }
}

static bool sdl_dingux_gfx_alive(void *data)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

   if (unlikely(!vid))
      return false;

   sdl_dingux_gfx_check_window(vid);
   return !vid->quitting;
}

static bool sdl_dingux_gfx_focus(void *data)
{
   return true;
}

static bool sdl_dingux_gfx_suppress_screensaver(void *data, bool enable)
{
   return false;
}

static bool sdl_dingux_gfx_has_windowed(void *data)
{
   return false;
}

static void sdl_dingux_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

   if (unlikely(!vid))
      return;

   vp->x      = 0;
   vp->y      = 0;
   vp->width  = vp->full_width  = vid->frame_width;
   vp->height = vp->full_height = vid->frame_height;
}

static float sdl_dingux_get_refresh_rate(void *data)
{
#if defined(DINGUX_BETA)
   sdl_dingux_video_t *vid = (sdl_dingux_video_t*)data;

   if (!vid)
      return 0.0f;

   switch (vid->refresh_rate)
   {
      case DINGUX_REFRESH_RATE_50HZ:
         return 50.0f;
      default:
         break;
   }
#endif

   return 60.0f;
}

static void sdl_dingux_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   sdl_dingux_video_t *vid                     = (sdl_dingux_video_t*)data;
   settings_t *settings                        = config_get_ptr();
   enum dingux_ipu_filter_type ipu_filter_type = (settings) ?
         (enum dingux_ipu_filter_type)settings->uints.video_dingux_ipu_filter_type :
         DINGUX_IPU_FILTER_BICUBIC;

   if (!vid || !settings)
      return;

   /* Update IPU filter setting, if required */
   if (vid->filter_type != ipu_filter_type)
   {
      dingux_ipu_set_filter_type(ipu_filter_type);
      vid->filter_type = ipu_filter_type;
   }
}

static void sdl_dingux_apply_state_changes(void *data)
{
   sdl_dingux_video_t *vid  = (sdl_dingux_video_t*)data;
   settings_t *settings     = config_get_ptr();
   bool ipu_keep_aspect     = (settings) ? settings->bools.video_dingux_ipu_keep_aspect : true;
   bool ipu_integer_scaling = (settings) ? settings->bools.video_scale_integer : false;

   if (!vid || !settings)
      return;

   /* Update IPU scaling mode, if required */
   if ((vid->keep_aspect != ipu_keep_aspect) ||
       (vid->integer_scaling != ipu_integer_scaling))
   {
      unsigned current_width  = vid->frame_width;
      unsigned current_height = vid->frame_height;
      unsigned screen_width   = vid->screen->w;
      unsigned screen_height  = vid->screen->h;
      unsigned sanitized_width;
      unsigned sanitized_height;

      dingux_ipu_set_scaling_mode(ipu_keep_aspect, ipu_integer_scaling);
      vid->keep_aspect     = ipu_keep_aspect;
      vid->integer_scaling = ipu_integer_scaling;

      /* Scaling mode can affect supported display
       * resolutions. In such cases, update the video
       * display mode */
      sdl_dingux_sanitize_frame_dimensions(vid,
            current_width, current_height,
            &sanitized_width, &sanitized_height);

      if ((screen_width  != sanitized_width) ||
          (screen_height != sanitized_height))
         sdl_dingux_set_output(vid,
               current_width, current_height, vid->rgb32);
   }
}

static uint32_t sdl_dingux_get_flags(void *data)
{
   return 0;
}

static const video_poke_interface_t sdl_dingux_poke_interface = {
   sdl_dingux_get_flags,
   NULL,
   NULL,
   NULL,
   sdl_dingux_get_refresh_rate,
   sdl_dingux_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL,
   sdl_dingux_apply_state_changes,
   sdl_dingux_set_texture_frame,
   sdl_dingux_set_texture_enable,
   NULL,
   NULL, /* sdl_show_mouse */
   NULL, /* sdl_grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void sdl_dingux_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   *iface = &sdl_dingux_poke_interface;
}

static bool sdl_dingux_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   return false;
}

video_driver_t video_sdl_dingux = {
   sdl_dingux_gfx_init,
   sdl_dingux_gfx_frame,
   sdl_dingux_gfx_set_nonblock_state,
   sdl_dingux_gfx_alive,
   sdl_dingux_gfx_focus,
   sdl_dingux_gfx_suppress_screensaver,
   sdl_dingux_gfx_has_windowed,
   sdl_dingux_gfx_set_shader,
   sdl_dingux_gfx_free,
   "sdl_dingux",
   NULL,
   NULL, /* set_rotation */
   sdl_dingux_gfx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   sdl_dingux_get_poke_interface
};
