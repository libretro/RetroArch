/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include "../config.h"
#endif

#include "image.h"
#include "../file.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "../general.h"

#ifdef HAVE_SDL_IMAGE

#include "SDL_image.h"
bool texture_image_load_argb_shift(const char *path, struct texture_image *out_img,
      unsigned a_shift, unsigned r_shift, unsigned g_shift, unsigned b_shift)
{
   SDL_Surface *img = IMG_Load(path);
   if (!img)
      return false;

   out_img->width = img->w;
   out_img->height = img->h;

   size_t size = out_img->width * out_img->height * sizeof(uint32_t);
   out_img->pixels = (uint32_t*)malloc(size);
   if (!out_img->pixels)
   {
      SDL_FreeSurface(img);
      return false;
   }

   const SDL_PixelFormat *fmt = img->format;
   
   RARCH_LOG("SDL_image: %dx%d @ %d bpp\n", img->w, img->h, img->format->BitsPerPixel);
   if (img->format->BitsPerPixel == 32)
   {
      for (int y = 0; y < img->h; y++)
      {
         uint32_t *dst = out_img->pixels + y * img->w;
         const uint32_t *src = (const uint32_t*)img->pixels + y * img->pitch / sizeof(uint32_t);

         for (int x = 0; x < img->w; x++)
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
      for (int y = 0; y < img->h; y++)
      {
         uint32_t *dst = out_img->pixels + y * img->w;
         const uint8_t *src = (const uint8_t*)img->pixels + y * img->pitch;

         for (int x = 0; x < img->w; x++)
         {
            // Correct?
            uint32_t color = 0;
            color |= src[3 * x + 0] << 0;
            color |= src[3 * x + 1] << 8;
            color |= src[3 * x + 2] << 16;
            uint32_t r = (color & fmt->Rmask) >> fmt->Rshift;
            uint32_t g = (color & fmt->Gmask) >> fmt->Gshift;
            uint32_t b = (color & fmt->Bmask) >> fmt->Bshift;
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

#else

bool texture_image_load_argb_shift(const char *path, struct texture_image *out_img,
      unsigned a_shift, unsigned r_shift, unsigned g_shift, unsigned b_shift)
{
   // TODO: Check more gracefully.
   if (!strstr(path, ".tga"))
   {
      RARCH_ERR("Extension is not .tga.\n");
      return false;
   }

   void *raw_buf = NULL;
   ssize_t len = read_file(path, &raw_buf);
   if (len < 0)
   {
      RARCH_ERR("Failed to read image: %s.\n", path);
      return false;
   }

   uint8_t *buf = (uint8_t*)raw_buf;

   if (buf[2] != 2) // Uncompressed RGB
   {
      RARCH_ERR("TGA image is not uncompressed RGB.\n");
      free(buf);
      return false;
   }

   unsigned width = 0;
   unsigned height = 0;

   uint8_t info[6];
   memcpy(info, buf + 12, 6);

   width = info[0] + ((unsigned)info[1] * 256);
   height = info[2] + ((unsigned)info[3] * 256);
   unsigned bits = info[4];

   RARCH_LOG("Loaded TGA: (%ux%u @ %u bpp)\n", width, height, bits);

   unsigned size = width * height * sizeof(uint32_t);
   out_img->pixels = (uint32_t*)malloc(size);
   out_img->width = width;
   out_img->height = height;
   if (!out_img->pixels)
   {
      RARCH_ERR("Failed to allocate TGA pixels.\n");
      free(buf);
      return false;
   }

   const uint8_t *tmp = buf + 18;
   if (bits == 32)
   {
      for (unsigned i = 0; i < width * height; i++)
      {
         uint32_t b = tmp[i * 4 + 0];
         uint32_t g = tmp[i * 4 + 1];
         uint32_t r = tmp[i * 4 + 2];
         uint32_t a = tmp[i * 4 + 3];

         out_img->pixels[i] = (a << a_shift) | (r << r_shift) | (g << g_shift) | (b << b_shift);
      }
   }
   else if (bits == 24)
   {
      for (unsigned i = 0; i < width * height; i++)
      {
         uint32_t b = tmp[i * 3 + 0];
         uint32_t g = tmp[i * 3 + 1];
         uint32_t r = tmp[i * 3 + 2];
         uint32_t a = 0xff;

         out_img->pixels[i] = (a << a_shift) | (r << r_shift) | (g << g_shift) | (b << b_shift);
      }
   }
   else
   {
      RARCH_ERR("Bit depth of TGA image is wrong. Only 32-bit and 24-bit supported.\n");
      free(buf);
      free(out_img->pixels);
      out_img->pixels = NULL;
      return false;
   }

   free(buf);
   return true;
}

#endif

bool texture_image_load(const char *path, struct texture_image *out_img)
{
   // This interface "leak" is very ugly. FIXME: Fix this properly ...
   if (driver.gfx_use_rgba)
      return texture_image_load_argb_shift(path, out_img, 24, 0, 8, 16);
   else
      return texture_image_load_argb_shift(path, out_img, 24, 16, 8, 0);
}

