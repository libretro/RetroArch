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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>

#include "rpng_common.h"
#include "rpng_decode.h"

static bool read_chunk_header_fio(FILE **fd, struct png_chunk *chunk)
{
   uint8_t dword[4] = {0};
   FILE *file = *fd;

   if (fread(dword, 1, 4, file) != 4)
      return false;

   chunk->size = dword_be(dword);

   if (fread(chunk->type, 1, 4, file) != 4)
      return false;

   return true;
}

static bool png_read_chunk(FILE **fd, struct png_chunk *chunk)
{
   FILE *file = *fd;
   free(chunk->data);
   chunk->data = (uint8_t*)calloc(1, chunk->size + sizeof(uint32_t)); /* CRC32 */
   if (!chunk->data)
      return false;

   if (fread(chunk->data, 1, chunk->size + 
            sizeof(uint32_t), file) != (chunk->size + sizeof(uint32_t)))
   {
      free(chunk->data);
      return false;
   }

   /* Ignore CRC. */
   return true;
}

static void png_free_chunk(struct png_chunk *chunk)
{
   if (!chunk)
      return;

   free(chunk->data);
   chunk->data = NULL;
}

static bool png_parse_ihdr_fio(FILE **fd,
      struct png_chunk *chunk, struct png_ihdr *ihdr)
{
   if (!png_read_chunk(fd, chunk))
      return false;

   if (chunk->size != 13)
      return false;

   ihdr->width       = dword_be(chunk->data + 0);
   ihdr->height      = dword_be(chunk->data + 4);
   ihdr->depth       = chunk->data[8];
   ihdr->color_type  = chunk->data[9];
   ihdr->compression = chunk->data[10];
   ihdr->filter      = chunk->data[11];
   ihdr->interlace   = chunk->data[12];

   if (ihdr->width == 0 || ihdr->height == 0)
      return false;

   return true;
}

static bool png_append_idat_fio(FILE **fd,
      const struct png_chunk *chunk, struct idat_buffer *buf)
{
   FILE *file = *fd;
   uint8_t *new_buffer = (uint8_t*)realloc(buf->data, buf->size + chunk->size);

   if (!new_buffer)
      return false;

   buf->data  = new_buffer;
   if (fread(buf->data + buf->size, 1, chunk->size, file) != chunk->size)
      return false;
   if (fseek(file, sizeof(uint32_t), SEEK_CUR) < 0)
      return false;
   buf->size += chunk->size;
   return true;
}

static bool png_read_plte_fio(FILE **fd, uint32_t *buffer, unsigned entries)
{
   unsigned i;
   uint8_t buf[256 * 3];
   FILE *file = *fd;

   if (entries > 256)
      return false;

   if (fread(buf, 3, entries, file) != entries)
      return false;

   for (i = 0; i < entries; i++)
   {
      uint32_t r = buf[3 * i + 0];
      uint32_t g = buf[3 * i + 1];
      uint32_t b = buf[3 * i + 2];
      buffer[i] = (r << 16) | (g << 8) | (b << 0) | (0xffu << 24);
   }

   if (fseek(file, sizeof(uint32_t), SEEK_CUR) < 0)
      return false;

   return true;
}

bool rpng_load_image_argb_iterate(FILE **fd, struct rpng_t *rpng)
{
   struct png_chunk chunk = {0};
   FILE *file = *fd;

   if (!read_chunk_header_fio(fd, &chunk))
      return false;

   switch (png_chunk_type(&chunk))
   {
      case PNG_CHUNK_NOOP:
      default:
         if (fseek(file, chunk.size + sizeof(uint32_t), SEEK_CUR) < 0)
            return false;
         break;

      case PNG_CHUNK_ERROR:
         return false;

      case PNG_CHUNK_IHDR:
         if (rpng->has_ihdr || rpng->has_idat || rpng->has_iend)
            return false;

         if (!png_parse_ihdr_fio(fd, &chunk, &rpng->ihdr))
         {
            png_free_chunk(&chunk);
            return false;
         }

         if (!png_process_ihdr(&rpng->ihdr))
         {
            png_free_chunk(&chunk);
            return false;
         }

         png_free_chunk(&chunk);
         rpng->has_ihdr = true;
         break;

      case PNG_CHUNK_PLTE:
         if (!rpng->has_ihdr || rpng->has_plte || rpng->has_iend || rpng->has_idat)
            return false;

         if (chunk.size % 3)
            return false;

         if (!png_read_plte_fio(fd, rpng->palette, chunk.size / 3))
            return false;

         rpng->has_plte = true;
         break;

      case PNG_CHUNK_IDAT:
         if (!rpng->has_ihdr || rpng->has_iend || (rpng->ihdr.color_type == PNG_IHDR_COLOR_PLT && !rpng->has_plte))
            return false;

         if (!png_append_idat_fio(fd, &chunk, &rpng->idat_buf))
            return false;

         rpng->has_idat = true;
         break;

      case PNG_CHUNK_IEND:
         if (!rpng->has_ihdr || !rpng->has_idat)
            return false;

         if (fseek(file, sizeof(uint32_t), SEEK_CUR) < 0)
            return false;

         rpng->has_iend = true;
         break;
   }

   return true;
}

bool rpng_load_image_argb(const char *path, uint32_t **data,
      unsigned *width, unsigned *height)
{
   long pos, file_len;
   FILE *file;
   char header[8]     = {0};
   struct rpng_t rpng = {{0}};
   bool ret           = true;
   int retval         = 0;

   *data   = NULL;
   *width  = 0;
   *height = 0;

   file = fopen(path, "rb");
   if (!file)
      return false;

   fseek(file, 0, SEEK_END);
   file_len = ftell(file);
   rewind(file);

   if (fread(header, 1, sizeof(header), file) != sizeof(header))
      GOTO_END_ERROR();

   if (memcmp(header, png_magic, sizeof(png_magic)) != 0)
      GOTO_END_ERROR();

   /* feof() apparently isn't triggered after a seek (IEND). */
   for (pos = ftell(file); 
         pos < file_len && pos >= 0; pos = ftell(file))
   {
      if (!rpng_load_image_argb_iterate(&file, &rpng))
         GOTO_END_ERROR();
   }

   if (!rpng.has_ihdr || !rpng.has_idat || !rpng.has_iend)
      GOTO_END_ERROR();

   if (!rpng_load_image_argb_process_init(&rpng, data, width,
            height))
      GOTO_END_ERROR();

   do{
      retval = rpng_load_image_argb_process_inflate_init(&rpng, data,
               width, height);
   }while(retval == 0);

   if (retval == -1)
      GOTO_END_ERROR();

   do{
      retval = png_reverse_filter_iterate(&rpng, data);
   }while(retval == PNG_PROCESS_NEXT);

   if (retval == PNG_PROCESS_ERROR || retval == PNG_PROCESS_ERROR_END)
      GOTO_END_ERROR();

end:
   if (file)
      fclose(file);
   if (!ret)
      free(*data);
   free(rpng.idat_buf.data);
   free(rpng.process.inflate_buf);

   if (rpng.process.stream)
   {
      zlib_stream_free(rpng.process.stream);
      free(rpng.process.stream);
   }
   return ret;
}
