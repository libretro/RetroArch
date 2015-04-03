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

#include <file/file_extract.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpng_common.h"
#include "rpng_decode.h"

#ifdef GEKKO
#include <malloc.h>
#endif

enum png_chunk_type png_chunk_type(const struct png_chunk *chunk)
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

bool png_process_ihdr(struct png_ihdr *ihdr)
{
   unsigned i;
   bool ret = true;

   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_RGB:
      case PNG_IHDR_COLOR_GRAY_ALPHA:
      case PNG_IHDR_COLOR_RGBA:
         if (ihdr->depth != 8 && ihdr->depth != 16)
            GOTO_END_ERROR();
         break;
      case PNG_IHDR_COLOR_GRAY:
         {
            static const unsigned valid_bpp[] = { 1, 2, 4, 8, 16 };
            bool correct_bpp = false;

            for (i = 0; i < ARRAY_SIZE(valid_bpp); i++)
            {
               if (valid_bpp[i] == ihdr->depth)
               {
                  correct_bpp = true;
                  break;
               }
            }

            if (!correct_bpp)
               GOTO_END_ERROR();
         }
         break;
      case PNG_IHDR_COLOR_PLT:
         {
            static const unsigned valid_bpp[] = { 1, 2, 4, 8 };
            bool correct_bpp = false;

            for (i = 0; i < ARRAY_SIZE(valid_bpp); i++)
            {
               if (valid_bpp[i] == ihdr->depth)
               {
                  correct_bpp = true;
                  break;
               }
            }

            if (!correct_bpp)
               GOTO_END_ERROR();
         }
         break;
      default:
         GOTO_END_ERROR();
   }

#ifdef RPNG_TEST
   fprintf(stderr, "IHDR: (%u x %u), bpc = %u, palette = %s, color = %s, alpha = %s, adam7 = %s.\n",
         ihdr->width, ihdr->height,
         ihdr->depth, ihdr->color_type == PNG_IHDR_COLOR_PLT ? "yes" : "no",
         ihdr->color_type & PNG_IHDR_COLOR_RGB ? "yes" : "no",
         ihdr->color_type & PNG_IHDR_COLOR_GRAY_ALPHA ? "yes" : "no",
         ihdr->interlace == 1 ? "yes" : "no");
#endif

   if (ihdr->compression != 0)
      GOTO_END_ERROR();

end:
   return ret;
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
      case PNG_IHDR_COLOR_GRAY:
         bpp   = (ihdr->depth + 7) / 8;
         pitch = (ihdr->width * ihdr->depth + 7) / 8;
         break;
      case PNG_IHDR_COLOR_RGB:
         bpp   = (ihdr->depth * 3 + 7) / 8;
         pitch = (ihdr->width * ihdr->depth * 3 + 7) / 8;
         break;
      case PNG_IHDR_COLOR_PLT:
         bpp   = (ihdr->depth + 7) / 8;
         pitch = (ihdr->width * ihdr->depth + 7) / 8;
         break;
      case PNG_IHDR_COLOR_GRAY_ALPHA:
         bpp   = (ihdr->depth * 2 + 7) / 8;
         pitch = (ihdr->width * ihdr->depth * 2 + 7) / 8;
         break;
      case PNG_IHDR_COLOR_RGBA:
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

      if (pngp->pass.size > zlib_stream_get_total_out(pngp->stream))
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

   if (zlib_stream_get_total_out(pngp->stream) < pass_size)
      return -1;

   pngp->restore_buf_size      = 0;
   pngp->data_restore_buf_size = 0;
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
      case PNG_FILTER_NONE:
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         break;
      case PNG_FILTER_SUB:
         for (i = 0; i < pngp->bpp; i++)
            pngp->decoded_scanline[i] = pngp->inflate_buf[i];
         for (i = pngp->bpp; i < pngp->pitch; i++)
            pngp->decoded_scanline[i] = pngp->decoded_scanline[i - pngp->bpp] + pngp->inflate_buf[i];
         break;
      case PNG_FILTER_UP:
         for (i = 0; i < pngp->pitch; i++)
            pngp->decoded_scanline[i] = pngp->prev_scanline[i] + pngp->inflate_buf[i];
         break;
      case PNG_FILTER_AVERAGE:
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
      case PNG_FILTER_PAETH:
         for (i = 0; i < pngp->bpp; i++)
            pngp->decoded_scanline[i] = paeth(0, pngp->prev_scanline[i], 0) + pngp->inflate_buf[i];
         for (i = pngp->bpp; i < pngp->pitch; i++)
            pngp->decoded_scanline[i] = paeth(pngp->decoded_scanline[i - pngp->bpp],
                  pngp->prev_scanline[i], pngp->prev_scanline[i - pngp->bpp]) + pngp->inflate_buf[i];
         break;

      default:
         return PNG_PROCESS_ERROR_END;
   }

   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_GRAY:
         png_reverse_filter_copy_line_bw(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
      case PNG_IHDR_COLOR_RGB:
         png_reverse_filter_copy_line_rgb(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
      case PNG_IHDR_COLOR_PLT:
         png_reverse_filter_copy_line_plt(data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth, pngp->palette);
         break;
      case PNG_IHDR_COLOR_GRAY_ALPHA:
         png_reverse_filter_copy_line_gray_alpha(data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth);
         break;
      case PNG_IHDR_COLOR_RGBA:
         png_reverse_filter_copy_line_rgba(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
   }

   memcpy(pngp->prev_scanline, pngp->decoded_scanline, pngp->pitch);

   return PNG_PROCESS_NEXT;
}

static int png_reverse_filter_regular_iterate(uint32_t **data, const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp)
{
   int ret = PNG_PROCESS_END;

   if (pngp->h < ihdr->height)
   {
      unsigned filter = *pngp->inflate_buf++;
      pngp->restore_buf_size += 1;
      ret = png_reverse_filter_copy_line(*data,
            ihdr, pngp, filter);
   }

   if (ret == PNG_PROCESS_END || ret == PNG_PROCESS_ERROR_END)
      goto end;

   pngp->h++;
   pngp->inflate_buf           += pngp->pitch;
   pngp->restore_buf_size      += pngp->pitch;

   *data                       += ihdr->width;
   pngp->data_restore_buf_size += ihdr->width;

   return PNG_PROCESS_NEXT;

end:
   png_reverse_filter_deinit(pngp);

   pngp->inflate_buf -= pngp->restore_buf_size;
   *data             -= pngp->data_restore_buf_size;
   pngp->data_restore_buf_size = 0;
   return ret;
}

static int png_reverse_filter_adam7_iterate(uint32_t **data_,
      const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp)
{
   int ret = 0;
   bool to_next = pngp->pass.pos < ARRAY_SIZE(passes);
   uint32_t *data = *data_;

   if (!to_next)
      return PNG_PROCESS_END;

   ret = png_reverse_filter_init(ihdr, pngp);

   if (ret == 1)
      return PNG_PROCESS_NEXT;
   if (ret == -1)
      return PNG_PROCESS_ERROR_END;

   if (png_reverse_filter_init(&pngp->ihdr, pngp) == -1)
      return PNG_PROCESS_ERROR;

   do{
      ret = png_reverse_filter_regular_iterate(&pngp->data,
            &pngp->ihdr, pngp);
   }while(ret == PNG_PROCESS_NEXT);

   if (ret == PNG_PROCESS_ERROR || ret == PNG_PROCESS_ERROR_END)
      return PNG_PROCESS_ERROR;

   pngp->inflate_buf            += pngp->pass.size;
   pngp->adam7_restore_buf_size += pngp->pass.size;

   zlib_stream_decrement_total_out(pngp->stream, pngp->pass.size);

   png_reverse_filter_adam7_deinterlace_pass(data,
         ihdr, pngp->data, pngp->pass.width, pngp->pass.height, &passes[pngp->pass.pos]);

   free(pngp->data);

   pngp->pass.width  = 0;
   pngp->pass.height = 0;
   pngp->pass.size   = 0;
   pngp->adam7_pass_initialized = false;

   return PNG_PROCESS_NEXT;
}

static int png_reverse_filter_adam7(uint32_t **data_,
      const struct png_ihdr *ihdr,
      struct rpng_process_t *pngp)
{
   int ret = png_reverse_filter_adam7_iterate(data_,
         ihdr, pngp);

   switch (ret)
   {
      case PNG_PROCESS_ERROR_END:
      case PNG_PROCESS_END:
         break;
      case PNG_PROCESS_NEXT:
         pngp->pass.pos++;
         return 0;
      case PNG_PROCESS_ERROR:
         if (pngp->data)
            free(pngp->data);
         pngp->inflate_buf -= pngp->adam7_restore_buf_size;
         pngp->adam7_restore_buf_size = 0;
         return -1;
   }

   pngp->inflate_buf -= pngp->adam7_restore_buf_size;
   pngp->adam7_restore_buf_size = 0;
   return ret;
}

int png_reverse_filter_iterate(struct rpng_t *rpng,
      uint32_t **data)
{
   if (!rpng)
      return false;

   if (rpng->ihdr.interlace)
      return png_reverse_filter_adam7(data, &rpng->ihdr, &rpng->process);

   return png_reverse_filter_regular_iterate(data, &rpng->ihdr, &rpng->process);
}

int rpng_load_image_argb_process_inflate_init(struct rpng_t *rpng,
      uint32_t **data, unsigned *width, unsigned *height)
{
   int zstatus;
   bool to_continue = (zlib_stream_get_avail_in(rpng->process.stream) > 0
         && zlib_stream_get_avail_out(rpng->process.stream) > 0);

   if (!to_continue)
      goto end;

   zstatus = zlib_inflate_data_to_file_iterate(rpng->process.stream);

   switch (zstatus)
   {
      case 1:
         goto end;
      case -1:
         goto error;
      default:
         break;
   }

   return 0;

end:
   zlib_stream_free(rpng->process.stream);

   *width  = rpng->ihdr.width;
   *height = rpng->ihdr.height;
#ifdef GEKKO
   /* we often use these in textures, make sure they're 32-byte aligned */
   *data = (uint32_t*)memalign(32, rpng->ihdr.width * 
         rpng->ihdr.height * sizeof(uint32_t));
#else
   *data = (uint32_t*)malloc(rpng->ihdr.width * 
         rpng->ihdr.height * sizeof(uint32_t));
#endif
   if (!*data)
      goto false_end;

   rpng->process.adam7_restore_buf_size = 0;
   rpng->process.restore_buf_size = 0;
   rpng->process.palette = rpng->palette;

   if (rpng->ihdr.interlace != 1)
      if (png_reverse_filter_init(&rpng->ihdr, &rpng->process) == -1)
         goto false_end;

   rpng->process.inflate_initialized = true;
   return 1;

error:
   zlib_stream_free(rpng->process.stream);

false_end:
   rpng->process.inflate_initialized = false;
   return -1;
}

bool rpng_load_image_argb_process_init(struct rpng_t *rpng,
      uint32_t **data, unsigned *width, unsigned *height)
{
   rpng->process.inflate_buf_size = 0;
   rpng->process.inflate_buf      = NULL;

   png_pass_geom(&rpng->ihdr, rpng->ihdr.width,
         rpng->ihdr.height, NULL, NULL, &rpng->process.inflate_buf_size);
   if (rpng->ihdr.interlace == 1) /* To be sure. */
      rpng->process.inflate_buf_size *= 2;

   rpng->process.stream = zlib_stream_new();

   if (!rpng->process.stream)
      return false;

   if (!zlib_inflate_init(rpng->process.stream))
      return false;

   rpng->process.inflate_buf = (uint8_t*)malloc(rpng->process.inflate_buf_size);
   if (!rpng->process.inflate_buf)
      return false;

   zlib_set_stream(
         rpng->process.stream,
         rpng->idat_buf.size,
         rpng->process.inflate_buf_size,
         rpng->idat_buf.data,
         rpng->process.inflate_buf);

   rpng->process.initialized = true;

   return true;
}
