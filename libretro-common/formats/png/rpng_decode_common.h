/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng.c).
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

#ifndef _RPNG_DECODE_COMMON_H
#define _RPNG_DECODE_COMMON_H

static void png_pass_geom(const struct png_ihdr *ihdr,
      unsigned width, unsigned height,
      unsigned *bpp_out, unsigned *pitch_out, size_t *pass_size)
{
   unsigned bpp;
   unsigned pitch;

   switch (ihdr->color_type)
   {
      case 0:
         bpp   = (ihdr->depth + 7) / 8;
         pitch = (ihdr->width * ihdr->depth + 7) / 8;
         break;

      case 2:
         bpp   = (ihdr->depth * 3 + 7) / 8;
         pitch = (ihdr->width * ihdr->depth * 3 + 7) / 8;
         break;

      case 3:
         bpp   = (ihdr->depth + 7) / 8;
         pitch = (ihdr->width * ihdr->depth + 7) / 8;
         break;

      case 4:
         bpp   = (ihdr->depth * 2 + 7) / 8;
         pitch = (ihdr->width * ihdr->depth * 2 + 7) / 8;
         break;

      case 6:
         bpp   = (ihdr->depth * 4 + 7) / 8;
         pitch = (ihdr->width * ihdr->depth * 4 + 7) / 8;
         break;

      default:
         bpp = 0;
         pitch = 0;
         break;
   }

   if (pass_size)
      *pass_size = (pitch + 1) * ihdr->height;
   if (bpp_out)
      *bpp_out = bpp;
   if (pitch_out)
      *pitch_out = pitch;
}

static void deinterlace_pass(uint32_t *data, const struct png_ihdr *ihdr,
      const uint32_t *input, unsigned pass_width, unsigned pass_height,
      const struct adam7_pass *pass)
{
   unsigned x, y;

   data += pass->y * ihdr->width + pass->x;

   for (y = 0; y < pass_height;
         y++, data += ihdr->width * pass->stride_y, input += pass_width)
   {
      uint32_t *out = data;
     
      for (x = 0; x < pass_width; x++, out += pass->stride_x)
         *out = input[x];
   }
}

static bool png_reverse_filter_init(uint32_t *data, const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp)
{
   if (ihdr->interlace == 1)
      return true;

   png_pass_geom(ihdr, ihdr->width, ihdr->height, &pngp->bpp, &pngp->pitch, &pngp->pass_size);

   if (pngp->total_out < pngp->pass_size)
      return false;

   pngp->prev_scanline    = (uint8_t*)calloc(1, pngp->pitch);
   pngp->decoded_scanline = (uint8_t*)calloc(1, pngp->pitch);

   if (!pngp->prev_scanline || !pngp->decoded_scanline)
   {
      free(pngp->decoded_scanline);
      free(pngp->prev_scanline);
      return false;
   }

   return true;
}

static bool png_reverse_filter(uint32_t *data, const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp)
{
   unsigned i;
   bool ret = true;

   for (; pngp->h < ihdr->height;
         pngp->h++, pngp->inflate_buf += pngp->pitch, data += ihdr->width)
   {
      unsigned filter = *pngp->inflate_buf++;

      switch (filter)
      {
         case 0: /* None */
            memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
            break;

         case 1: /* Sub */
            for (i = 0; i < pngp->bpp; i++)
               pngp->decoded_scanline[i] = pngp->inflate_buf[i];
            for (i = pngp->bpp; i < pngp->pitch; i++)
               pngp->decoded_scanline[i] = pngp->decoded_scanline[i - pngp->bpp] + pngp->inflate_buf[i];
            break;

         case 2: /* Up */
            for (i = 0; i < pngp->pitch; i++)
               pngp->decoded_scanline[i] = pngp->prev_scanline[i] + pngp->inflate_buf[i];
            break;

         case 3: /* Average */
            for (i = 0; i < pngp->bpp; i++)
            {
               uint8_t avg = pngp->prev_scanline[i] >> 1;

               pngp->decoded_scanline[i] = avg + pngp->inflate_buf[i];
            }
            for (i = pngp->bpp; i < pngp->pitch; i++)
            {
               uint8_t avg = (pngp->decoded_scanline[i - pngp->bpp] + pngp->prev_scanline[i]) >> 1;

               pngp->decoded_scanline[i] = avg + pngp->inflate_buf[i];
            }
            break;

         case 4: /* Paeth */
            for (i = 0; i < pngp->bpp; i++)
               pngp->decoded_scanline[i] = paeth(0, pngp->prev_scanline[i], 0) + pngp->inflate_buf[i];
            for (i = pngp->bpp; i < pngp->pitch; i++)
               pngp->decoded_scanline[i] = paeth(pngp->decoded_scanline[i - pngp->bpp],
                     pngp->prev_scanline[i], pngp->prev_scanline[i - pngp->bpp]) + pngp->inflate_buf[i];
            break;

         default:
            GOTO_END_ERROR();
      }

      if (ihdr->color_type == 0)
         copy_line_bw(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
      else if (ihdr->color_type == 2)
         copy_line_rgb(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
      else if (ihdr->color_type == 3)
         copy_line_plt(data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth, pngp->palette);
      else if (ihdr->color_type == 4)
         copy_line_gray_alpha(data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth);
      else if (ihdr->color_type == 6)
         copy_line_rgba(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);

      memcpy(pngp->prev_scanline, pngp->decoded_scanline, pngp->pitch);
   }

end:
   if (pngp->decoded_scanline)
      free(pngp->decoded_scanline);
   if (pngp->prev_scanline)
      free(pngp->prev_scanline);
   return ret;
}

static bool png_reverse_filter_adam7(uint32_t *data,
      const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp)
{
   static const struct adam7_pass passes[] = {
      { 0, 0, 8, 8 },
      { 4, 0, 8, 8 },
      { 0, 4, 4, 8 },
      { 2, 0, 4, 4 },
      { 0, 2, 2, 4 },
      { 1, 0, 2, 2 },
      { 0, 1, 1, 2 },
   };

   for (; pngp->pass < ARRAY_SIZE(passes); pngp->pass++)
   {
      unsigned pass_width, pass_height;
      struct png_ihdr tmp_ihdr;
      uint32_t *tmp_data = NULL;

      if (ihdr->width <= passes[pngp->pass].x ||
            ihdr->height <= passes[pngp->pass].y) /* Empty pass */
         continue;

      pass_width  = (ihdr->width - 
            passes[pngp->pass].x + passes[pngp->pass].stride_x - 1) / passes[pngp->pass].stride_x;
      pass_height = (ihdr->height - passes[pngp->pass].y + 
            passes[pngp->pass].stride_y - 1) / passes[pngp->pass].stride_y;

      tmp_data = (uint32_t*)malloc(
            pass_width * pass_height * sizeof(uint32_t));

      if (!tmp_data)
         return false;

      tmp_ihdr = *ihdr;
      tmp_ihdr.width = pass_width;
      tmp_ihdr.height = pass_height;

      png_pass_geom(&tmp_ihdr, pass_width,
            pass_height, NULL, NULL, &pngp->pass_size);

      if (pngp->pass_size > pngp->total_out)
      {
         free(tmp_data);
         return false;
      }

      if (!png_reverse_filter(tmp_data,
               &tmp_ihdr, pngp))
      {
         free(tmp_data);
         return false;
      }

      pngp->inflate_buf     += pngp->pass_size;
      pngp->total_out       -= pngp->pass_size;

      deinterlace_pass(data,
            ihdr, tmp_data, pass_width, pass_height, &passes[pngp->pass]);
      free(tmp_data);
   }

   return true;
}

#endif
