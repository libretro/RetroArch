/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "../../general.h"
#include "../../file_ops.h"
#include <file/nbio.h>

#ifdef HAVE_ZLIB
static bool rpng_nbio_load_image_argb_nonblocking(const char *path, uint32_t **data,
      unsigned *width, unsigned *height)
{
   size_t file_len;
   bool ret = true;
   struct rpng_t *rpng = NULL;
   void *ptr = NULL;
   struct nbio_t* handle = (void*)nbio_open(path, NBIO_READ);

   if (!handle)
      goto end;

   ptr  = nbio_get_ptr(handle, &file_len);

   nbio_begin_read(handle);

   while (!nbio_iterate(handle));

   ptr = nbio_get_ptr(handle, &file_len);

   if (!ptr)
   {
      ret = false;
      goto end;
   }

   rpng = (struct rpng_t*)calloc(1, sizeof(struct rpng_t));

   if (!rpng)
   {
      ret = false;
      goto end;
   }

   rpng->buff_data = (uint8_t*)ptr;

   if (!rpng->buff_data)
   {
      ret = false;
      goto end;
   }

   if (!rpng_nbio_load_image_argb_start(rpng))
   {
      ret = false;
      goto end;
   }

   while (rpng_nbio_load_image_argb_iterate(
            rpng->buff_data, rpng))
   {
      rpng->buff_data += 4 + 4 + rpng->chunk.size + 4;
   }

#if 0
   fprintf(stderr, "has_ihdr: %d\n", rpng->has_ihdr);
   fprintf(stderr, "has_idat: %d\n", rpng->has_idat);
   fprintf(stderr, "has_iend: %d\n", rpng->has_iend);
#endif

   if (!rpng->has_ihdr || !rpng->has_idat || !rpng->has_iend)
   {
      ret = false;
      goto end;
   }
   
   rpng_nbio_load_image_argb_process(rpng, data, width, height);

end:
   if (handle)
      nbio_free(handle);
   if (rpng)
      rpng_nbio_load_image_free(rpng);
   rpng = NULL;
   if (!ret)
      free(*data);
   return ret;
}

static bool rpng_image_load_argb_shift(const char *path,
      struct texture_image *out_img,
      unsigned a_shift, unsigned r_shift,
      unsigned g_shift, unsigned b_shift)
{
   bool ret = rpng_nbio_load_image_argb_nonblocking(path,
         &out_img->pixels, &out_img->width, &out_img->height);

   if (!ret)
      return false;

   /* This is quite uncommon. */
   if (a_shift != 24 || r_shift != 16 || g_shift != 8 || b_shift != 0)
   {
      uint32_t i;
      uint32_t num_pixels = out_img->width * out_img->height;
      uint32_t *pixels = (uint32_t*)out_img->pixels;

      for (i = 0; i < num_pixels; i++)
      {
         uint32_t col = pixels[i];
         uint8_t a = (uint8_t)(col >> 24);
         uint8_t r = (uint8_t)(col >> 16);
         uint8_t g = (uint8_t)(col >>  8);
         uint8_t b = (uint8_t)(col >>  0);
         pixels[i] = (a << a_shift) |
            (r << r_shift) | (g << g_shift) | (b << b_shift);
      }
   }

   return true;
}
#endif

#ifdef GEKKO

#define GX_BLIT_LINE_32(off) \
{ \
   const uint16_t *tmp_src = src; \
   uint16_t *tmp_dst = dst; \
   for (unsigned x = 0; x < width2 >> 3; x++, tmp_src += 8, tmp_dst += 32) \
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
   const uint16_t *src;
   uint16_t *dst;
   /* Memory allocation in libogc is extremely primitive so try 
    * to avoid gaps in memory when converting by copying over to 
    * a temporary buffer first, then converting over into 
    * main buffer again. */
   void *tmp = malloc(image->width * image->height * sizeof(uint32_t));

   if (!tmp)
   {
      RARCH_ERR("Failed to create temp buffer for conversion.\n");
      return false;
   }

   memcpy(tmp, image->pixels, image->width * image->height * sizeof(uint32_t));
   tmp_pitch = (image->width * sizeof(uint32_t)) >> 1;

   image->width &= ~3;
   image->height &= ~3;

   width2 = image->width << 1;

   src = (uint16_t *) tmp;
   dst = (uint16_t *) image->pixels;

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
   /* This interface "leak" is very ugly. FIXME: Fix this properly ... */
   bool ret         = false;
   bool use_rgba    = driver.gfx_use_rgba;
   unsigned a_shift = 24;
   unsigned r_shift = use_rgba ? 0 : 16;
   unsigned g_shift = 8;
   unsigned b_shift = use_rgba ? 16 : 0;

#ifdef HAVE_ZLIB
   if (strstr(path, ".png"))
   {
      ret = rpng_image_load_argb_shift(path, out_img,
            a_shift, r_shift, g_shift, b_shift);
   }
#endif

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

   return ret;
}
