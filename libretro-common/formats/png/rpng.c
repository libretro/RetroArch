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

#undef GOTO_END_ERROR
#define GOTO_END_ERROR() do { \
   fprintf(stderr, "[RPNG]: Error in line %d.\n", __LINE__); \
   ret = false; \
   goto end; \
} while(0)

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

static bool png_parse_ihdr(FILE *file,
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


static bool png_reverse_filter(uint32_t *data, const struct png_ihdr *ihdr,
      const uint8_t *inflate_buf, size_t inflate_buf_size,
      const uint32_t *palette)
{
   unsigned i, h;
   unsigned bpp;
   unsigned pitch;
   size_t pass_size;
   uint8_t *prev_scanline = NULL;
   uint8_t *decoded_scanline = NULL;
   bool ret = true;

   png_pass_geom(ihdr, ihdr->width, ihdr->height, &bpp, &pitch, &pass_size);

   if (inflate_buf_size < pass_size)
      return false;

   prev_scanline    = (uint8_t*)calloc(1, pitch);
   decoded_scanline = (uint8_t*)calloc(1, pitch);

   if (!prev_scanline || !decoded_scanline)
      GOTO_END_ERROR();

   for (h = 0; h < ihdr->height;
         h++, inflate_buf += pitch, data += ihdr->width)
   {
      unsigned filter = *inflate_buf++;

      switch (filter)
      {
         case 0: /* None */
            memcpy(decoded_scanline, inflate_buf, pitch);
            break;

         case 1: /* Sub */
            for (i = 0; i < bpp; i++)
               decoded_scanline[i] = inflate_buf[i];
            for (i = bpp; i < pitch; i++)
               decoded_scanline[i] = decoded_scanline[i - bpp] + inflate_buf[i];
            break;

         case 2: /* Up */
            for (i = 0; i < pitch; i++)
               decoded_scanline[i] = prev_scanline[i] + inflate_buf[i];
            break;

         case 3: /* Average */
            for (i = 0; i < bpp; i++)
            {
               uint8_t avg = prev_scanline[i] >> 1;
               decoded_scanline[i] = avg + inflate_buf[i];
            }
            for (i = bpp; i < pitch; i++)
            {
               uint8_t avg = (decoded_scanline[i - bpp] + prev_scanline[i]) >> 1;
               decoded_scanline[i] = avg + inflate_buf[i];
            }
            break;

         case 4: /* Paeth */
            for (i = 0; i < bpp; i++)
               decoded_scanline[i] = paeth(0, prev_scanline[i], 0) + inflate_buf[i];
            for (i = bpp; i < pitch; i++)
               decoded_scanline[i] = paeth(decoded_scanline[i - bpp],
                     prev_scanline[i], prev_scanline[i - bpp]) + inflate_buf[i];
            break;

         default:
            GOTO_END_ERROR();
      }

      if (ihdr->color_type == 0)
         copy_line_bw(data, decoded_scanline, ihdr->width, ihdr->depth);
      else if (ihdr->color_type == 2)
         copy_line_rgb(data, decoded_scanline, ihdr->width, ihdr->depth);
      else if (ihdr->color_type == 3)
         copy_line_plt(data, decoded_scanline, ihdr->width,
               ihdr->depth, palette);
      else if (ihdr->color_type == 4)
         copy_line_gray_alpha(data, decoded_scanline, ihdr->width,
               ihdr->depth);
      else if (ihdr->color_type == 6)
         copy_line_rgba(data, decoded_scanline, ihdr->width, ihdr->depth);

      memcpy(prev_scanline, decoded_scanline, pitch);
   }

end:
   free(decoded_scanline);
   free(prev_scanline);
   return ret;
}

struct adam7_pass
{
   unsigned x;
   unsigned y;
   unsigned stride_x;
   unsigned stride_y;
};

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

static bool png_reverse_filter_adam7(uint32_t *data,
      const struct png_ihdr *ihdr,
      const uint8_t *inflate_buf, size_t inflate_buf_size,
      const uint32_t *palette)
{
   unsigned pass;
   static const struct adam7_pass passes[] = {
      { 0, 0, 8, 8 },
      { 4, 0, 8, 8 },
      { 0, 4, 4, 8 },
      { 2, 0, 4, 4 },
      { 0, 2, 2, 4 },
      { 1, 0, 2, 2 },
      { 0, 1, 1, 2 },
   };

   for (pass = 0; pass < ARRAY_SIZE(passes); pass++)
   {
      unsigned pass_width, pass_height;
      size_t pass_size;
      struct png_ihdr tmp_ihdr;
      uint32_t *tmp_data = NULL;

      if (ihdr->width <= passes[pass].x ||
            ihdr->height <= passes[pass].y) /* Empty pass */
         continue;

      pass_width  = (ihdr->width - 
            passes[pass].x + passes[pass].stride_x - 1) / passes[pass].stride_x;
      pass_height = (ihdr->height - passes[pass].y + 
            passes[pass].stride_y - 1) / passes[pass].stride_y;

      tmp_data = (uint32_t*)malloc(
            pass_width * pass_height * sizeof(uint32_t));

      if (!tmp_data)
         return false;

      tmp_ihdr = *ihdr;
      tmp_ihdr.width = pass_width;
      tmp_ihdr.height = pass_height;

      png_pass_geom(&tmp_ihdr, pass_width,
            pass_height, NULL, NULL, &pass_size);

      if (pass_size > inflate_buf_size)
      {
         free(tmp_data);
         return false;
      }

      if (!png_reverse_filter(tmp_data,
               &tmp_ihdr, inflate_buf, pass_size, palette))
      {
         free(tmp_data);
         return false;
      }

      inflate_buf += pass_size;
      inflate_buf_size -= pass_size;

      deinterlace_pass(data,
            ihdr, tmp_data, pass_width, pass_height, &passes[pass]);
      free(tmp_data);
   }

   return true;
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
   uint8_t *inflate_buf = NULL;
   size_t inflate_buf_size = 0;
   z_stream stream = {0};
   struct idat_buffer idat_buf = {0};
   struct png_ihdr ihdr = {0};
   uint32_t palette[256] = {0};
   bool has_ihdr = false;
   bool has_idat = false;
   bool has_iend = false;
   bool has_plte = false;
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
            if (has_ihdr || has_idat || has_iend)
               GOTO_END_ERROR();

            if (!png_parse_ihdr(file, &chunk, &ihdr))
               GOTO_END_ERROR();

            has_ihdr = true;
            break;

         case PNG_CHUNK_PLTE:
            if (!has_ihdr || has_plte || has_iend || has_idat)
               GOTO_END_ERROR();

            if (chunk.size % 3)
               GOTO_END_ERROR();

            if (!png_read_plte_fio(file, palette, chunk.size / 3))
               GOTO_END_ERROR();

            has_plte = true;
            break;

         case PNG_CHUNK_IDAT:
            if (!has_ihdr || has_iend || (ihdr.color_type == 3 && !has_plte))
               GOTO_END_ERROR();

            if (!png_append_idat_fio(file, &chunk, &idat_buf))
               GOTO_END_ERROR();

            has_idat = true;
            break;

         case PNG_CHUNK_IEND:
            if (!has_ihdr || !has_idat)
               GOTO_END_ERROR();

            if (fseek(file, sizeof(uint32_t), SEEK_CUR) < 0)
               GOTO_END_ERROR();

            has_iend = true;
            break;
      }
   }

   if (!has_ihdr || !has_idat || !has_iend)
      GOTO_END_ERROR();

   if (inflateInit(&stream) != Z_OK)
      GOTO_END_ERROR();

   png_pass_geom(&ihdr, ihdr.width, ihdr.height, NULL, NULL, &inflate_buf_size);
   if (ihdr.interlace == 1) /* To be sure. */
      inflate_buf_size *= 2;

   inflate_buf = (uint8_t*)malloc(inflate_buf_size);
   if (!inflate_buf)
      GOTO_END_ERROR();

   stream.next_in   = idat_buf.data;
   stream.avail_in  = idat_buf.size;
   stream.avail_out = inflate_buf_size;
   stream.next_out  = inflate_buf;

   if (inflate(&stream, Z_FINISH) != Z_STREAM_END)
   {
      inflateEnd(&stream);
      GOTO_END_ERROR();
   }
   inflateEnd(&stream);

   *width  = ihdr.width;
   *height = ihdr.height;
#ifdef GEKKO
   /* we often use these in textures, make sure they're 32-byte aligned */
   *data = (uint32_t*)memalign(32, ihdr.width * ihdr.height * sizeof(uint32_t));
#else
   *data = (uint32_t*)malloc(ihdr.width * ihdr.height * sizeof(uint32_t));
#endif
   if (!*data)
      GOTO_END_ERROR();

   if (ihdr.interlace == 1)
   {
      if (!png_reverse_filter_adam7(*data,
               &ihdr, inflate_buf, stream.total_out, palette))
         GOTO_END_ERROR();
   }
   else if (!png_reverse_filter(*data,
            &ihdr, inflate_buf, stream.total_out, palette))
      GOTO_END_ERROR();

end:
   if (file)
      fclose(file);
   if (!ret)
      free(*data);
   free(idat_buf.data);
   free(inflate_buf);
   return ret;
}
