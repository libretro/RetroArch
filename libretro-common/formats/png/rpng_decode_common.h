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

static enum png_chunk_type png_chunk_type(const struct png_chunk *chunk)
{
   unsigned i;
   struct
   {
      const char *id;
      enum png_chunk_type type;
   } static const chunk_map[] = {
      { "IHDR", PNG_CHUNK_IHDR },
      { "IDAT", PNG_CHUNK_IDAT },
      { "IEND", PNG_CHUNK_IEND },
      { "PLTE", PNG_CHUNK_PLTE },
   };

   for (i = 0; i < ARRAY_SIZE(chunk_map); i++)
   {
      if (memcmp(chunk->type, chunk_map[i].id, 4) == 0)
         return chunk_map[i].type;
   }

   return PNG_CHUNK_NOOP;
}

static void png_reverse_filter_copy_line_rgb(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned bpp)
{
   unsigned i;

   bpp /= 8;

   for (i = 0; i < width; i++)
   {
      uint32_t r, g, b;

      r        = *decoded;
      decoded += bpp;
      g        = *decoded;
      decoded += bpp;
      b        = *decoded;
      decoded += bpp;
      data[i]  = (0xffu << 24) | (r << 16) | (g << 8) | (b << 0);
   }
}

static void png_reverse_filter_copy_line_rgba(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned bpp)
{
   unsigned i;

   bpp /= 8;

   for (i = 0; i < width; i++)
   {
      uint32_t r, g, b, a;
      r        = *decoded;
      decoded += bpp;
      g        = *decoded;
      decoded += bpp;
      b        = *decoded;
      decoded += bpp;
      a        = *decoded;
      decoded += bpp;
      data[i]  = (a << 24) | (r << 16) | (g << 8) | (b << 0);
   }
}

static void png_reverse_filter_copy_line_bw(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned depth)
{
   unsigned i, bit;
   static const unsigned mul_table[] = { 0, 0xff, 0x55, 0, 0x11, 0, 0, 0, 0x01 };
   unsigned mul, mask;
   
   if (depth == 16)
   {
      for (i = 0; i < width; i++)
      {
         uint32_t val = decoded[i << 1];
         data[i]      = (val * 0x010101) | (0xffu << 24);
      }
      return;
   }

   mul  = mul_table[depth];
   mask = (1 << depth) - 1;
   bit  = 0;

   for (i = 0; i < width; i++, bit += depth)
   {
      unsigned byte = bit >> 3;
      unsigned val  = decoded[byte] >> (8 - depth - (bit & 7));

      val          &= mask;
      val          *= mul;
      data[i]       = (val * 0x010101) | (0xffu << 24);
   }
}

static void png_reverse_filter_copy_line_gray_alpha(uint32_t *data,
      const uint8_t *decoded, unsigned width,
      unsigned bpp)
{
   unsigned i;

   bpp /= 8;

   for (i = 0; i < width; i++)
   {
      uint32_t gray, alpha;

      gray     = *decoded;
      decoded += bpp;
      alpha    = *decoded;
      decoded += bpp;

      data[i]  = (gray * 0x010101) | (alpha << 24);
   }
}

static void png_reverse_filter_copy_line_plt(uint32_t *data,
      const uint8_t *decoded, unsigned width,
      unsigned depth, const uint32_t *palette)
{
   unsigned i, bit;
   unsigned mask = (1 << depth) - 1;

   bit = 0;

   for (i = 0; i < width; i++, bit += depth)
   {
      unsigned byte = bit >> 3;
      unsigned val  = decoded[byte] >> (8 - depth - (bit & 7));

      val          &= mask;
      data[i]       = palette[val];
   }
}

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

static void png_reverse_filter_adam7_deinterlace_pass(uint32_t *data,
      const struct png_ihdr *ihdr,
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

static void png_reverse_filter_deinit(struct rpng_process_t *pngp)
{
   if (pngp->decoded_scanline)
      free(pngp->decoded_scanline);
   pngp->decoded_scanline = NULL;
   if (pngp->prev_scanline)
      free(pngp->prev_scanline);
   pngp->prev_scanline    = NULL;

   pngp->pass_initialized = false;
   pngp->h                = 0;
}

static const struct adam7_pass passes[] = {
   { 0, 0, 8, 8 },
   { 4, 0, 8, 8 },
   { 0, 4, 4, 8 },
   { 2, 0, 4, 4 },
   { 0, 2, 2, 4 },
   { 1, 0, 2, 2 },
   { 0, 1, 1, 2 },
};

static int png_reverse_filter_init(const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp)
{
   size_t pass_size;

   if (!pngp->adam7_pass_initialized && ihdr->interlace)
   {
      if (ihdr->width <= passes[pngp->pass.pos].x ||
            ihdr->height <= passes[pngp->pass.pos].y) /* Empty pass */
         return 1;

      pngp->pass.width  = (ihdr->width - 
            passes[pngp->pass.pos].x + passes[pngp->pass.pos].stride_x - 1) / passes[pngp->pass.pos].stride_x;
      pngp->pass.height = (ihdr->height - passes[pngp->pass.pos].y + 
            passes[pngp->pass.pos].stride_y - 1) / passes[pngp->pass.pos].stride_y;

      pngp->data = (uint32_t*)malloc(
            pngp->pass.width * pngp->pass.height * sizeof(uint32_t));

      if (!pngp->data)
         return -1;

      pngp->ihdr        = *ihdr;
      pngp->ihdr.width  = pngp->pass.width;
      pngp->ihdr.height = pngp->pass.height;

      png_pass_geom(&pngp->ihdr, pngp->pass.width,
            pngp->pass.height, NULL, NULL, &pngp->pass.size);

      if (pngp->pass.size > pngp->stream.total_out)
      {
         free(pngp->data);
         return -1;
      }

      pngp->adam7_pass_initialized = true;

      return 0;
   }

   if (pngp->pass_initialized)
      return 0;

   png_pass_geom(ihdr, ihdr->width, ihdr->height, &pngp->bpp, &pngp->pitch, &pass_size);

   if (pngp->stream.total_out < pass_size)
      return -1;

   pngp->restore_buf_size = 0;
   pngp->prev_scanline    = (uint8_t*)calloc(1, pngp->pitch);
   pngp->decoded_scanline = (uint8_t*)calloc(1, pngp->pitch);

   if (!pngp->prev_scanline || !pngp->decoded_scanline)
      goto error;

   pngp->h = 0;
   pngp->pass_initialized = true;

   return 0;

error:
   png_reverse_filter_deinit(pngp);
   return -1;
}

static int png_reverse_filter_copy_line(uint32_t *data, const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp, unsigned filter)
{
   unsigned i;

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
         return -1;
   }

   switch (ihdr->color_type)
   {
      case 0:
         png_reverse_filter_copy_line_bw(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
      case 2:
         png_reverse_filter_copy_line_rgb(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
      case 3:
         png_reverse_filter_copy_line_plt(data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth, pngp->palette);
         break;
      case 4:
         png_reverse_filter_copy_line_gray_alpha(data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth);
         break;
      case 6:
         png_reverse_filter_copy_line_rgba(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
   }

   memcpy(pngp->prev_scanline, pngp->decoded_scanline, pngp->pitch);

   return 0;
}

static bool png_reverse_filter_iterate(uint32_t *data, const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp)
{
   int ret = 1;

   if (pngp->h < ihdr->height)
   {
      unsigned filter = *pngp->inflate_buf++;
      pngp->restore_buf_size += 1;
      ret = png_reverse_filter_copy_line(data,
            ihdr, pngp, filter);
   }

   if (ret == 1 || ret == -1)
   {
      png_reverse_filter_deinit(pngp);
      return ret;
   }

   pngp->h++;
   pngp->inflate_buf      += pngp->pitch;
   pngp->restore_buf_size += pngp->pitch;

   return 0;
}

static bool png_reverse_filter_regular(uint32_t *data, const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp)
{
   int ret;

   do{
      ret = png_reverse_filter_iterate(data, ihdr, pngp);

      if (ret != 0)
         break;
      data += ihdr->width;
   }while(1);

   pngp->inflate_buf -= pngp->restore_buf_size;

   if (ret == 1)
      return true;
   return false;
}

static int png_reverse_filter_adam7(uint32_t *data,
      const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp)
{
   int ret = 0;
   bool to_cont = pngp->pass.pos < ARRAY_SIZE(passes);

   if (!to_cont)
   {
      ret = 1;
      goto end;
   }

   ret = png_reverse_filter_init(ihdr, pngp);

   if (ret == 1)
      goto cont;
   if (ret == -1)
   {
      ret = -1;
      goto end;
   }

   if (png_reverse_filter_init(&pngp->ihdr, pngp) == -1)
      goto error;

   if (!png_reverse_filter_regular(pngp->data,
            &pngp->ihdr, pngp))
      goto error;

   pngp->inflate_buf            += pngp->pass.size;
   pngp->adam7_restore_buf_size += pngp->pass.size;
   pngp->stream.total_out       -= pngp->pass.size;

   png_reverse_filter_adam7_deinterlace_pass(data,
         ihdr, pngp->data, pngp->pass.width, pngp->pass.height, &passes[pngp->pass.pos]);

   free(pngp->data);

   pngp->pass.width  = 0;
   pngp->pass.height = 0;
   pngp->pass.size   = 0;
   pngp->adam7_pass_initialized = false;

cont:
   pngp->pass.pos++;
   return 0;

end:
   pngp->inflate_buf -= pngp->adam7_restore_buf_size;
   pngp->adam7_restore_buf_size = 0;

   return ret;

error:
   if (pngp->data)
      free(pngp->data);
   pngp->inflate_buf -= pngp->adam7_restore_buf_size;
   pngp->adam7_restore_buf_size = 0;
   return -1;
}

static bool png_reverse_filter_loop(struct rpng_t *rpng,
      uint32_t **data)
{
   const struct png_ihdr *ihdr = NULL;
   struct rpng_process_t *pngp = NULL;
   rpng->process.adam7_restore_buf_size = 0;
   rpng->process.restore_buf_size = 0;
   rpng->process.palette = rpng->palette;

   ihdr = &rpng->ihdr;
   pngp = &rpng->process;

   if (rpng->ihdr.interlace == 1)
   {
      int ret = 0;

      do
      {
         ret = png_reverse_filter_adam7(*data,
               ihdr, pngp);
      }while(ret == 0);

      if (ret == -1)
         return false;
   }
   else
   {
      if (png_reverse_filter_init(ihdr, pngp) == -1)
         return false;
      if (!png_reverse_filter_regular(*data,
               ihdr, pngp))
         return false;
   }

   return true;
}

#endif
