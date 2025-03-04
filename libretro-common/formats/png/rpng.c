/* Copyright  (C) 2010-2020 The RetroArch team
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

#ifdef DEBUG
#include <stdio.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef GEKKO
#include <malloc.h>
#endif

#include <boolean.h>
#include <formats/image.h>
#include <formats/rpng.h>
#include <streams/trans_stream.h>
#include <string/stdstring.h>

#include "rpng_internal.h"

enum png_ihdr_color_type
{
   PNG_IHDR_COLOR_GRAY       = 0,
   PNG_IHDR_COLOR_RGB        = 2,
   PNG_IHDR_COLOR_PLT        = 3,
   PNG_IHDR_COLOR_GRAY_ALPHA = 4,
   PNG_IHDR_COLOR_RGBA       = 6
};

enum png_line_filter
{
   PNG_FILTER_NONE = 0,
   PNG_FILTER_SUB,
   PNG_FILTER_UP,
   PNG_FILTER_AVERAGE,
   PNG_FILTER_PAETH
};

enum png_chunk_type
{
   PNG_CHUNK_NOOP = 0,
   PNG_CHUNK_ERROR,
   PNG_CHUNK_IHDR,
   PNG_CHUNK_IDAT,
   PNG_CHUNK_PLTE,
   PNG_CHUNK_tRNS,
   PNG_CHUNK_IEND
};

struct adam7_pass
{
   unsigned x;
   unsigned y;
   unsigned stride_x;
   unsigned stride_y;
};

struct idat_buffer
{
   uint8_t *data;
   size_t size;
};

enum rpng_process_flags
{
   RPNG_PROCESS_FLAG_INFLATE_INITIALIZED    = (1 << 0),
   RPNG_PROCESS_FLAG_ADAM7_PASS_INITIALIZED = (1 << 1),
   RPNG_PROCESS_FLAG_PASS_INITIALIZED       = (1 << 2)
};

struct rpng_process
{
   uint32_t *data;
   uint32_t *palette;
   void *stream;
   const struct trans_stream_backend *stream_backend;
   uint8_t *prev_scanline;
   uint8_t *decoded_scanline;
   uint8_t *inflate_buf;
   size_t restore_buf_size;
   size_t adam7_restore_buf_size;
   size_t data_restore_buf_size;
   size_t inflate_buf_size;
   size_t avail_in;
   size_t avail_out;
   size_t total_out;
   size_t pass_size;
   struct png_ihdr ihdr; /* uint32_t alignment */
   unsigned bpp;
   unsigned pitch;
   unsigned h;
   unsigned pass_width;
   unsigned pass_height;
   unsigned pass_pos;
   uint8_t flags;
};

enum rpng_flags
{
   RPNG_FLAG_HAS_IHDR = (1 << 0),
   RPNG_FLAG_HAS_IDAT = (1 << 1),
   RPNG_FLAG_HAS_IEND = (1 << 2),
   RPNG_FLAG_HAS_PLTE = (1 << 3),
   RPNG_FLAG_HAS_TRNS = (1 << 4)
};

struct rpng
{
   struct rpng_process *process;
   uint8_t *buff_data;
   uint8_t *buff_end;
   struct idat_buffer idat_buf; /* ptr alignment */
   struct png_ihdr ihdr; /* uint32 alignment */
   uint32_t palette[256];
   uint8_t flags;
};

static const struct adam7_pass rpng_passes[] = {
   { 0, 0, 8, 8 },
   { 4, 0, 8, 8 },
   { 0, 4, 4, 8 },
   { 2, 0, 4, 4 },
   { 0, 2, 2, 4 },
   { 1, 0, 2, 2 },
   { 0, 1, 1, 2 },
};

static INLINE uint32_t rpng_dword_be(const uint8_t *buf)
{
   return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] << 0);
}

#if defined(DEBUG) || defined(RPNG_TEST)
static bool rpng_process_ihdr(struct png_ihdr *ihdr)
{
   uint8_t ihdr_depth = ihdr->depth;

   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_RGB:
      case PNG_IHDR_COLOR_GRAY_ALPHA:
      case PNG_IHDR_COLOR_RGBA:
         if (ihdr_depth != 8 && ihdr_depth != 16)
         {
            fprintf(stderr, "[RPNG]: Error in line %d.\n", __LINE__);
            return false;
         }
         break;
      case PNG_IHDR_COLOR_GRAY:
         /* Valid bitdepths are: 1, 2, 4, 8, 16 */
         if (ihdr_depth > 16 || (0x977F7FFF << ihdr_depth) & 0x80000000)
         {
            fprintf(stderr, "[RPNG]: Error in line %d.\n", __LINE__);
            return false;
         }
         break;
      case PNG_IHDR_COLOR_PLT:
         /* Valid bitdepths are: 1, 2, 4, 8 */
         if (ihdr_depth > 8 || (0x977F7FFF << ihdr_depth)  & 0x80000000)
         {
            fprintf(stderr, "[RPNG]: Error in line %d.\n", __LINE__);
            return false;
         }
         break;
      default:
         fprintf(stderr, "[RPNG]: Error in line %d.\n", __LINE__);
         return false;
   }

#ifdef RPNG_TEST
   fprintf(stderr, "IHDR: (%u x %u), bpc = %u, palette = %s, color = %s, alpha = %s, adam7 = %s.\n",
         ihdr->width, ihdr->height,
         ihdr_depth, (ihdr->color_type == PNG_IHDR_COLOR_PLT) ? "yes" : "no",
         (ihdr->color_type & PNG_IHDR_COLOR_RGB)              ? "yes" : "no",
         (ihdr->color_type & PNG_IHDR_COLOR_GRAY_ALPHA)       ? "yes" : "no",
         ihdr->interlace == 1 ? "yes" : "no");
#endif

   return true;
}
#else
static bool rpng_process_ihdr(struct png_ihdr *ihdr)
{
   uint8_t ihdr_depth = ihdr->depth;

   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_RGB:
      case PNG_IHDR_COLOR_GRAY_ALPHA:
      case PNG_IHDR_COLOR_RGBA:
         if (ihdr_depth != 8 && ihdr_depth != 16)
            return false;
         break;
      case PNG_IHDR_COLOR_GRAY:
         /* Valid bitdepths are: 1, 2, 4, 8, 16 */
         if (ihdr_depth > 16 || (0x977F7FFF << ihdr_depth) & 0x80000000)
            return false;
         break;
      case PNG_IHDR_COLOR_PLT:
         /* Valid bitdepths are: 1, 2, 4, 8 */
         if (ihdr_depth > 8 || (0x977F7FFF << ihdr_depth)  & 0x80000000)
            return false;
         break;
      default:
         return false;
   }

   return true;
}
#endif

static void rpng_reverse_filter_copy_line_rgb(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned bpp)
{
   int i;

   bpp /= 8;

   for (i = 0; i < (int)width; i++)
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

static void rpng_reverse_filter_copy_line_rgba(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned bpp)
{
   int i;

   bpp /= 8;

   for (i = 0; i < (int)width; i++)
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

static void rpng_reverse_filter_copy_line_bw(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned depth)
{
   int i;
   unsigned bit;
   static const unsigned mul_table[] = { 0, 0xff, 0x55, 0, 0x11, 0, 0, 0, 0x01 };
   unsigned mul, mask;

   if (depth == 16)
   {
      for (i = 0; i < (int)width; i++)
      {
         uint32_t val = decoded[i << 1];
         data[i]      = (val * 0x010101) | (0xffu << 24);
      }
      return;
   }

   mul  = mul_table[depth];
   mask = (1 << depth) - 1;
   bit  = 0;

   for (i = 0; i < (int)width; i++, bit += depth)
   {
      unsigned byte = bit >> 3;
      unsigned val  = decoded[byte] >> (8 - depth - (bit & 7));

      val          &= mask;
      val          *= mul;
      data[i]       = (val * 0x010101) | (0xffu << 24);
   }
}

static void rpng_reverse_filter_copy_line_gray_alpha(uint32_t *data,
      const uint8_t *decoded, unsigned width,
      unsigned bpp)
{
   int i;

   bpp /= 8;

   for (i = 0; i < (int)width; i++)
   {
      uint32_t gray, alpha;

      gray     = *decoded;
      decoded += bpp;
      alpha    = *decoded;
      decoded += bpp;

      data[i]  = (gray * 0x010101) | (alpha << 24);
   }
}

static void rpng_reverse_filter_copy_line_plt(uint32_t *data,
      const uint8_t *decoded, unsigned width,
      unsigned depth, const uint32_t *palette)
{
   switch (depth)
   {
      case 1:
         {
            int i;
            unsigned w = width / 8;
            for (i = 0; i < (int)w; i++, decoded++)
            {
               *data++ = palette[(*decoded >> 7) & 1];
               *data++ = palette[(*decoded >> 6) & 1];
               *data++ = palette[(*decoded >> 5) & 1];
               *data++ = palette[(*decoded >> 4) & 1];
               *data++ = palette[(*decoded >> 3) & 1];
               *data++ = palette[(*decoded >> 2) & 1];
               *data++ = palette[(*decoded >> 1) & 1];
               *data++ = palette[*decoded & 1];
            }

            switch (width & 7)
            {
               case 7:
                  data[6] = palette[(*decoded >> 1) & 1];
               case 6:
                  data[5] = palette[(*decoded >> 2) & 1];
               case 5:
                  data[4] = palette[(*decoded >> 3) & 1];
               case 4:
                  data[3] = palette[(*decoded >> 4) & 1];
               case 3:
                  data[2] = palette[(*decoded >> 5) & 1];
               case 2:
                  data[1] = palette[(*decoded >> 6) & 1];
               case 1:
                  data[0] = palette[(*decoded >> 7) & 1];
                  break;
            }
         }
         break;

      case 2:
         {
            int i;
            unsigned w = width / 4;
            for (i = 0; i < (int)w; i++, decoded++)
            {
               *data++ = palette[(*decoded >> 6) & 3];
               *data++ = palette[(*decoded >> 4) & 3];
               *data++ = palette[(*decoded >> 2) & 3];
               *data++ = palette[*decoded & 3];
            }

            switch (width & 3)
            {
               case 3:
                  data[2] = palette[(*decoded >> 2) & 3];
               case 2:
                  data[1] = palette[(*decoded >> 4) & 3];
               case 1:
                  data[0] = palette[(*decoded >> 6) & 3];
                  break;
            }
         }
         break;

      case 4:
         {
            int i;
            unsigned w = width / 2;
            for (i = 0; i < (int)w; i++, decoded++)
            {
               *data++ = palette[*decoded >> 4];
               *data++ = palette[*decoded & 0x0f];
            }

            if (width & 1)
               *data = palette[*decoded >> 4];
         }
         break;

      case 8:
         {
            int i;
            for (i = 0; i < (int)width; i++, decoded++, data++)
               *data = palette[*decoded];
         }
         break;
   }
}

static void rpng_pass_geom(const struct png_ihdr *ihdr,
      unsigned width, unsigned height,
      unsigned *bpp_out, unsigned *pitch_out, size_t *pass_size)
{
   unsigned bpp   = 0;
   unsigned pitch = 0;

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
         break;
   }

   if (pass_size)
      *pass_size = (pitch + 1) * ihdr->height;
   if (bpp_out)
      *bpp_out   = bpp;
   if (pitch_out)
      *pitch_out = pitch;
}

static void rpng_reverse_filter_adam7_deinterlace_pass(uint32_t *data,
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

static void rpng_reverse_filter_deinit(struct rpng_process *pngp)
{
   if (!pngp)
      return;
   if (pngp->decoded_scanline)
      free(pngp->decoded_scanline);
   pngp->decoded_scanline = NULL;
   if (pngp->prev_scanline)
      free(pngp->prev_scanline);
   pngp->prev_scanline    = NULL;

   pngp->flags           &= ~RPNG_PROCESS_FLAG_PASS_INITIALIZED;
   pngp->h                = 0;
}

static int rpng_reverse_filter_init(const struct png_ihdr *ihdr,
      struct rpng_process *pngp)
{
   size_t pass_size;

   if (   !(pngp->flags & RPNG_PROCESS_FLAG_ADAM7_PASS_INITIALIZED)
         && ihdr->interlace)
   {
      if (     ihdr->width  <= rpng_passes[pngp->pass_pos].x
            || ihdr->height <= rpng_passes[pngp->pass_pos].y) /* Empty pass */
         return 1;

      pngp->pass_width  = (ihdr->width -
            rpng_passes[pngp->pass_pos].x + rpng_passes[pngp->pass_pos].stride_x
- 1) / rpng_passes[pngp->pass_pos].stride_x;
      pngp->pass_height = (ihdr->height - rpng_passes[pngp->pass_pos].y +
            rpng_passes[pngp->pass_pos].stride_y - 1) / rpng_passes[pngp->pass_pos].stride_y;

      if (!(pngp->data = (uint32_t*)malloc(
            pngp->pass_width * pngp->pass_height * sizeof(uint32_t))))
         return -1;

      pngp->ihdr        = *ihdr;
      pngp->ihdr.width  = pngp->pass_width;
      pngp->ihdr.height = pngp->pass_height;

      rpng_pass_geom(&pngp->ihdr, pngp->pass_width,
            pngp->pass_height, NULL, NULL, &pngp->pass_size);

      if (pngp->pass_size > pngp->total_out)
      {
         free(pngp->data);
         pngp->data = NULL;
         return -1;
      }

      pngp->flags |= RPNG_PROCESS_FLAG_ADAM7_PASS_INITIALIZED;

      return 0;
   }

   if (pngp->flags & RPNG_PROCESS_FLAG_PASS_INITIALIZED)
      return 0;

   rpng_pass_geom(ihdr, ihdr->width, ihdr->height, &pngp->bpp, &pngp->pitch, &pass_size);

   if (pngp->total_out < pass_size)
      return -1;

   pngp->restore_buf_size      = 0;
   pngp->data_restore_buf_size = 0;
   pngp->prev_scanline         = (uint8_t*)calloc(1, pngp->pitch);
   pngp->decoded_scanline      = (uint8_t*)calloc(1, pngp->pitch);

   if (!pngp->prev_scanline || !pngp->decoded_scanline)
      goto error;

   pngp->h                    = 0;
   pngp->flags               |= RPNG_PROCESS_FLAG_PASS_INITIALIZED;

   return 0;

error:
   rpng_reverse_filter_deinit(pngp);
   return -1;
}

static int rpng_reverse_filter_copy_line(uint32_t *data,
      const struct png_ihdr *ihdr,
      struct rpng_process *pngp, unsigned filter)
{
   unsigned i;

   switch (filter)
   {
      case PNG_FILTER_NONE:
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         break;
      case PNG_FILTER_SUB:
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         for (i = pngp->bpp; i < pngp->pitch; i++)
            pngp->decoded_scanline[i] += pngp->decoded_scanline[i - pngp->bpp];
         break;
      case PNG_FILTER_UP:
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         for (i = 0; i < pngp->pitch; i++)
            pngp->decoded_scanline[i] += pngp->prev_scanline[i];
         break;
      case PNG_FILTER_AVERAGE:
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         for (i = 0; i < pngp->bpp; i++)
         {
            uint8_t avg = pngp->prev_scanline[i] >> 1;
            pngp->decoded_scanline[i] += avg;
         }
         for (i = pngp->bpp; i < pngp->pitch; i++)
         {
            uint8_t avg = (pngp->decoded_scanline[i - pngp->bpp] + pngp->prev_scanline[i]) >> 1;
            pngp->decoded_scanline[i] += avg;
         }
         break;
      case PNG_FILTER_PAETH:
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         for (i = 0; i < pngp->bpp; i++)
            pngp->decoded_scanline[i] += pngp->prev_scanline[i];
         for (i = pngp->bpp; i < pngp->pitch; i++)
            pngp->decoded_scanline[i] += paeth(pngp->decoded_scanline[i - pngp->bpp],
                  pngp->prev_scanline[i], pngp->prev_scanline[i - pngp->bpp]);
         break;
      default:
         return IMAGE_PROCESS_ERROR_END;
   }

   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_GRAY:
         rpng_reverse_filter_copy_line_bw(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
      case PNG_IHDR_COLOR_RGB:
         rpng_reverse_filter_copy_line_rgb(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
      case PNG_IHDR_COLOR_PLT:
         rpng_reverse_filter_copy_line_plt(
               data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth, pngp->palette);
         break;
      case PNG_IHDR_COLOR_GRAY_ALPHA:
         rpng_reverse_filter_copy_line_gray_alpha(data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth);
         break;
      case PNG_IHDR_COLOR_RGBA:
         rpng_reverse_filter_copy_line_rgba(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
   }

   memcpy(pngp->prev_scanline, pngp->decoded_scanline, pngp->pitch);

   return IMAGE_PROCESS_NEXT;
}

static int rpng_reverse_filter_regular_iterate(
      uint32_t **data, const struct png_ihdr *ihdr,
      struct rpng_process *pngp)
{
   int ret = IMAGE_PROCESS_END;
   if (pngp->h < ihdr->height)
   {
      unsigned filter         = *pngp->inflate_buf++;
      pngp->restore_buf_size += 1;
      ret                     = rpng_reverse_filter_copy_line(*data,
            ihdr, pngp, filter);
      if (ret == IMAGE_PROCESS_END || ret == IMAGE_PROCESS_ERROR_END)
         goto end;
   }
   else
      goto end;

   pngp->h++;
   pngp->inflate_buf           += pngp->pitch;
   pngp->restore_buf_size      += pngp->pitch;

   *data                       += ihdr->width;
   pngp->data_restore_buf_size += ihdr->width;

   return IMAGE_PROCESS_NEXT;

end:
   rpng_reverse_filter_deinit(pngp);

   pngp->inflate_buf -= pngp->restore_buf_size;
   *data             -= pngp->data_restore_buf_size;
   pngp->data_restore_buf_size = 0;
   return ret;
}

static int rpng_reverse_filter_adam7_iterate(uint32_t **data_,
      const struct png_ihdr *ihdr,
      struct rpng_process *pngp)
{
   int        ret = 0;
   bool   to_next = pngp->pass_pos < ARRAY_SIZE(rpng_passes);
   uint32_t *data = *data_;

   if (!to_next)
      return IMAGE_PROCESS_END;

   if ((ret = rpng_reverse_filter_init(ihdr, pngp)) == 1)
      return IMAGE_PROCESS_NEXT;
   else if (ret == -1)
      return IMAGE_PROCESS_ERROR_END;

   if (rpng_reverse_filter_init(&pngp->ihdr, pngp) == -1)
      return IMAGE_PROCESS_ERROR;

   do
   {
      ret = rpng_reverse_filter_regular_iterate(&pngp->data,
            &pngp->ihdr, pngp);
   } while (ret == IMAGE_PROCESS_NEXT);

   if (ret == IMAGE_PROCESS_ERROR || ret == IMAGE_PROCESS_ERROR_END)
      return IMAGE_PROCESS_ERROR;

   pngp->inflate_buf            += pngp->pass_size;
   pngp->adam7_restore_buf_size += pngp->pass_size;

   pngp->total_out              -= pngp->pass_size;

   rpng_reverse_filter_adam7_deinterlace_pass(data,
         ihdr, pngp->data, pngp->pass_width, pngp->pass_height,
         &rpng_passes[pngp->pass_pos]);

   free(pngp->data);

   pngp->data                   = NULL;
   pngp->pass_width             = 0;
   pngp->pass_height            = 0;
   pngp->pass_size              = 0;
   pngp->flags                 &= ~RPNG_PROCESS_FLAG_ADAM7_PASS_INITIALIZED;

   return IMAGE_PROCESS_NEXT;
}

static int rpng_reverse_filter_adam7(uint32_t **data_,
      const struct png_ihdr *ihdr,
      struct rpng_process *pngp)
{
   int ret = rpng_reverse_filter_adam7_iterate(data_,
         ihdr, pngp);

   switch (ret)
   {
      case IMAGE_PROCESS_ERROR_END:
      case IMAGE_PROCESS_END:
         break;
      case IMAGE_PROCESS_NEXT:
         pngp->pass_pos++;
         return 0;
      case IMAGE_PROCESS_ERROR:
         if (pngp->data)
         {
            free(pngp->data);
            pngp->data = NULL;
         }
         pngp->inflate_buf -= pngp->adam7_restore_buf_size;
         pngp->adam7_restore_buf_size = 0;
         return -1;
   }

   pngp->inflate_buf            -= pngp->adam7_restore_buf_size;
   pngp->adam7_restore_buf_size  = 0;
   return ret;
}

static int rpng_load_image_argb_process_inflate_init(
      rpng_t *rpng, uint32_t **data)
{
   bool zstatus;
   enum trans_stream_error terror;
   uint32_t rd, wn;
   struct rpng_process *process = (struct rpng_process*)rpng->process;
   bool to_continue        = (process->avail_in > 0
         && process->avail_out > 0);

   if (!to_continue)
      goto end;

   zstatus = process->stream_backend->trans(process->stream, false, &rd, &wn, &terror);

   if (!zstatus && terror != TRANS_STREAM_ERROR_BUFFER_FULL)
      goto error;

   process->avail_in -= rd;
   process->avail_out -= wn;
   process->total_out += wn;

   if (terror)
      return 0;

end:
   process->stream_backend->stream_free(process->stream);
   process->stream = NULL;

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

   process->adam7_restore_buf_size = 0;
   process->restore_buf_size       = 0;
   process->palette                = rpng->palette;

   if (rpng->ihdr.interlace != 1)
      if (rpng_reverse_filter_init(&rpng->ihdr, process) == -1)
         goto false_end;

   process->flags              |=  RPNG_PROCESS_FLAG_INFLATE_INITIALIZED;
   return 1;

error:
false_end:
   process->flags              &= ~RPNG_PROCESS_FLAG_INFLATE_INITIALIZED;
   return -1;
}

static bool rpng_realloc_idat(struct idat_buffer *buf, uint32_t chunk_size)
{
   uint8_t *new_buffer = (uint8_t*)realloc(buf->data, buf->size + chunk_size);

   if (!new_buffer)
      return false;

   buf->data  = new_buffer;
   return true;
}

static struct rpng_process *rpng_process_init(rpng_t *rpng)
{
   uint8_t *inflate_buf            = NULL;
   struct rpng_process *process    = (struct rpng_process*)malloc(sizeof(*process));

   if (!process)
      return NULL;

   process->flags                  = 0;
   process->prev_scanline          = NULL;
   process->decoded_scanline       = NULL;
   process->inflate_buf            = NULL;

   process->ihdr.width             = 0;
   process->ihdr.height            = 0;
   process->ihdr.depth             = 0;
   process->ihdr.color_type        = 0;
   process->ihdr.compression       = 0;
   process->ihdr.filter            = 0;
   process->ihdr.interlace         = 0;

   process->restore_buf_size       = 0;
   process->adam7_restore_buf_size = 0;
   process->data_restore_buf_size  = 0;
   process->inflate_buf_size       = 0;
   process->avail_in               = 0;
   process->avail_out              = 0;
   process->total_out              = 0;
   process->pass_size              = 0;
   process->bpp                    = 0;
   process->pitch                  = 0;
   process->h                      = 0;
   process->pass_width             = 0;
   process->pass_height            = 0;
   process->pass_pos               = 0;
   process->data                   = 0;
   process->palette                = 0;
   process->stream                 = NULL;
   process->stream_backend         = trans_stream_get_zlib_inflate_backend();

   rpng_pass_geom(&rpng->ihdr, rpng->ihdr.width,
         rpng->ihdr.height, NULL, NULL, &process->inflate_buf_size);
   if (rpng->ihdr.interlace == 1) /* To be sure. */
      process->inflate_buf_size *= 2;

   process->stream = process->stream_backend->stream_new();

   if (!process->stream)
   {
      free(process);
      return NULL;
   }

   inflate_buf = (uint8_t*)malloc(process->inflate_buf_size);
   if (!inflate_buf)
      goto error;

   process->inflate_buf = inflate_buf;
   process->avail_in    = rpng->idat_buf.size;
   process->avail_out   = process->inflate_buf_size;

   process->stream_backend->set_in(
         process->stream,
         rpng->idat_buf.data,
         (uint32_t)rpng->idat_buf.size);
   process->stream_backend->set_out(
         process->stream,
         process->inflate_buf,
         (uint32_t)process->inflate_buf_size);

   return process;

error:
   if (process)
   {
      if (process->stream)
         process->stream_backend->stream_free(process->stream);
      free(process);
   }
   return NULL;
}

/**
 * rpng_read_chunk_header:
 *
 * Leaf function.
 *
 * @return The PNG type of the memory chunk (i.e. IHDR, IDAT, IEND,
   PLTE, and/or tRNS)
 **/
static enum png_chunk_type rpng_read_chunk_header(
      uint8_t *buf, uint32_t chunk_size)
{
   int i;
   char type[4];

   for (i = 0; i < 4; i++)
   {
      uint8_t byte = buf[i + 4];

      /* All four bytes of the chunk type must be
       * ASCII letters (codes 65-90 and 97-122) */
      if ((byte < 65) || ((byte > 90) && (byte < 97)) || (byte > 122))
         return PNG_CHUNK_ERROR;
      type[i]      = byte;
   }

   if (
            type[0] == 'I'
         && type[1] == 'H'
         && type[2] == 'D'
         && type[3] == 'R'
      )
      return PNG_CHUNK_IHDR;
   else if
      (
          type[0] == 'I'
       && type[1] == 'D'
       && type[2] == 'A'
       && type[3] == 'T'
      )
         return PNG_CHUNK_IDAT;
   else if
      (
          type[0] == 'I'
       && type[1] == 'E'
       && type[2] == 'N'
       && type[3] == 'D'
      )
         return PNG_CHUNK_IEND;
   else if
      (
          type[0] == 'P'
       && type[1] == 'L'
       && type[2] == 'T'
       && type[3] == 'E'
      )
         return PNG_CHUNK_PLTE;
   else if
      (
          type[0] == 't'
       && type[1] == 'R'
       && type[2] == 'N'
       && type[3] == 'S'
      )
         return PNG_CHUNK_tRNS;

   return PNG_CHUNK_NOOP;
}

bool rpng_iterate_image(rpng_t *rpng)
{
   unsigned i;
   uint8_t *buf             = (uint8_t*)rpng->buff_data;
   uint32_t chunk_size      = 0;

   /* Check whether data buffer pointer is valid */
   if (buf > rpng->buff_end)
      return false;

   /* Check whether reading the header will overflow
    * the data buffer */
   if (rpng->buff_end - buf < 8)
      return false;

   chunk_size = rpng_dword_be(buf);

   /* Check whether chunk will overflow the data buffer */
   if (buf + 8 + chunk_size > rpng->buff_end)
      return false;

   switch (rpng_read_chunk_header(buf, chunk_size))
   {
      case PNG_CHUNK_NOOP:
      default:
         break;

      case PNG_CHUNK_ERROR:
         return false;

      case PNG_CHUNK_IHDR:
         if (     (rpng->flags & RPNG_FLAG_HAS_IHDR)
               || (rpng->flags & RPNG_FLAG_HAS_IDAT)
               || (rpng->flags & RPNG_FLAG_HAS_IEND))
            return false;

         if (chunk_size != 13)
            return false;

         buf                    += 4 + 4;

         rpng->ihdr.width        = rpng_dword_be(buf + 0);
         rpng->ihdr.height       = rpng_dword_be(buf + 4);
         rpng->ihdr.depth        = buf[8];
         rpng->ihdr.color_type   = buf[9];
         rpng->ihdr.compression  = buf[10];
         rpng->ihdr.filter       = buf[11];
         rpng->ihdr.interlace    = buf[12];

         if (     rpng->ihdr.width  == 0
               || rpng->ihdr.height == 0
               /* ensure multiplications don't overflow and wrap around, that'd give buffer overflow crashes */
               || (uint64_t)rpng->ihdr.width*rpng->ihdr.height*sizeof(uint32_t) >= 0x80000000)
            return false;

         if (!rpng_process_ihdr(&rpng->ihdr))
            return false;

         if (rpng->ihdr.compression != 0)
         {
#if defined(DEBUG) || defined(RPNG_TEST)
            fprintf(stderr, "[RPNG]: Error in line %d.\n", __LINE__);
#endif
            return false;
         }

         rpng->flags   |= RPNG_FLAG_HAS_IHDR;
         break;

      case PNG_CHUNK_PLTE:
         {
            int i;
            unsigned entries = chunk_size / 3;

            if (entries > 256)
               return false;
            if (chunk_size % 3)
               return false;

            if (     !(rpng->flags & RPNG_FLAG_HAS_IHDR)
                  ||  (rpng->flags & RPNG_FLAG_HAS_PLTE)
                  ||  (rpng->flags & RPNG_FLAG_HAS_IEND)
                  ||  (rpng->flags & RPNG_FLAG_HAS_IDAT)
                  ||  (rpng->flags & RPNG_FLAG_HAS_TRNS))
               return false;

            buf += 8;

            for (i = 0; i < (int)entries; i++)
            {
               uint32_t r       = buf[3 * i + 0];
               uint32_t g       = buf[3 * i + 1];
               uint32_t b       = buf[3 * i + 2];
               rpng->palette[i] = (r << 16) | (g << 8) | (b << 0) | (0xffu << 24);
            }

            rpng->flags        |= RPNG_FLAG_HAS_PLTE;
         }
         break;

      case PNG_CHUNK_tRNS:
         if (rpng->flags & RPNG_FLAG_HAS_IDAT)
            return false;

         if (rpng->ihdr.color_type == PNG_IHDR_COLOR_PLT)
         {
            int i;
            uint32_t *palette;
            /* we should compare with the number of palette entries */
            if (chunk_size > 256)
               return false;

            buf    += 8;
            palette = rpng->palette;

            for (i = 0; i < (int)chunk_size; i++, buf++, palette++)
               *palette = (*palette & 0x00ffffff) | (unsigned)*buf << 24;
         }
         /* TODO: support colorkey in grayscale and truecolor images */

         rpng->flags         |= RPNG_FLAG_HAS_TRNS;
         break;

      case PNG_CHUNK_IDAT:
         if (     !(rpng->flags & RPNG_FLAG_HAS_IHDR)
               ||  (rpng->flags & RPNG_FLAG_HAS_IEND)
               ||  (rpng->ihdr.color_type == PNG_IHDR_COLOR_PLT
                  &&
                  !(rpng->flags & RPNG_FLAG_HAS_PLTE)))
            return false;

         if (!rpng_realloc_idat(&rpng->idat_buf, chunk_size))
            return false;

         buf += 8;

         for (i = 0; i < chunk_size; i++)
            rpng->idat_buf.data[i + rpng->idat_buf.size] = buf[i];

         rpng->idat_buf.size += chunk_size;

         rpng->flags         |= RPNG_FLAG_HAS_IDAT;
         break;

      case PNG_CHUNK_IEND:
         if (     !(rpng->flags & RPNG_FLAG_HAS_IHDR)
               || !(rpng->flags & RPNG_FLAG_HAS_IDAT))
            return false;

         rpng->flags         |= RPNG_FLAG_HAS_IEND;
         return false;
   }

   rpng->buff_data += chunk_size + 12;

   /* Check whether data buffer pointer is valid */
   if (rpng->buff_data > rpng->buff_end)
      return false;
   return true;
}

int rpng_process_image(rpng_t *rpng,
      void **_data, size_t len, unsigned *width, unsigned *height)
{
   uint32_t **data = (uint32_t**)_data;

   if (!rpng->process)
   {
      struct rpng_process *process = rpng_process_init(rpng);

      if (!process)
         goto error;

      rpng->process = process;
      return IMAGE_PROCESS_NEXT;
   }

   if (!(rpng->process->flags & RPNG_PROCESS_FLAG_INFLATE_INITIALIZED))
   {
      if (rpng_load_image_argb_process_inflate_init(rpng, data) == -1)
         goto error;
      return IMAGE_PROCESS_NEXT;
   }

   *width  = rpng->ihdr.width;
   *height = rpng->ihdr.height;

   if (rpng->ihdr.interlace && rpng->process)
      return rpng_reverse_filter_adam7(data, &rpng->ihdr, rpng->process);
   return rpng_reverse_filter_regular_iterate(data, &rpng->ihdr, rpng->process);

error:
   if (rpng->process)
   {
      if (rpng->process->inflate_buf)
         free(rpng->process->inflate_buf);
      if (rpng->process->stream)
         rpng->process->stream_backend->stream_free(rpng->process->stream);
      free(rpng->process);
      rpng->process = NULL;
   }
   return IMAGE_PROCESS_ERROR;
}

void rpng_free(rpng_t *rpng)
{
   if (!rpng)
      return;

   if (rpng->idat_buf.data)
      free(rpng->idat_buf.data);
   if (rpng->process)
   {
      if (rpng->process->inflate_buf)
         free(rpng->process->inflate_buf);
      if (rpng->process->stream)
      {
         if (rpng->process->stream_backend && rpng->process->stream_backend->stream_free)
            rpng->process->stream_backend->stream_free(rpng->process->stream);
         else
            free(rpng->process->stream);
      }
      free(rpng->process);
   }

   free(rpng);
}

bool rpng_start(rpng_t *rpng)
{
   if (!rpng)
      return false;

   /* Check whether reading the header will overflow
    * the data buffer */
   if (rpng->buff_end - rpng->buff_data < 8)
      return false;

   if (string_is_not_equal_fast(
            rpng->buff_data, png_magic, sizeof(png_magic)))
      return false;

   rpng->buff_data += 8;

   return true;
}

/**
 * rpng_is_valid:
 *
 * Check if @rpng is a valid PNG image.
 * Must contain an IHDR chunk, one or more IDAT
 * chunks, and an IEND chunk.
 *
 * Leaf function.
 *
 * @return true if it's a valid PNG image, otherwise false.
 **/
bool rpng_is_valid(rpng_t *rpng)
{
   return (rpng && ((rpng->flags & (RPNG_FLAG_HAS_IHDR | RPNG_FLAG_HAS_IDAT |
RPNG_FLAG_HAS_IEND)) > 0));
}

bool rpng_set_buf_ptr(rpng_t *rpng, void *data, size_t len)
{
   if (!rpng || (len < 1))
      return false;

   rpng->buff_data = (uint8_t*)data;
   rpng->buff_end  = rpng->buff_data + (len - 1);

   return true;
}

rpng_t *rpng_alloc(void)
{
   rpng_t *rpng = (rpng_t*)calloc(1, sizeof(*rpng));
   if (!rpng)
      return NULL;
   return rpng;
}
