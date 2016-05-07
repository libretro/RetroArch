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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <boolean.h>
#include <formats/image.h>
#include <file/nbio.h>
#ifdef HAVE_RPNG
#include <formats/rpng.h>
#endif
#include <formats/tga.h>
#include <streams/file_stream.h>

#include "../general.h"

enum video_image_format
{
   IMAGE_FORMAT_NONE = 0,
   IMAGE_FORMAT_TGA,
   IMAGE_FORMAT_PNG
};

bool video_texture_image_set_color_shifts(
      unsigned *r_shift, unsigned *g_shift, unsigned *b_shift,
      unsigned *a_shift)
{
   *a_shift             = 24;
   *r_shift             = 16;
   *g_shift             = 8;
   *b_shift             = 0;

   if (video_driver_ctl(
         RARCH_DISPLAY_CTL_SUPPORTS_RGBA, NULL))
   {
      *r_shift = 0;
      *b_shift = 16;
      return true;
   }

   return false;
}

bool video_texture_image_color_convert(unsigned r_shift,
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

static bool video_texture_image_rpng_gx_convert_texture32(
      struct texture_image *image)
{
   int ret;
   unsigned tmp_pitch, width2, i;
   const uint16_t *src = NULL;
   uint16_t *dst       = NULL;
   /* Memory allocation in libogc is extremely primitive so try 
    * to avoid gaps in memory when converting by copying over to 
    * a temporary buffer first, then converting over into 
    * main buffer again. */
   void *tmp           = malloc(image->width 
         * image->height * sizeof(uint32_t));

   if (!tmp)
   {
      RARCH_ERR("Failed to create temp buffer for conversion.\n");
      return false;
   }

   memcpy(tmp, image->pixels, image->width 
         * image->height * sizeof(uint32_t));
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

static bool video_texture_image_load_png(
      void *ptr,
      struct texture_image *out_img,
      unsigned a_shift, unsigned r_shift,
      unsigned g_shift, unsigned b_shift)
{
   int ret;
   bool success = false;
   rpng_t *rpng = rpng_alloc();

   if (!rpng)
      goto end;

   if (!rpng_set_buf_ptr(rpng, (uint8_t*)ptr))
      goto end;

   if (!rpng_nbio_load_image_argb_start(rpng))
      goto end;

   while (rpng_nbio_load_image_argb_iterate(rpng));

   if (!rpng_is_valid(rpng))
      goto end;

   do
   {
      ret = rpng_nbio_load_image_argb_process(rpng, &out_img->pixels, &out_img->width,
            &out_img->height);
   }while(ret == IMAGE_PROCESS_NEXT);

   if (ret == IMAGE_PROCESS_ERROR || ret == IMAGE_PROCESS_ERROR_END)
      goto end;

   video_texture_image_color_convert(r_shift, g_shift, b_shift,
         a_shift, out_img);

#ifdef GEKKO
   if (!video_texture_image_rpng_gx_convert_texture32(out_img))
   {
      video_texture_image_free(out_img);
      goto end;
   }
#endif

   success = true;

end:
   if (rpng)
      rpng_nbio_load_image_free(rpng);

   return success;
}
#endif


void video_texture_image_free(struct texture_image *img)
{
   if (!img)
      return;

   if (img->pixels)
      free(img->pixels);
   memset(img, 0, sizeof(*img));
}

static enum video_image_format video_texture_image_get_type(const char *path)
{
   if (strstr(path, ".tga"))
      return IMAGE_FORMAT_TGA;
   if (strstr(path, ".png"))
      return IMAGE_FORMAT_PNG;
   return IMAGE_FORMAT_NONE;
}

bool video_texture_image_load(struct texture_image *out_img, 
      const char *path)
{
   size_t file_len;
   unsigned r_shift, g_shift, b_shift, a_shift;
   struct nbio_t      *handle  = NULL;
   void                   *ptr = NULL;
   enum video_image_format fmt = video_texture_image_get_type(path);

   video_texture_image_set_color_shifts(&r_shift, &g_shift, &b_shift,
         &a_shift);

   switch (fmt)
   {
      case IMAGE_FORMAT_NONE:
         break;
      default:
         handle = (struct nbio_t*)nbio_open(path, NBIO_READ);
         if (!handle)
            goto error;
         nbio_begin_read(handle);

         while (!nbio_iterate(handle));

         ptr = nbio_get_ptr(handle, &file_len);

         if (!ptr)
            goto error;
         break;
   }

   switch (fmt)
   {
      case IMAGE_FORMAT_TGA:
         if (rtga_image_load_shift(ptr, out_img,
                  a_shift, r_shift, g_shift, b_shift))
            goto success;
         break;
      case IMAGE_FORMAT_PNG:
#ifdef HAVE_RPNG
         if (video_texture_image_load_png(ptr, out_img,
               a_shift, r_shift, g_shift, b_shift))
            goto success;
#endif
         break;
      default:
      case IMAGE_FORMAT_NONE:
         break;
   }

error:
   out_img->pixels = NULL;
   out_img->width  = 0;
   out_img->height = 0;
   if (handle)
      nbio_free(handle);

   return false;

success:
   if (handle)
      nbio_free(handle);

   return true;
}
