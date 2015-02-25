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

#include <formats/rpng.h>

#include <zlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GEKKO
#include <malloc.h>
#endif

#include "rpng_common.h"
#include "rpng_decode_common.h"

static bool read_chunk_header_fio(FILE *file, struct png_chunk *chunk)
{
   uint8_t dword[4] = {0};

   if (fread(dword, 1, 4, file) != 4)
      return false;

   chunk->size = dword_be(dword);

   if (fread(chunk->type, 1, 4, file) != 4)
      return false;

   return true;
}

static bool png_read_chunk(FILE *file, struct png_chunk *chunk)
{
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

static bool png_parse_ihdr_fio(FILE *file,
      struct png_chunk *chunk, struct png_ihdr *ihdr)
{
   unsigned i;
   bool ret = true;

   if (!png_read_chunk(file, chunk))
      return false;

   if (chunk->size != 13)
      GOTO_END_ERROR();

   ihdr->width       = dword_be(chunk->data + 0);
   ihdr->height      = dword_be(chunk->data + 4);
   ihdr->depth       = chunk->data[8];
   ihdr->color_type  = chunk->data[9];
   ihdr->compression = chunk->data[10];
   ihdr->filter      = chunk->data[11];
   ihdr->interlace   = chunk->data[12];

   if (ihdr->width == 0 || ihdr->height == 0)
      GOTO_END_ERROR();

   if (ihdr->color_type == 2 || 
         ihdr->color_type == 4 || ihdr->color_type == 6)
   {
      if (ihdr->depth != 8 && ihdr->depth != 16)
         GOTO_END_ERROR();
   }
   else if (ihdr->color_type == 0)
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
   else if (ihdr->color_type == 3)
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
   else
      GOTO_END_ERROR();

#ifdef RPNG_TEST
   fprintf(stderr, "IHDR: (%u x %u), bpc = %u, palette = %s, color = %s, alpha = %s, adam7 = %s.\n",
         ihdr->width, ihdr->height,
         ihdr->depth, ihdr->color_type == 3 ? "yes" : "no",
         ihdr->color_type & 2 ? "yes" : "no",
         ihdr->color_type & 4 ? "yes" : "no",
         ihdr->interlace == 1 ? "yes" : "no");
#endif

   if (ihdr->compression != 0)
      GOTO_END_ERROR();

#if 0
   if (ihdr->interlace != 0) /* No Adam7 supported. */
      GOTO_END_ERROR();
#endif

end:
   png_free_chunk(chunk);
   return ret;
}

static bool png_append_idat_fio(FILE *file,
      const struct png_chunk *chunk, struct idat_buffer *buf)
{
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

static bool png_read_plte_fio(FILE *file, uint32_t *buffer, unsigned entries)
{
   unsigned i;
   uint8_t buf[256 * 3];

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

bool rpng_load_image_argb(const char *path, uint32_t **data,
      unsigned *width, unsigned *height)
{
   long pos, file_len;
   FILE *file;
   char header[8];
   z_stream stream = {0};
   struct rpng_t rpng = {0};
   struct rpng_process_t process = {0};
   bool ret      = true;

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
      struct png_chunk chunk = {0};

      if (!read_chunk_header_fio(file, &chunk))
         GOTO_END_ERROR();

      switch (png_chunk_type(&chunk))
      {
         case PNG_CHUNK_NOOP:
         default:
            if (fseek(file, chunk.size + sizeof(uint32_t), SEEK_CUR) < 0)
               GOTO_END_ERROR();
            break;

         case PNG_CHUNK_ERROR:
            GOTO_END_ERROR();

         case PNG_CHUNK_IHDR:
            if (rpng.has_ihdr || rpng.has_idat || rpng.has_iend)
               GOTO_END_ERROR();

            if (!png_parse_ihdr_fio(file, &chunk, &rpng.ihdr))
               GOTO_END_ERROR();

            rpng.has_ihdr = true;
            break;

         case PNG_CHUNK_PLTE:
            if (!rpng.has_ihdr || rpng.has_plte || rpng.has_iend || rpng.has_idat)
               GOTO_END_ERROR();

            if (chunk.size % 3)
               GOTO_END_ERROR();

            if (!png_read_plte_fio(file, rpng.palette, chunk.size / 3))
               GOTO_END_ERROR();

            rpng.has_plte = true;
            break;

         case PNG_CHUNK_IDAT:
            if (!rpng.has_ihdr || rpng.has_iend || (rpng.ihdr.color_type == 3 && !rpng.has_plte))
               GOTO_END_ERROR();

            if (!png_append_idat_fio(file, &chunk, &rpng.idat_buf))
               GOTO_END_ERROR();

            rpng.has_idat = true;
            break;

         case PNG_CHUNK_IEND:
            if (!rpng.has_ihdr || !rpng.has_idat)
               GOTO_END_ERROR();

            if (fseek(file, sizeof(uint32_t), SEEK_CUR) < 0)
               GOTO_END_ERROR();

            rpng.has_iend = true;
            break;
      }
   }

   if (!rpng.has_ihdr || !rpng.has_idat || !rpng.has_iend)
      GOTO_END_ERROR();

   if (inflateInit(&stream) != Z_OK)
      GOTO_END_ERROR();

   png_pass_geom(&rpng.ihdr, rpng.ihdr.width, rpng.ihdr.height, NULL, NULL, &rpng.inflate_buf_size);
   if (rpng.ihdr.interlace == 1) /* To be sure. */
      rpng.inflate_buf_size *= 2;

   rpng.inflate_buf = (uint8_t*)malloc(rpng.inflate_buf_size);
   if (!rpng.inflate_buf)
      GOTO_END_ERROR();

   stream.next_in   = rpng.idat_buf.data;
   stream.avail_in  = rpng.idat_buf.size;
   stream.avail_out = rpng.inflate_buf_size;
   stream.next_out  = rpng.inflate_buf;

   if (inflate(&stream, Z_FINISH) != Z_STREAM_END)
   {
      inflateEnd(&stream);
      GOTO_END_ERROR();
   }
   inflateEnd(&stream);

   *width  = rpng.ihdr.width;
   *height = rpng.ihdr.height;
#ifdef GEKKO
   /* we often use these in textures, make sure they're 32-byte aligned */
   *data = (uint32_t*)memalign(32, rpng.ihdr.width * rpng.ihdr.height * sizeof(uint32_t));
#else
   *data = (uint32_t*)malloc(rpng.ihdr.width * rpng.ihdr.height * sizeof(uint32_t));
#endif
   if (!*data)
      GOTO_END_ERROR();

   process.total_out   = stream.total_out;
   process.inflate_buf = rpng.inflate_buf;
   process.palette     = rpng.palette;

   if (rpng.ihdr.interlace == 1)
   {
      if (!png_reverse_filter_adam7(*data,
               &rpng.ihdr, &process))
         GOTO_END_ERROR();
   }
   else if (!png_reverse_filter(*data,
            &rpng.ihdr, &process))
      GOTO_END_ERROR();

end:
   if (file)
      fclose(file);
   if (!ret)
      free(*data);
   free(rpng.idat_buf.data);
   free(rpng.inflate_buf);
   return ret;
}
