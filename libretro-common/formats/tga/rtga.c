/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtga.c).
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <formats/tga.h>
#include <formats/image.h>

bool rtga_image_load_shift(uint8_t *buf,
      void *data,
      unsigned a_shift, unsigned r_shift,
      unsigned g_shift, unsigned b_shift)
{
   unsigned i, bits, size, bits_mul;
   uint8_t               info[6] = {0};
   unsigned                width = 0;
   unsigned               height = 0;
   const uint8_t            *tmp = NULL;
   struct texture_image *out_img = (struct texture_image*)data;

   if (!buf || buf[2] != 2)
   {
      fprintf(stderr, "TGA image is not uncompressed RGB.\n");
      goto error;
   }

   memcpy(info, buf + 12, 6);

   width  = info[0] + ((unsigned)info[1] * 256);
   height = info[2] + ((unsigned)info[3] * 256);
   bits   = info[4];

   fprintf(stderr, "Loaded TGA: (%ux%u @ %u bpp)\n", width, height, bits);

   size            = width * height * sizeof(uint32_t);
   out_img->pixels = (uint32_t*)malloc(size);
   out_img->width  = width;
   out_img->height = height;

   if (!out_img->pixels)
   {
      fprintf(stderr, "Failed to allocate TGA pixels.\n");
      goto error;
   }

   tmp      = buf + 18;
   bits_mul = 3;

   if (bits != 32 && bits != 24)
   {
      fprintf(stderr, "Bit depth of TGA image is wrong. Only 32-bit and 24-bit supported.\n");
      goto error;
   }

   if (bits == 32)
      bits_mul = 4;

   for (i = 0; i < width * height; i++)
   {
      uint32_t b = tmp[i * bits_mul + 0];
      uint32_t g = tmp[i * bits_mul + 1];
      uint32_t r = tmp[i * bits_mul + 2];
      uint32_t a = tmp[i * bits_mul + 3];

      if (bits == 24)
         a = 0xff;

      out_img->pixels[i] = (a << a_shift) |
         (r << r_shift) | (g << g_shift) | (b << b_shift);
   }

   return true;

error:
   if (out_img->pixels)
      free(out_img->pixels);

   out_img->pixels = NULL;
   out_img->width  = out_img->height = 0;
   return false;
}
