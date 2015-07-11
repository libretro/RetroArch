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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpng_common.h"
#include "rpng_decode.h"

static bool read_chunk_header(uint8_t *buf, struct png_chunk *chunk)
{
   unsigned i;
   uint8_t dword[4] = {0};

   for (i = 0; i < 4; i++)
      dword[i] = buf[i];

   buf += 4;

   chunk->size = dword_be(dword);

   for (i = 0; i < 4; i++)
      chunk->type[i] = buf[i];

   buf += 4;

   return true;
}

static bool png_parse_ihdr(uint8_t *buf,
      struct png_ihdr *ihdr)
{
   buf += 4 + 4;

   ihdr->width       = dword_be(buf + 0);
   ihdr->height      = dword_be(buf + 4);
   ihdr->depth       = buf[8];
   ihdr->color_type  = buf[9];
   ihdr->compression = buf[10];
   ihdr->filter      = buf[11];
   ihdr->interlace   = buf[12];

   if (ihdr->width == 0 || ihdr->height == 0)
      return false;

   return true;
}

static bool png_realloc_idat(const struct png_chunk *chunk, struct idat_buffer *buf)
{
   uint8_t *new_buffer = (uint8_t*)realloc(buf->data, buf->size + chunk->size);

   if (!new_buffer)
      return false;

   buf->data  = new_buffer;
   return true;
}

static bool png_read_plte_into_buf(uint8_t *buf, 
      uint32_t *buffer, unsigned entries)
{
   unsigned i;

   if (entries > 256)
      return false;

   buf += 8;

   for (i = 0; i < entries; i++)
   {
      uint32_t r = buf[3 * i + 0];
      uint32_t g = buf[3 * i + 1];
      uint32_t b = buf[3 * i + 2];
      buffer[i] = (r << 16) | (g << 8) | (b << 0) | (0xffu << 24);
   }

   return true;
}

bool rpng_nbio_load_image_argb_iterate(uint8_t *buf, struct rpng_t *rpng, unsigned *ret)
{
   unsigned i;

   struct png_chunk chunk = {0};

   if (!read_chunk_header(buf, &chunk))
      return false;

#if 0
   for (i = 0; i < 4; i++)
   {
      fprintf(stderr, "chunktype: %c\n", chunk.type[i]);
   }
#endif

   switch (png_chunk_type(&chunk))
   {
      case PNG_CHUNK_NOOP:
      default:
         break;

      case PNG_CHUNK_ERROR:
         goto error;

      case PNG_CHUNK_IHDR:
         if (rpng->has_ihdr || rpng->has_idat || rpng->has_iend)
            goto error;

         if (chunk.size != 13)
            goto error;

         if (!png_parse_ihdr(buf, &rpng->ihdr))
            goto error;

         if (!png_process_ihdr(&rpng->ihdr))
            goto error;

         rpng->has_ihdr = true;
         break;

      case PNG_CHUNK_PLTE:
         {
            unsigned entries = chunk.size / 3;

            if (!rpng->has_ihdr || rpng->has_plte || rpng->has_iend || rpng->has_idat)
               goto error;

            if (chunk.size % 3)
               goto error;

            if (!png_read_plte_into_buf(buf, rpng->palette, entries))
               goto error;

            rpng->has_plte = true;
         }
         break;

      case PNG_CHUNK_IDAT:
         if (!(rpng->has_ihdr) || rpng->has_iend || (rpng->ihdr.color_type == PNG_IHDR_COLOR_PLT && !(rpng->has_plte)))
            goto error;

         if (!png_realloc_idat(&chunk, &rpng->idat_buf))
            goto error;

         buf += 8;

         for (i = 0; i < chunk.size; i++)
            rpng->idat_buf.data[i + rpng->idat_buf.size] = buf[i];

         rpng->idat_buf.size += chunk.size;

         rpng->has_idat = true;
         break;

      case PNG_CHUNK_IEND:
         if (!(rpng->has_ihdr) || !(rpng->has_idat))
            goto error;

         rpng->has_iend = true;
         goto error;
   }

   *ret = 4 + 4 + chunk.size + 4;

   return true;

error:
   return false;
}

int rpng_nbio_load_image_argb_process(struct rpng_t *rpng,
      uint32_t **data, unsigned *width, unsigned *height)
{
   if (!rpng->process.initialized)
   {
      if (!rpng_load_image_argb_process_init(rpng, data, width,
               height))
         return PNG_PROCESS_ERROR;
      return 0;
   }

   if (!rpng->process.inflate_initialized)
   {
      int ret = rpng_load_image_argb_process_inflate_init(rpng, data,
               width, height);
      if (ret == -1)
         return PNG_PROCESS_ERROR;
      return 0;
   }

   return png_reverse_filter_iterate(rpng, data);
}

void rpng_nbio_load_image_free(struct rpng_t *rpng)
{
   if (!rpng)
      return;

   if (rpng->idat_buf.data)
      free(rpng->idat_buf.data);
   if (rpng->process.inflate_buf)
      free(rpng->process.inflate_buf);
   if (rpng->process.stream)
   {
      zlib_stream_free(rpng->process.stream);
      free(rpng->process.stream);
   }

   free(rpng);
}

bool rpng_nbio_load_image_argb_start(struct rpng_t *rpng)
{
   unsigned i;
   char header[8] = {0};

   if (!rpng)
      return false;
   
   for (i = 0; i < 8; i++)
      header[i] = rpng->buff_data[i];

   if (memcmp(header, png_magic, sizeof(png_magic)) != 0)
      return false;

   rpng->buff_data += 8;

   return true;
}
