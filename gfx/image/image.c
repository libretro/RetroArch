/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <formats/image.h>
#ifdef HAVE_RPNG
#include <formats/rpng.h>
#endif
#include <formats/tga.h>
#include "../../file_ops.h"
#include "../../verbosity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <boolean.h>
#include "../../general.h"

bool texture_image_set_color_shifts(unsigned *r_shift, unsigned *g_shift,
      unsigned *b_shift, unsigned *a_shift)
{
   bool use_rgba        = video_driver_ctl(RARCH_DISPLAY_CTL_SUPPORTS_RGBA, NULL);
   *a_shift             = 24;
   *r_shift             = use_rgba ? 0 : 16;
   *g_shift             = 8;
   *b_shift             = use_rgba ? 16 : 0;

   return use_rgba;
}

bool texture_image_color_convert(unsigned r_shift,
      unsigned g_shift, unsigned b_shift, unsigned a_shift,
      struct texture_image *out_img)
{
   /* This is quite uncommon. */
   if (a_shift != 24 || r_shift != 16 || g_shift != 8 || b_shift != 0)
   {
      uint32_t i;
      uint32_t num_pixels = out_img->width * out_img->height;
      uint32_t *pixels    = (uint32_t*)out_img->pixels;

      for (i = 0; i < num_pixels; i++)
      {
         uint32_t col = pixels[i];
         uint8_t a    = (uint8_t)(col >> 24);
         uint8_t r    = (uint8_t)(col >> 16);
         uint8_t g    = (uint8_t)(col >>  8);
         uint8_t b    = (uint8_t)(col >>  0);
         pixels[i]    = (a << a_shift) |
            (r << r_shift) | (g << g_shift) | (b << b_shift);
      }

      return true;
   }

   return false;
}

#ifdef HAVE_RPNG
static bool rpng_image_load_argb_shift(const char *path,
      struct texture_image *out_img,
      unsigned a_shift, unsigned r_shift,
      unsigned g_shift, unsigned b_shift)
{
   bool ret = rpng_load_image_argb(path,
         &out_img->pixels, &out_img->width, &out_img->height);

   if (!ret)
   {
      out_img->pixels = NULL;
      out_img->width  = out_img->height = 0;
      return false;
   }

   texture_image_color_convert(r_shift, g_shift, b_shift,
         a_shift, out_img);

   return true;
}
#endif

#ifdef GEKKO

#define GX_BLIT_LINE_32(off) \
{ \
   unsigned x; \
   const uint16_t *tmp_src = src; \
   uint16_t       *tmp_dst = dst; \
   for (x = 0; x < width2 >> 3; x++, tmp_src += 8, tmp_dst += 32) \
   { \
      tmp_dst[  0 + off] = tmp_src[0]; \
      tmp_dst[ 16 + off] = tmp_src[1]; \
      tmp_dst[  1 + off] = tmp_src[2]; \
      tmp_dst[ 17 + off] = tmp_src[3]; \
      tmp_dst[  2 + off] = tmp_src[4]; \
      tmp_dst[ 18 + off] = tmp_src[5]; \
      tmp_dst[  3 + off] = tmp_src[6]; \
      tmp_dst[ 19 + off] = tmp_src[7]; \
   } \
   src += tmp_pitch; \
}

static bool rpng_gx_convert_texture32(struct texture_image *image)
{
   unsigned tmp_pitch, width2, i;
   const uint16_t *src = NULL;
   uint16_t *dst       = NULL;
   /* Memory allocation in libogc is extremely primitive so try 
    * to avoid gaps in memory when converting by copying over to 
    * a temporary buffer first, then converting over into 
    * main buffer again. */
   void *tmp           = malloc(image->width * image->height * sizeof(uint32_t));

   if (!tmp)
   {
      RARCH_ERR("Failed to create temp buffer for conversion.\n");
      return false;
   }

   memcpy(tmp, image->pixels, image->width * image->height * sizeof(uint32_t));
   tmp_pitch = (image->width * sizeof(uint32_t)) >> 1;

   image->width       &= ~3;
   image->height      &= ~3;
   width2              = image->width << 1;
   src                 = (uint16_t*)tmp;
   dst                 = (uint16_t*)image->pixels;

   for (i = 0; i < image->height; i += 4, dst += 4 * width2)
   {
      GX_BLIT_LINE_32(0)
      GX_BLIT_LINE_32(4)
      GX_BLIT_LINE_32(8)
      GX_BLIT_LINE_32(12)
   }

   free(tmp);
   return true;
}

#endif

void texture_image_free(struct texture_image *img)
{
   if (!img)
      return;

   if (img->pixels)
      free(img->pixels);
   memset(img, 0, sizeof(*img));
}

bool texture_image_load(struct texture_image *out_img, const char *path)
{
   unsigned r_shift, g_shift, b_shift, a_shift;
   bool ret = false;

   texture_image_set_color_shifts(&r_shift, &g_shift, &b_shift,
         &a_shift);

   (void)ret;

   if (strstr(path, ".tga"))
   {
      ssize_t len;
      void *raw_buf = NULL;
      uint8_t *buf  = NULL;
      ret = read_file(path, &raw_buf, &len);

      if (!ret || len < 0)
      {
         RARCH_ERR("Failed to read image: %s.\n", path);
         return false;
      }

      buf = (uint8_t*)raw_buf;

      ret = rtga_image_load_shift(buf, out_img,
            a_shift, r_shift, g_shift, b_shift);

      if (buf)
         free(buf);
   }
#ifdef HAVE_RPNG
   else if (strstr(path, ".png"))
   {
      ret = rpng_image_load_argb_shift(path, out_img,
            a_shift, r_shift, g_shift, b_shift);
   }

#ifdef GEKKO
   if (ret)
   {
      if (!rpng_gx_convert_texture32(out_img))
      {
         texture_image_free(out_img);
         return false;
      }
   }
#endif

#endif

   return ret;
}
