/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2011-2017 - Higor Euripedes
 *  Copyright (C) 2019-2021 - James Leaver
 *  Copyright (C)      2021 - John Parton
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

#include <retro_assert.h>
#include <gfx/video_frame.h>
#include <retro_assert.h>
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

#if defined(MIYOO)
#define SDL_RS90_WIDTH  320
#define SDL_RS90_HEIGHT 240
#else
#define SDL_RS90_WIDTH  240
#define SDL_RS90_HEIGHT 160
#endif

#define SDL_RS90_NUM_FONT_GLYPHS 256

#if defined(MIYOO)
#define SDL_RS90_SURFACE_FLAGS_VSYNC_ON  (SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN)
#else
#define SDL_RS90_SURFACE_FLAGS_VSYNC_ON  (SDL_HWSURFACE | SDL_TRIPLEBUF | SDL_FULLSCREEN)
#endif
#define SDL_RS90_SURFACE_FLAGS_VSYNC_OFF (SDL_HWSURFACE | SDL_FULLSCREEN)

typedef struct sdl_rs90_video sdl_rs90_video_t;
struct sdl_rs90_video
{
   SDL_Surface *screen;
   void (*scale_frame16)(sdl_rs90_video_t *vid,
         uint16_t *src, unsigned width, unsigned height,
         unsigned src_pitch);
   void (*scale_frame32)(sdl_rs90_video_t *vid,
         uint32_t *src, unsigned width, unsigned height,
         unsigned src_pitch);
   /* Scaling/padding/cropping parameters */
   unsigned content_width;
   unsigned content_height;
   unsigned frame_width;
   unsigned frame_height;
   unsigned frame_padding_x;
   unsigned frame_padding_y;
   unsigned frame_crop_x;
   unsigned frame_crop_y;
   bool rgb32;
   bool menu_active;
   bool was_in_menu;
   bool mode_valid;
   retro_time_t last_frame_time;
   retro_time_t ff_frame_time_min;
   enum dingux_rs90_softfilter_type softfilter_type;
#if defined(DINGUX_BETA)
   enum dingux_refresh_rate refresh_rate;
#endif
   bool vsync;
   bool keep_aspect;
   bool scale_integer;
   bool quitting;
   bitmapfont_lut_t *osd_font;
   uint32_t font_colour32;
   uint16_t font_colour16;
   uint16_t menu_texture[SDL_RS90_WIDTH * SDL_RS90_HEIGHT];
};

/* Image interpolation START */

static void sdl_rs90_scale_frame16_integer(sdl_rs90_video_t *vid,
      uint16_t *src, unsigned width, unsigned height,
      unsigned src_pitch)
{
   /* 16 bit - divide pitch by 2 */
   size_t in_stride  = (size_t)(src_pitch >> 1);
   size_t out_stride = (size_t)(vid->screen->pitch >> 1);

   /* Manipulate offsets so that padding/crop
    * are applied correctly */
   uint16_t *in_ptr  = src + vid->frame_crop_x + vid->frame_crop_y * in_stride;
   uint16_t *out_ptr = (uint16_t*)(vid->screen->pixels) + vid->frame_padding_x +
         out_stride * vid->frame_padding_y;

   size_t y          = vid->frame_height;

   /* TODO/FIXME: Optimize this loop */
   do
   {
      memcpy(out_ptr, in_ptr, vid->frame_width * sizeof(uint16_t));
      in_ptr  += in_stride;
      out_ptr += out_stride;
   }
   while (--y);
}

static void sdl_rs90_scale_frame32_integer(sdl_rs90_video_t *vid,
      uint32_t *src, unsigned width, unsigned height,
      unsigned src_pitch)
{
   /* 32 bit - divide pitch by 4 */
   size_t in_stride  = (size_t)(src_pitch >> 2);
   size_t out_stride = (size_t)(vid->screen->pitch >> 2);

   /* Manipulate offsets so that padding/crop
    * are applied correctly */
   uint32_t *in_ptr  = src + vid->frame_crop_x + vid->frame_crop_y * in_stride;
   uint32_t *out_ptr = (uint32_t*)(vid->screen->pixels) + vid->frame_padding_x +
         out_stride * vid->frame_padding_y;

   size_t y          = vid->frame_height;

   /* TODO/FIXME: Optimize this loop */
   do
   {
      memcpy(out_ptr, in_ptr, vid->frame_width * sizeof(uint32_t));
      in_ptr  += in_stride;
      out_ptr += out_stride;
   }
   while (--y);
}

/* Approximate nearest-neighbour scaling using
 * bitshifts and integer math */
static void sdl_rs90_scale_frame16_point(sdl_rs90_video_t *vid,
      uint16_t *src, unsigned width, unsigned height,
      unsigned src_pitch)
{
   uint32_t x_step      = (((uint32_t)(width)  << 16) + 1) / vid->frame_width;
   uint32_t y_step      = (((uint32_t)(height) << 16) + 1) / vid->frame_height;

   /* 16 bit - divide pitch by 2 */
   size_t in_stride     = (size_t)(src_pitch >> 1);
   size_t out_stride    = (size_t)(vid->screen->pitch >> 1);

   /* Apply x/y padding offset */
   uint16_t *top_corner = (uint16_t*)(vid->screen->pixels) + vid->frame_padding_x +
         out_stride * vid->frame_padding_y;

   /* Temporary pointers */
   uint16_t *in_ptr     = NULL;
   uint16_t *out_ptr    = NULL;

   uint32_t y           = 0;
   size_t row;

   /* TODO/FIXME: Optimize these loops further.
    * Consider saving these computations in an array
    * and indexing over them.
    * Would likely be slower due to cache (non-)locality,
    * but it's worth a shot.
    * Tons of -> operations. */
   for (row = 0; row < vid->frame_height; row++)
   {
      size_t col = vid->frame_width;
      uint32_t x = 0;

      out_ptr    = top_corner + out_stride * row;
      in_ptr     = src + (y >> 16) * in_stride;

      do
      {
        *(out_ptr++) = in_ptr[x >> 16];
        x           += x_step;
      }
      while (--col);

      y += y_step;
   }
}

static void sdl_rs90_scale_frame32_point(sdl_rs90_video_t *vid,
      uint32_t *src, unsigned width, unsigned height,
      unsigned src_pitch)
{
   uint32_t x_step      = (((uint32_t)(width)  << 16) + 1) / vid->frame_width;
   uint32_t y_step      = (((uint32_t)(height) << 16) + 1) / vid->frame_height;

   /* 32 bit - divide pitch by 4 */
   size_t in_stride     = (size_t)(src_pitch >> 2);
   size_t out_stride    = (size_t)(vid->screen->pitch >> 2);

   /* Apply x/y padding offset */
   uint32_t *top_corner = (uint32_t*)(vid->screen->pixels) + vid->frame_padding_x +
         out_stride * vid->frame_padding_y;

   /* Temporary pointers */
   uint32_t *in_ptr     = NULL;
   uint32_t *out_ptr    = NULL;

   uint32_t y           = 0;
   size_t row;

   /* TODO/FIXME: Optimize these loops further.
    * Consider saving these computations in an array
    * and indexing over them.
    * Would likely be slower due to cache (non-)locality,
    * but it's worth a shot.
    * Tons of -> operations. */
   for (row = 0; row < vid->frame_height; row++)
   {
      size_t col = vid->frame_width;
      uint32_t x = 0;

      out_ptr    = top_corner + out_stride * row;
      in_ptr     = src + (y >> 16) * in_stride;

      do
      {
        *(out_ptr++) = in_ptr[x >> 16];
        x           += x_step;
      }
      while (--col);

      y += y_step;
   }
}

/* Produces a 50:50 mix of pixels a and b
 * > c.f. "Mixing Packed RGB Pixels Efficiently"
 *        http://blargg.8bitalley.com/info/rgb_mixing.html */
#define SDL_RS90_PIXEL_AVERAGE_16(a, b) (((a) + (b) + (((a) ^ (b)) & 0x821))   >> 1)
#define SDL_RS90_PIXEL_AVERAGE_32(a, b) (((a) + (b) + (((a) ^ (b)) & 0x10101)) >> 1)

/* Scales a single horizontal line using approximate
 * linear scaling
 * > c.f. "Image Scaling with Bresenham"
 *        https://www.drdobbs.com/image-scaling-with-bresenham/184405045 */
static void sdl_rs90_scale_line_bresenham16(uint16_t *target, uint16_t *src,
      unsigned src_width, unsigned target_width,
      unsigned int_part, unsigned fract_part, unsigned midpoint)
{
   unsigned num_pixels = target_width;
   unsigned E = 0; /* TODO/FIXME: Determine better variable name - 'error'? */

   /* If source and target have the same width,
    * can perform a fast copy of raw pixel data */
   if (src_width == target_width)
   {
      memcpy(target, src, target_width * sizeof(uint16_t));
      return;
   }
   else if (target_width > src_width)
      num_pixels--;

   do
   {
      *(target++) = (E >= midpoint) ?
            SDL_RS90_PIXEL_AVERAGE_16(*src, *(src + 1)) : *src;

      src += int_part;
      E   += fract_part;

      if (E >= target_width)
      {
         E -= target_width;
         src++;
      }
   }
   while (--num_pixels);

   if (target_width > src_width)
      *target = *src;
}

static void sdl_rs90_scale_frame16_bresenham_horz(sdl_rs90_video_t *vid,
      uint16_t *src, unsigned width, unsigned height,
      unsigned src_pitch)
{
   /* 16 bit - divide pitch by 2 */
   size_t in_stride        = (size_t)(src_pitch >> 1);
   size_t out_stride       = (size_t)(vid->screen->pitch >> 1);

   uint16_t *prev_src      = NULL;

   /* Account for x/y padding */
   uint16_t *target        = (uint16_t*)(vid->screen->pixels) + vid->frame_padding_x +
         (out_stride * vid->frame_padding_y);
   unsigned target_width   = vid->frame_width;
   unsigned target_height  = vid->frame_height;

   unsigned num_lines      = target_height;
   unsigned int_part       = (height / target_height) * in_stride;
   unsigned fract_part     = height % target_height;
   unsigned E              = 0; /* TODO/FIXME: Determine better variable name - 'error'? */

   unsigned col_int_part   = width / target_width;
   unsigned col_fract_part = width % target_width;
   /* Enhance midpoint selection as described in
    * https://www.compuphase.com/graphic/scale1errata.htm */
   int col_midpoint        = (target_width > width) ?
         ((int)target_width - 3 * ((int)target_width - (int)width)) >> 1 :
               (int)(target_width << 1) - (int)width;
   /* Clamp lower bound to (target_width / 2) */
   if (col_midpoint < (int)(target_width >> 1))
      col_midpoint = (int)(target_width >> 1);

   while (num_lines--)
   {
      /* If line is supposed to be identical to
       * previous line, just copy it */
      if (src == prev_src)
         memcpy(target, target - out_stride, target_width * sizeof(uint16_t));
      else
      {
         sdl_rs90_scale_line_bresenham16(target, src, width, target_width,
               col_int_part, col_fract_part, (unsigned)col_midpoint);
         prev_src = src;
      }

      target += out_stride;
      src    += int_part;
      E      += fract_part;

      if (E >= target_height)
      {
         E   -= target_height;
         src += in_stride;
      }
   }
}

static void sdl_rs90_scale_line_bresenham32(uint32_t *target, uint32_t *src,
      unsigned src_width, unsigned target_width,
      unsigned int_part, unsigned fract_part, unsigned midpoint)
{
   unsigned num_pixels = target_width;
   unsigned E = 0; /* TODO/FIXME: Determine better variable name - 'error'? */

   /* If source and target have the same width,
    * can perform a fast copy of raw pixel data */
   if (src_width == target_width)
   {
      memcpy(target, src, target_width * sizeof(uint32_t));
      return;
   }
   else if (target_width > src_width)
      num_pixels--;

   do
   {
      *(target++) = (E >= midpoint) ?
            SDL_RS90_PIXEL_AVERAGE_32(*src, *(src + 1)) : *src;

      src += int_part;
      E   += fract_part;

      if (E >= target_width)
      {
         E -= target_width;
         src++;
      }
   }
   while (--num_pixels);

   if (target_width > src_width)
      *target = *src;
}

static void sdl_rs90_scale_frame32_bresenham_horz(sdl_rs90_video_t *vid,
      uint32_t *src, unsigned width, unsigned height,
      unsigned src_pitch)
{
   /* 32 bit - divide pitch by 4 */
   size_t in_stride        = (size_t)(src_pitch >> 2);
   size_t out_stride       = (size_t)(vid->screen->pitch >> 2);

   uint32_t *prev_src      = NULL;

   /* Account for x/y padding */
   uint32_t *target        = (uint32_t*)(vid->screen->pixels) + vid->frame_padding_x +
         (out_stride * vid->frame_padding_y);
   unsigned target_width   = vid->frame_width;
   unsigned target_height  = vid->frame_height;

   unsigned num_lines      = target_height;
   unsigned int_part       = (height / target_height) * in_stride;
   unsigned fract_part     = height % target_height;
   unsigned E              = 0; /* TODO/FIXME: Determine better variable name - 'error'? */

   unsigned col_int_part   = width / target_width;
   unsigned col_fract_part = width % target_width;
   /* Enhance midpoint selection as described in
    * https://www.compuphase.com/graphic/scale1errata.htm */
   int col_midpoint        = (target_width > width) ?
         ((int)target_width - 3 * ((int)target_width - (int)width)) >> 1 :
               (int)(target_width << 1) - (int)width;
   /* Clamp lower bound to (target_width / 2) */
   if (col_midpoint < (int)(target_width >> 1))
      col_midpoint = (int)(target_width >> 1);

   while (num_lines--)
   {
      /* If line is supposed to be identical to
       * previous line, just copy it */
      if (src == prev_src)
         memcpy(target, target - out_stride, target_width * sizeof(uint32_t));
      else
      {
         sdl_rs90_scale_line_bresenham32(target, src, width, target_width,
               col_int_part, col_fract_part, (unsigned)col_midpoint);
         prev_src = src;
      }

      target += out_stride;
      src    += int_part;
      E      += fract_part;

      if (E >= target_height)
      {
         E   -= target_height;
         src += in_stride;
      }
   }
}

static void sdl_rs90_set_scale_frame_functions(sdl_rs90_video_t *vid)
{
   /* Set integer scaling by default */
   vid->scale_frame16 = sdl_rs90_scale_frame16_integer;
   vid->scale_frame32 = sdl_rs90_scale_frame32_integer;

   if (!vid->scale_integer)
   {
      switch (vid->softfilter_type)
      {
         case DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ:
            vid->scale_frame16 = sdl_rs90_scale_frame16_bresenham_horz;
            vid->scale_frame32 = sdl_rs90_scale_frame32_bresenham_horz;
            break;
         case DINGUX_RS90_SOFTFILTER_POINT:
         default:
            vid->scale_frame16 = sdl_rs90_scale_frame16_point;
            vid->scale_frame32 = sdl_rs90_scale_frame32_point;
            break;
      }
   }
}

/* Image interpolation END */

static void sdl_rs90_init_font_color(sdl_rs90_video_t *vid)
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

static void sdl_rs90_blit_text16(
      sdl_rs90_video_t *vid,
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

         if (symbol >= SDL_RS90_NUM_FONT_GLYPHS)
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

static void sdl_rs90_blit_text32(
      sdl_rs90_video_t *vid,
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

         if (symbol >= SDL_RS90_NUM_FONT_GLYPHS)
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

static void sdl_rs90_blit_video_mode_error_msg(sdl_rs90_video_t *vid)
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
      sdl_rs90_blit_text32(vid,
            FONT_WIDTH_STRIDE, FONT_WIDTH_STRIDE,
            error_msg);

      sdl_rs90_blit_text32(vid,
            FONT_WIDTH_STRIDE, FONT_WIDTH_STRIDE + FONT_HEIGHT_STRIDE,
            display_mode);
   }
   else
   {
      sdl_rs90_blit_text16(vid,
            FONT_WIDTH_STRIDE, FONT_WIDTH_STRIDE,
            error_msg);

      sdl_rs90_blit_text16(vid,
            FONT_WIDTH_STRIDE, FONT_WIDTH_STRIDE + FONT_HEIGHT_STRIDE,
            display_mode);
   }
}

static void sdl_rs90_gfx_free(void *data)
{
   sdl_rs90_video_t *vid = (sdl_rs90_video_t*)data;

   if (!vid)
      return;

   if (vid->osd_font)
      bitmapfont_free_lut(vid->osd_font);

   free(vid);
}

static void sdl_rs90_input_driver_init(
      const char *input_driver_name, const char *joypad_driver_name,
      input_driver_t **input, void **input_data)
{
   /* Sanity check */
   if (!input || !input_data)
      return;

   *input      = NULL;
   *input_data = NULL;

   /* If input driver name is empty, cannot
    * initialise anything... */
   if (string_is_empty(input_driver_name))
      return;

   if (string_is_equal(input_driver_name, "sdl_dingux"))
   {
      *input_data = input_driver_init_wrap(&input_sdl_dingux,
            joypad_driver_name);

      if (*input_data)
         *input = &input_sdl_dingux;

      return;
   }

#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   if (string_is_equal(input_driver_name, "sdl"))
   {
      *input_data = input_driver_init_wrap(&input_sdl,
            joypad_driver_name);

      if (*input_data)
         *input = &input_sdl;

      return;
   }
#endif

#if defined(HAVE_UDEV)
   if (string_is_equal(input_driver_name, "udev"))
   {
      *input_data = input_driver_init_wrap(&input_udev,
            joypad_driver_name);

      if (*input_data)
         *input = &input_udev;

      return;
   }
#endif

#if defined(__linux__)
   if (string_is_equal(input_driver_name, "linuxraw"))
   {
      *input_data = input_driver_init_wrap(&input_linuxraw,
            joypad_driver_name);

      if (*input_data)
         *input = &input_linuxraw;

      return;
   }
#endif
}

static void *sdl_rs90_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   sdl_rs90_video_t *vid                         = NULL;
   uint32_t sdl_subsystem_flags                  = SDL_WasInit(0);
   settings_t *settings                          = config_get_ptr();
#if defined(DINGUX_BETA)
   enum dingux_refresh_rate current_refresh_rate = DINGUX_REFRESH_RATE_60HZ;
   enum dingux_refresh_rate target_refresh_rate  = (enum dingux_refresh_rate)
         settings->uints.video_dingux_refresh_rate;
   bool refresh_rate_valid                       = false;
   float hw_refresh_rate                         = 0.0f;
#endif
   const char *input_driver_name                 = settings->arrays.input_driver;
   const char *joypad_driver_name                = settings->arrays.input_joypad_driver;
   uint32_t surface_flags                        = (video->vsync) ?
         SDL_RS90_SURFACE_FLAGS_VSYNC_ON :
         SDL_RS90_SURFACE_FLAGS_VSYNC_OFF;

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

   vid = (sdl_rs90_video_t*)calloc(1, sizeof(*vid));
   if (!vid)
      return NULL;

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
         SDL_RS90_WIDTH, SDL_RS90_HEIGHT,
         video->rgb32 ? 32 : 16,
         surface_flags);

   if (!vid->screen)
   {
      RARCH_ERR("[SDL1]: Failed to init SDL surface: %s\n", SDL_GetError());
      goto error;
   }

   vid->content_width   = SDL_RS90_WIDTH;
   vid->content_height  = SDL_RS90_HEIGHT;
   vid->frame_width     = SDL_RS90_WIDTH;
   vid->frame_height    = SDL_RS90_HEIGHT;
   vid->rgb32           = video->rgb32;
   vid->vsync           = video->vsync;
   vid->keep_aspect     = settings->bools.video_dingux_ipu_keep_aspect;
   vid->scale_integer   = settings->bools.video_scale_integer;
   vid->softfilter_type = (enum dingux_rs90_softfilter_type)
         settings->uints.video_dingux_rs90_softfilter_type;
   vid->menu_active     = false;
   vid->was_in_menu     = false;
   vid->quitting        = false;
   vid->mode_valid      = true;
   vid->last_frame_time = 0;

   SDL_ShowCursor(SDL_DISABLE);

   sdl_rs90_input_driver_init(input_driver_name,
         joypad_driver_name, input, input_data);

   /* Initialise OSD font */
   sdl_rs90_init_font_color(vid);

   vid->osd_font = bitmapfont_get_lut();

   if (!vid->osd_font ||
       vid->osd_font->glyph_max <
            (SDL_RS90_NUM_FONT_GLYPHS - 1))
   {
      RARCH_ERR("[SDL1]: Failed to init OSD font\n");
      goto error;
   }

   /* Assign frame scaling function pointers */
   sdl_rs90_set_scale_frame_functions(vid);

   return vid;

error:
   sdl_rs90_gfx_free(vid);
   return NULL;
}

static void sdl_rs90_set_output(
      sdl_rs90_video_t* vid,
      unsigned width, unsigned height, bool rgb32)
{
   uint32_t surface_flags = (vid->vsync) ?
         SDL_RS90_SURFACE_FLAGS_VSYNC_ON :
         SDL_RS90_SURFACE_FLAGS_VSYNC_OFF;

   vid->content_width  = width;
   vid->content_height = height;

   /* Technically, "scale_integer" here just means "do not scale"
    * If the content is larger, we crop, otherwise we just centre
    * it in the frame.
    * If we want to support a core with an absolutely tiny screen
    * (i.e. less than 120x80), we should do actual integer scaling
    * (PokeMini @ 96x64 and VeMUlator @ 48x32 are probably the
    * only cores where this is an issue, but PokeMini at least
    * offers internal upscaling...) */
   if (vid->scale_integer)
   {
      if (width > SDL_RS90_WIDTH)
      {
         vid->frame_width     = SDL_RS90_WIDTH;
         vid->frame_crop_x    = (width - SDL_RS90_WIDTH) >> 1;
         vid->frame_padding_x = 0;
      }
      else
      {
         vid->frame_width     = width;
         vid->frame_crop_x    = 0;
         vid->frame_padding_x = (SDL_RS90_WIDTH - width) >> 1;
      }

      if (height > SDL_RS90_HEIGHT)
      {
         vid->frame_height    = SDL_RS90_HEIGHT;
         vid->frame_crop_y    = (height - SDL_RS90_HEIGHT) >> 1;
         vid->frame_padding_y = 0;
      }
      else
      {
         vid->frame_height    = height;
         vid->frame_crop_y    = 0;
         vid->frame_padding_y = (SDL_RS90_HEIGHT - height) >> 1;
      }
   }
   else
   {
      /* Normal scaling */
      if (vid->keep_aspect)
      {
         if (height * SDL_RS90_WIDTH > width * SDL_RS90_HEIGHT)
         {
            /* Integer math is fine */
            vid->frame_width  = (width * SDL_RS90_HEIGHT) / height;
            vid->frame_height = SDL_RS90_HEIGHT;
         }
         else
         {
            /* Integer math is fine */
            vid->frame_width  = SDL_RS90_WIDTH;
            vid->frame_height = (height * SDL_RS90_WIDTH) / width;
         }
      }
      else
      {
         vid->frame_width  = SDL_RS90_WIDTH;
         vid->frame_height = SDL_RS90_HEIGHT;
      }

      vid->frame_crop_x    = 0;
      vid->frame_padding_x = (SDL_RS90_WIDTH - vid->frame_width) >> 1;
      vid->frame_crop_y    = 0;
      vid->frame_padding_y = (SDL_RS90_HEIGHT - vid->frame_height) >> 1;
   }

   /* Attempt to change video mode */
   vid->screen = SDL_SetVideoMode(
         SDL_RS90_WIDTH, SDL_RS90_HEIGHT,
         rgb32 ? 32 : 16,
         surface_flags);

   /* Check whether selected display mode is valid */
   if (unlikely(!vid->screen))
   {
      RARCH_ERR("[SDL1]: Failed to init SDL surface: %s\n", SDL_GetError());
      vid->mode_valid = false;
   }
   else
   {
      /* Determine whether frame padding is required */
      if ((vid->frame_padding_x > 0) ||
          (vid->frame_padding_y > 0))
      {
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

static void sdl_rs90_blit_frame16(sdl_rs90_video_t *vid,
      uint16_t* src, unsigned width, unsigned height,
      unsigned src_pitch)
{
   /* If source and destination buffers have the
    * same pitch, perform fast copy of raw pixel data */
   /* TODO/FIXME: Make sure this code path is used for
    * GBA content */
   if (src_pitch == vid->screen->pitch &&
       height == SDL_RS90_HEIGHT)
      memcpy(vid->screen->pixels, src, src_pitch * SDL_RS90_HEIGHT);
   else
      vid->scale_frame16(vid, src, width, height, src_pitch);
}

static void sdl_rs90_blit_frame32(sdl_rs90_video_t *vid,
      uint32_t* src, unsigned width, unsigned height,
      unsigned src_pitch)
{
   /* If source and destination buffers have the
    * same pitch, perform fast copy of raw pixel data */
   /* TODO/FIXME: Make sure this code path is used for
    * GBA content */
   if ((src_pitch == vid->screen->pitch) &&
       (height == SDL_RS90_HEIGHT))
      memcpy(vid->screen->pixels, src, src_pitch * SDL_RS90_HEIGHT);
   else
      vid->scale_frame32(vid, src, width, height, src_pitch);
}

static bool sdl_rs90_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   sdl_rs90_video_t* vid = (sdl_rs90_video_t*)data;

   /* Return early if:
    * - Input sdl_rs90_video_t struct is NULL
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
            (vid->content_width  != width) ||
            (vid->content_height != height)))
         sdl_rs90_set_output(vid, width, height, vid->rgb32);

      /* Must always lock SDL surface before
       * manipulating raw pixel buffer */
      if (SDL_MUSTLOCK(vid->screen))
         SDL_LockSurface(vid->screen);

      if (likely(vid->mode_valid))
      {
         /* Blit frame to SDL surface */
         if (vid->rgb32)
            sdl_rs90_blit_frame32(vid, (uint32_t*)frame,
                  width, height, pitch);
         else
            sdl_rs90_blit_frame16(vid, (uint16_t*)frame,
                  width, height, pitch);
      }
      /* If current display mode is invalid,
       * just display an error message */
      else
         sdl_rs90_blit_video_mode_error_msg(vid);

      vid->was_in_menu = false;
   }
   else
   {
      /* If this is the first frame that the menu
       * is active, update video mode */
      if (!vid->was_in_menu)
      {
         sdl_rs90_set_output(vid,
               SDL_RS90_WIDTH, SDL_RS90_HEIGHT, false);

         vid->was_in_menu = true;
      }

      if (SDL_MUSTLOCK(vid->screen))
         SDL_LockSurface(vid->screen);

      /* Blit menu texture to SDL surface */
      sdl_rs90_blit_frame16(vid, vid->menu_texture,
            SDL_RS90_WIDTH, SDL_RS90_HEIGHT,
            SDL_RS90_WIDTH * sizeof(uint16_t));
   }

   /* Print OSD text, if required */
   if (msg)
   {
      /* If menu is active, colour depth is overridden
       * to 16 bit */
      if (vid->rgb32 && !vid->menu_active)
         sdl_rs90_blit_text32(vid, FONT_WIDTH_STRIDE,
               vid->screen->h - (FONT_HEIGHT + FONT_WIDTH_STRIDE), msg);
      else
         sdl_rs90_blit_text16(vid, FONT_WIDTH_STRIDE,
               vid->screen->h - (FONT_HEIGHT + FONT_WIDTH_STRIDE), msg);
   }

   /* Pixel manipulation complete - unlock
    * SDL surface */
   if (SDL_MUSTLOCK(vid->screen))
      SDL_UnlockSurface(vid->screen);

   SDL_Flip(vid->screen);

   return true;
}

static void sdl_rs90_set_texture_enable(void *data, bool state, bool full_screen)
{
   sdl_rs90_video_t *vid = (sdl_rs90_video_t*)data;

   if (unlikely(!vid))
      return;

   vid->menu_active = state;
}

static void sdl_rs90_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   sdl_rs90_video_t *vid = (sdl_rs90_video_t*)data;

   if (unlikely(
         !vid ||
         rgb32 ||
         (width > SDL_RS90_WIDTH) ||
         (height > SDL_RS90_HEIGHT)))
      return;

   memcpy(vid->menu_texture, frame, width * height * sizeof(uint16_t));
}

static void sdl_rs90_gfx_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   sdl_rs90_video_t *vid = (sdl_rs90_video_t*)data;
   bool vsync            = !toggle;

   if (unlikely(!vid))
      return;

   /* Check whether vsync status has changed */
   if (vid->vsync != vsync)
   {
      unsigned current_width  = vid->content_width;
      unsigned current_height = vid->content_height;
      vid->vsync              = vsync;

      /* Update video mode */

      /* TODO/FIXME: The following workaround is required
       * on GCW0 devices; check whether it is required on
       * the RS-90, and remove if not */

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
       * sdl_rs90_set_output() *twice*, setting the dimensions
       * to an arbitrary value before restoring the actual
       * desired width/height */
      sdl_rs90_set_output(vid,
            current_width,
            (current_height > 4) ? (current_height - 2) : 16,
            vid->rgb32);

      sdl_rs90_set_output(vid,
            current_width, current_height, vid->rgb32);
   }
}

static void sdl_rs90_gfx_check_window(sdl_rs90_video_t *vid)
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

static bool sdl_rs90_gfx_alive(void *data)
{
   sdl_rs90_video_t *vid = (sdl_rs90_video_t*)data;

   if (unlikely(!vid))
      return false;

   sdl_rs90_gfx_check_window(vid);
   return !vid->quitting;
}

static bool sdl_rs90_gfx_focus(void *data)
{
   return true;
}

static bool sdl_rs90_gfx_suppress_screensaver(void *data, bool enable)
{
   return false;
}

static bool sdl_rs90_gfx_has_windowed(void *data)
{
   return false;
}

static void sdl_rs90_gfx_viewport_info(void *data, struct video_viewport *vp)
{
   sdl_rs90_video_t *vid = (sdl_rs90_video_t*)data;

   if (unlikely(!vid))
      return;

   vp->x      = 0;
   vp->y      = 0;
   vp->width  = vp->full_width  = vid->frame_width;
   vp->height = vp->full_height = vid->frame_height;
}

static float sdl_rs90_get_refresh_rate(void *data)
{
#if defined(DINGUX_BETA)
   sdl_rs90_video_t *vid = (sdl_rs90_video_t*)data;

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

static void sdl_rs90_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   sdl_rs90_video_t *vid                            = (sdl_rs90_video_t*)data;
   settings_t *settings                             = config_get_ptr();
   enum dingux_rs90_softfilter_type softfilter_type = (settings) ?
         (enum dingux_rs90_softfilter_type)settings->uints.video_dingux_rs90_softfilter_type :
               DINGUX_RS90_SOFTFILTER_POINT;

   if (!vid || !settings)
      return;

   /* Update software filter setting, if required */
   if (vid->softfilter_type != softfilter_type)
   {
      vid->softfilter_type = softfilter_type;
      sdl_rs90_set_scale_frame_functions(vid);
   }
}

static void sdl_rs90_apply_state_changes(void *data)
{
   sdl_rs90_video_t *vid  = (sdl_rs90_video_t*)data;
   settings_t *settings   = config_get_ptr();
   bool keep_aspect       = (settings) ? settings->bools.video_dingux_ipu_keep_aspect : true;
   bool integer_scaling   = (settings) ? settings->bools.video_scale_integer : false;

   if (!vid || !settings)
      return;

   if ((vid->keep_aspect != keep_aspect) ||
       (vid->scale_integer != integer_scaling))
   {
      vid->keep_aspect   = keep_aspect;
      vid->scale_integer = integer_scaling;

      /* Reassign frame scaling function pointers */
      sdl_rs90_set_scale_frame_functions(vid);

      /* Aspect/scaling changes require all frame
       * dimension/padding/cropping parameters to
       * be recalculated. Easiest method is to just
       * (re-)set the current output video mode
       * Note: If menu is active, colour depth is
       * overridden to 16 bit */
      sdl_rs90_set_output(vid, vid->content_width,
            vid->content_height, vid->menu_active ? false : vid->rgb32);
   }
}

static uint32_t sdl_rs90_get_flags(void *data)
{
   return 0;
}

static const video_poke_interface_t sdl_rs90_poke_interface = {
   sdl_rs90_get_flags,
   NULL,
   NULL,
   NULL,
   sdl_rs90_get_refresh_rate,
   sdl_rs90_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL,
   sdl_rs90_apply_state_changes,
   sdl_rs90_set_texture_frame,
   sdl_rs90_set_texture_enable,
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

static void sdl_rs90_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   *iface = &sdl_rs90_poke_interface;
}

static bool sdl_rs90_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   return false;
}

video_driver_t video_sdl_rs90 = {
   sdl_rs90_gfx_init,
   sdl_rs90_gfx_frame,
   sdl_rs90_gfx_set_nonblock_state,
   sdl_rs90_gfx_alive,
   sdl_rs90_gfx_focus,
   sdl_rs90_gfx_suppress_screensaver,
   sdl_rs90_gfx_has_windowed,
   sdl_rs90_gfx_set_shader,
   sdl_rs90_gfx_free,
   "sdl_rs90",
   NULL,
   NULL, /* set_rotation */
   sdl_rs90_gfx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   sdl_rs90_get_poke_interface
};
