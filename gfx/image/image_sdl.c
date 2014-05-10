/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "../image_context.h"
#include "../../file.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "../../general.h"
#include "../rpng/rpng.h"

#include "SDL_image.h"

static bool sdl_load_argb_shift(const char *path, struct texture_image *out_img,
      unsigned a_shift, unsigned r_shift, unsigned g_shift, unsigned b_shift)
{
   int y, x;
   size_t size;
   SDL_PixelFormat *fmt;
   SDL_Surface *img = (SDL_Surface*)IMG_Load(path);
   if (!img)
      return false;

   out_img->width = img->w;
   out_img->height = img->h;

   size = out_img->width * out_img->height * sizeof(uint32_t);
   out_img->pixels = (uint32_t*)malloc(size);

   if (!out_img->pixels)
   {
      SDL_FreeSurface(img);
      return false;
   }

   fmt = (SDL_PixelFormat*)img->format;
   
   RARCH_LOG("SDL_image: %dx%d @ %d bpp\n", img->w, img->h, img->format->BitsPerPixel);

   if (img->format->BitsPerPixel == 32)
   {
      for (y = 0; y < img->h; y++)
      {
         uint32_t *dst = (uint32_t*)(out_img->pixels + y * img->w);
         const uint32_t *src = (const uint32_t*)(img->pixels + y * img->pitch / sizeof(uint32_t));

         for (x = 0; x < img->w; x++)
         {
            uint32_t r = (src[x] & fmt->Rmask) >> fmt->Rshift;
            uint32_t g = (src[x] & fmt->Gmask) >> fmt->Gshift;
            uint32_t b = (src[x] & fmt->Bmask) >> fmt->Bshift;
            uint32_t a = (src[x] & fmt->Amask) >> fmt->Ashift;
            dst[x] = (a << a_shift) | (r << r_shift) | (g << g_shift) | (b << b_shift);
         }
      }
   }
   else if (img->format->BitsPerPixel == 24)
   {
      for (y = 0; y < img->h; y++)
      {
         uint32_t *dst = (uint32_t*)(out_img->pixels + y * img->w);
         const uint8_t *src = (const uint8_t*)(img->pixels + y * img->pitch);

         for (x = 0; x < img->w; x++)
         {
            // Correct?
            uint32_t color,r, g, b;
            color = 0;
            color |= src[3 * x + 0] << 0;
            color |= src[3 * x + 1] << 8;
            color |= src[3 * x + 2] << 16;
            r = (color & fmt->Rmask) >> fmt->Rshift;
            g = (color & fmt->Gmask) >> fmt->Gshift;
            b = (color & fmt->Bmask) >> fmt->Bshift;
            dst[x] = (0xff << a_shift) | (r << r_shift) | (g << g_shift) | (b << b_shift);
         }
      }
   }
   else
   {
      RARCH_ERR("8-bit and 16-bit image support are not implemented.\n");
      SDL_FreeSurface(img);
      return false;
   }

   SDL_FreeSurface(img);

   return true;
}

static bool sdl_image_load(void *data, const char *path, void *image_data)
{
   bool ret;
   (void)data;
   struct texture_image *out_img = (struct texture_image*)image_data;

   // This interface "leak" is very ugly. FIXME: Fix this properly ...
  
   if (driver.gfx_use_rgba)
      ret = sdl_load_argb_shift(path, out_img, 24, 0, 8, 16);
   else
      ret = sdl_load_argb_shift(path, out_img, 24, 16, 8, 0);

   return ret;
}

static void sdl_image_free(void *data, void *image_data)
{
   struct texture_image *img = (struct texture_image*)image_data;
   free(img->pixels);
   memset(img, 0, sizeof(*img));
}

const image_ctx_driver_t image_ctx_sdl = {
   sdl_image_load,
   sdl_image_free,
   "sdl",
};
