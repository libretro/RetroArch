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

#include "rpng.h"

#ifdef WANT_RZLIB
#include "../../deps/rzlib/zlib.h"
#else
#include <zlib.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../hash.h"

// Decodes a subset of PNG standard.
// Does not handle much outside 24/32-bit RGB(A) images.
//
// Missing: Adam7 interlace, 16 bpp, various color formats.

#define GOTO_END_ERROR() do { \
   fprintf(stderr, "[RPNG]: Error in line %d.\n", __LINE__); \
   ret = false; \
   goto end; \
} while(0)

static const uint8_t png_magic[8] = {
   0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a,
};

struct png_chunk
{
   uint32_t size;
   char type[4];
   uint8_t *data;
};

struct png_ihdr
{
   uint32_t width;
   uint32_t height;
   uint8_t depth;
   uint8_t color_type;
   uint8_t compression;
   uint8_t filter;
   uint8_t interlace;
};

enum png_chunk_type
{
   PNG_CHUNK_NOOP = 0,
   PNG_CHUNK_ERROR,
   PNG_CHUNK_IHDR,
   PNG_CHUNK_IDAT,
   PNG_CHUNK_IEND
};

static uint32_t dword_be(const uint8_t *buf)
{
   return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] << 0);
}

static bool read_chunk_header(FILE *file, struct png_chunk *chunk)
{
   uint8_t dword[4] = {0};
   if (fread(dword, 1, 4, file) != 4)
      return false;

   chunk->size = dword_be(dword);

   if (fread(chunk->type, 1, 4, file) != 4)
      return false;

   return true;
}

struct
{
   const char *id;
   enum png_chunk_type type;
} static const chunk_map[] = {
   { "IHDR", PNG_CHUNK_IHDR },
   { "IDAT", PNG_CHUNK_IDAT },
   { "IEND", PNG_CHUNK_IEND },
};

struct idat_buffer
{
   uint8_t *data;
   size_t size;
};

static enum png_chunk_type png_chunk_type(const struct png_chunk *chunk)
{
   for (unsigned i = 0; i < sizeof(chunk_map) / sizeof(chunk_map[0]); i++)
   {
      if (memcmp(chunk->type, chunk_map[i].id, 4) == 0)
         return chunk_map[i].type;
   }

   return PNG_CHUNK_NOOP;
}

static bool png_read_chunk(FILE *file, struct png_chunk *chunk)
{
   free(chunk->data);
   chunk->data = (uint8_t*)calloc(1, chunk->size + sizeof(uint32_t)); // CRC32
   if (!chunk->data)
      return false;

   if (fread(chunk->data, 1, chunk->size + sizeof(uint32_t), file) != (chunk->size + sizeof(uint32_t)))
   {
      free(chunk->data);
      return false;
   }

   // Ignore CRC.

   return true;
}

static void png_free_chunk(struct png_chunk *chunk)
{
   free(chunk->data);
   chunk->data = NULL;
}

static bool png_parse_ihdr(FILE *file, struct png_chunk *chunk, struct png_ihdr *ihdr)
{
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

   if (ihdr->depth != 8) // Only 8bpc supported.
      GOTO_END_ERROR();

   if (ihdr->color_type != 2 && ihdr->color_type != 6) // Only RGB/RGBA supported.
      GOTO_END_ERROR();

   if (ihdr->compression != 0)
      GOTO_END_ERROR();

   if (ihdr->interlace != 0) // No Adam7 supported.
      GOTO_END_ERROR();

end:
   png_free_chunk(chunk);
   return ret;
}

// Paeth prediction filter.
static inline int paeth(int a, int b, int c)
{
   int p = a + b - c;
   int pa = abs(p - a);
   int pb = abs(p - b);
   int pc = abs(p - c);

   if (pa <= pb && pa <= pc)
      return a;
   else if (pb <= pc)
      return b;
   else
      return c;
}

static inline void copy_line_rgb(uint32_t *data, const uint8_t *decoded, unsigned width)
{
   for (unsigned i = 0; i < width; i++)
   {
      uint32_t r = *decoded++;
      uint32_t g = *decoded++;
      uint32_t b = *decoded++;
      data[i] = (0xffu << 24) | (r << 16) | (g << 8) | (b << 0);
   }
}

static inline void copy_line_rgba(uint32_t *data, const uint8_t *decoded, unsigned width)
{
   for (unsigned i = 0; i < width; i++)
   {
      uint32_t r = *decoded++;
      uint32_t g = *decoded++;
      uint32_t b = *decoded++;
      uint32_t a = *decoded++;
      data[i] = (a << 24) | (r << 16) | (g << 8) | (b << 0);
   }
}

static bool png_reverse_filter(uint32_t *data, const struct png_ihdr *ihdr,
      const uint8_t *inflate_buf, size_t inflate_buf_size)
{
   bool ret = true;
   unsigned bpp = ihdr->color_type == 2 ? 3 : 4;
   if (inflate_buf_size < (ihdr->width * bpp + 1) * ihdr->height)
      return false;

   unsigned pitch = ihdr->width * bpp;
   uint8_t *prev_scanline    = (uint8_t*)calloc(1, pitch);
   uint8_t *decoded_scanline = (uint8_t*)calloc(1, pitch);

   if (!decoded_scanline || !decoded_scanline)
      GOTO_END_ERROR();

   for (unsigned h = 0; h < ihdr->height;
         h++, inflate_buf += pitch, data += ihdr->width)
   {
      unsigned filter = *inflate_buf++;
      switch (filter)
      {
         case 0: // None
            memcpy(decoded_scanline, inflate_buf, pitch);
            break;

         case 1: // Sub
            for (unsigned i = 0; i < bpp; i++)
               decoded_scanline[i] = inflate_buf[i];
            for (unsigned i = bpp; i < pitch; i++)
               decoded_scanline[i] = decoded_scanline[i - bpp] + inflate_buf[i];
            break;

         case 2: // Up
            for (unsigned i = 0; i < pitch; i++)
               decoded_scanline[i] = prev_scanline[i] + inflate_buf[i];
            break;

         case 3: // Average
            for (unsigned i = 0; i < bpp; i++)
            {
               uint8_t avg = prev_scanline[i] >> 1;
               decoded_scanline[i] = avg + inflate_buf[i];
            }
            for (unsigned i = bpp; i < pitch; i++)
            {
               uint8_t avg = (decoded_scanline[i - bpp] + prev_scanline[i]) >> 1;
               decoded_scanline[i] = avg + inflate_buf[i];
            }
            break;

         case 4: // Paeth
            for (unsigned i = 0; i < bpp; i++)
               decoded_scanline[i] = paeth(0, prev_scanline[i], 0) + inflate_buf[i];
            for (unsigned i = bpp; i < pitch; i++)
               decoded_scanline[i] = paeth(decoded_scanline[i - bpp], prev_scanline[i], prev_scanline[i - bpp]) + inflate_buf[i];
            break;

         default:
            GOTO_END_ERROR();
      }

      if (bpp == 3)
         copy_line_rgb(data, decoded_scanline, ihdr->width);
      else
         copy_line_rgba(data, decoded_scanline, ihdr->width);

      memcpy(prev_scanline, decoded_scanline, pitch);
   }

end:
   free(decoded_scanline);
   free(prev_scanline);
   return ret;
}

static bool png_append_idat(FILE *file, const struct png_chunk *chunk, struct idat_buffer *buf)
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

bool rpng_load_image_argb(const char *path, uint32_t **data, unsigned *width, unsigned *height)
{
   *data   = NULL;
   *width  = 0;
   *height = 0;

   bool ret = true;
   FILE *file = fopen(path, "rb");
   if (!file)
      return NULL;

   fseek(file, 0, SEEK_END);
   long file_len = ftell(file);
   rewind(file);

   bool has_ihdr = false;
   bool has_idat = false;
   bool has_iend = false;
   uint8_t *inflate_buf = NULL;
   size_t inflate_buf_size = 0;
   z_stream stream = {0};

   struct idat_buffer idat_buf = {0};
   struct png_ihdr ihdr = {0};

   char header[8];
   if (fread(header, 1, sizeof(header), file) != sizeof(header))
      GOTO_END_ERROR();

   if (memcmp(header, png_magic, sizeof(png_magic)) != 0)
      GOTO_END_ERROR();

   // feof() apparently isn't triggered after a seek (IEND).
   for (long pos = ftell(file); pos < file_len && pos >= 0; pos = ftell(file))
   {
      struct png_chunk chunk = {0};
      if (!read_chunk_header(file, &chunk))
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

         case PNG_CHUNK_IDAT:
            if (!has_ihdr || has_iend)
               GOTO_END_ERROR();

            if (!png_append_idat(file, &chunk, &idat_buf))
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

   inflate_buf_size = (ihdr.width + 1) * ihdr.height * sizeof(uint32_t);
   inflate_buf = (uint8_t*)malloc(inflate_buf_size);
   if (!inflate_buf)
      GOTO_END_ERROR();

   stream.next_in   = idat_buf.data;
   stream.avail_in  = idat_buf.size;
   stream.avail_out = inflate_buf_size;
   stream.next_out  = inflate_buf;

   if (inflate(&stream, Z_SYNC_FLUSH) != Z_STREAM_END)
   {
      inflateEnd(&stream);
      GOTO_END_ERROR();
   }
   inflateEnd(&stream);

   *width  = ihdr.width;
   *height = ihdr.height;
   *data = (uint32_t*)malloc(ihdr.width * ihdr.height * sizeof(uint32_t));
   if (!*data)
      GOTO_END_ERROR();

   if (!png_reverse_filter(*data, &ihdr, inflate_buf, stream.total_out))
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

#ifdef HAVE_ZLIB_DEFLATE

static void dword_write_be(uint8_t *buf, uint32_t val)
{
   *buf++ = (uint8_t)(val >> 24);
   *buf++ = (uint8_t)(val >> 16);
   *buf++ = (uint8_t)(val >>  8);
   *buf++ = (uint8_t)(val >>  0);
}

static bool png_write_crc(FILE *file, const uint8_t *data, size_t size)
{
   uint32_t crc = crc32_calculate(data, size);
   uint8_t crc_raw[4] = {0};
   dword_write_be(crc_raw, crc);
   return fwrite(crc_raw, 1, sizeof(crc_raw), file) == sizeof(crc_raw);
}

static bool png_write_ihdr(FILE *file, const struct png_ihdr *ihdr)
{
   uint8_t ihdr_raw[] = {
      '0', '0', '0', '0', // Size
      'I', 'H', 'D', 'R',

      0, 0, 0, 0, // Width
      0, 0, 0, 0, // Height
      ihdr->depth,
      ihdr->color_type,
      ihdr->compression,
      ihdr->filter,
      ihdr->interlace,
   };

   dword_write_be(ihdr_raw +  0, sizeof(ihdr_raw) - 8);
   dword_write_be(ihdr_raw +  8, ihdr->width);
   dword_write_be(ihdr_raw + 12, ihdr->height);
   if (fwrite(ihdr_raw, 1, sizeof(ihdr_raw), file) != sizeof(ihdr_raw))
      return false;

   if (!png_write_crc(file, ihdr_raw + sizeof(uint32_t), sizeof(ihdr_raw) - sizeof(uint32_t)))
      return false;

   return true;
}

static bool png_write_idat(FILE *file, const uint8_t *data, size_t size)
{
   if (fwrite(data, 1, size, file) != size)
      return false;

   if (!png_write_crc(file, data + sizeof(uint32_t), size - sizeof(uint32_t)))
      return false;

   return true;
}

static bool png_write_iend(FILE *file)
{
   const uint8_t data[] = {
      0, 0, 0, 0,
      'I', 'E', 'N', 'D',
   };

   if (fwrite(data, 1, sizeof(data), file) != sizeof(data))
      return false;

   if (!png_write_crc(file, data + sizeof(uint32_t), sizeof(data) - sizeof(uint32_t)))
      return false;

   return true;
}

static void copy_argb_line(uint8_t *dst, const uint32_t *src, unsigned width)
{
   for (unsigned i = 0; i < width; i++)
   {
      uint32_t col = src[i];
      *dst++ = (uint8_t)(col >> 16);
      *dst++ = (uint8_t)(col >>  8);
      *dst++ = (uint8_t)(col >>  0);
      *dst++ = (uint8_t)(col >> 24);
   }
}

static void copy_bgr24_line(uint8_t *dst, const uint8_t *src, unsigned width)
{
   for (unsigned i = 0; i < width; i++, dst += 3, src += 3)
   {
      dst[2] = src[0];
      dst[1] = src[1];
      dst[0] = src[2];
   }
}

static unsigned count_zeroes(const uint8_t *data, size_t size)
{
   unsigned cnt = 0;
   for (size_t i = 0; i < size; i++)
      cnt += data[i] == 0;
   return cnt;
}

static unsigned filter_up(uint8_t *target, const uint8_t *line, const uint8_t *prev,
      unsigned width, unsigned bpp)
{
   width *= bpp;
   for (unsigned i = 0; i < width; i++)
      target[i] = line[i] - prev[i];

   return count_zeroes(target, width);
}

static unsigned filter_sub(uint8_t *target, const uint8_t *line,
      unsigned width, unsigned bpp)
{
   width *= bpp;
   for (unsigned i = 0; i < bpp; i++)
      target[i] = line[i];
   for (unsigned i = bpp; i < width; i++)
      target[i] = line[i] - line[i - bpp];

   return count_zeroes(target, width);
}

static unsigned filter_avg(uint8_t *target, const uint8_t *line, const uint8_t *prev,
      unsigned width, unsigned bpp)
{
   width *= bpp;
   for (unsigned i = 0; i < bpp; i++)
      target[i] = line[i] - (prev[i] >> 1);
   for (unsigned i = bpp; i < width; i++)
      target[i] = line[i] - ((line[i - bpp] + prev[i]) >> 1);

   return count_zeroes(target, width);
}

static unsigned filter_paeth(uint8_t *target, const uint8_t *line, const uint8_t *prev,
      unsigned width, unsigned bpp)
{
   width *= bpp;
   for (unsigned i = 0; i < bpp; i++)
      target[i] = line[i] - paeth(0, prev[i], 0);
   for (unsigned i = bpp; i < width; i++)
      target[i] = line[i] - paeth(line[i - bpp], prev[i], prev[i - bpp]);

   return count_zeroes(target, width);
}

static bool rpng_save_image(const char *path, const uint8_t *data,
      unsigned width, unsigned height, unsigned pitch, unsigned bpp)
{
   bool ret = true;
   struct png_ihdr ihdr = {0};

   size_t encode_buf_size  = 0;
   uint8_t *encode_buf     = NULL;
   uint8_t *deflate_buf    = NULL;
   uint8_t *rgba_line      = NULL;
   uint8_t *up_filtered    = NULL;
   uint8_t *sub_filtered   = NULL;
   uint8_t *avg_filtered   = NULL;
   uint8_t *paeth_filtered = NULL;
   uint8_t *prev_encoded   = NULL;
   uint8_t *encode_target  = NULL;

   z_stream stream = {0};

   FILE *file = fopen(path, "wb");
   if (!file)
      GOTO_END_ERROR();

   if (fwrite(png_magic, 1, sizeof(png_magic), file) != sizeof(png_magic))
      GOTO_END_ERROR();

   ihdr.width = width;
   ihdr.height = height;
   ihdr.depth = 8;
   ihdr.color_type = bpp == sizeof(uint32_t) ? 6 : 2; // RGBA or RGB
   if (!png_write_ihdr(file, &ihdr))
      GOTO_END_ERROR();

   encode_buf_size = (width * bpp + 1) * height;
   encode_buf = (uint8_t*)malloc(encode_buf_size);
   if (!encode_buf)
      GOTO_END_ERROR();

   prev_encoded = (uint8_t*)calloc(1, width * bpp);
   if (!prev_encoded)
      GOTO_END_ERROR();

   rgba_line      = (uint8_t*)malloc(width * bpp);
   up_filtered    = (uint8_t*)malloc(width * bpp);
   sub_filtered   = (uint8_t*)malloc(width * bpp);
   avg_filtered   = (uint8_t*)malloc(width * bpp);
   paeth_filtered = (uint8_t*)malloc(width * bpp);
   if (!rgba_line || !up_filtered || !sub_filtered || !avg_filtered || !paeth_filtered)
      GOTO_END_ERROR();

   encode_target = encode_buf;
   for (unsigned h = 0; h < height;
         h++, encode_target += width * bpp, data += pitch)
   {
      if (bpp == sizeof(uint32_t))
         copy_argb_line(rgba_line, (const uint32_t*)data, width);
      else
         copy_bgr24_line(rgba_line, data, width);

      // Try every filtering method, and choose the method
      // which has most entries as zero.
      // This is probably not very optimal, but it's very simple to implement.
      unsigned none_score  = count_zeroes(rgba_line, width * bpp);
      unsigned up_score    = filter_up(up_filtered, rgba_line, prev_encoded, width, bpp);
      unsigned sub_score   = filter_sub(sub_filtered, rgba_line, width, bpp);
      unsigned avg_score   = filter_avg(avg_filtered, rgba_line, prev_encoded, width, bpp);
      unsigned paeth_score = filter_paeth(paeth_filtered, rgba_line, prev_encoded, width, bpp);

      uint8_t filter = 0;
      unsigned max_zeros = none_score;
      const uint8_t *chosen_filtered = rgba_line;

      if (sub_score > max_zeros)
      {
         filter = 1;
         chosen_filtered = sub_filtered;
         max_zeros = sub_score;
      }

      if (up_score > max_zeros)
      {
         filter = 2;
         chosen_filtered = up_filtered;
         max_zeros = up_score;
      }

      if (avg_score > max_zeros)
      {
         filter = 3;
         chosen_filtered = avg_filtered;
         max_zeros = avg_score;
      }

      if (paeth_score > max_zeros)
      {
         filter = 4;
         chosen_filtered = paeth_filtered;
         max_zeros = paeth_score;
      }

      *encode_target++ = filter;
      memcpy(encode_target, chosen_filtered, width * bpp);

      memcpy(prev_encoded, rgba_line, width * bpp);
   }

   deflate_buf = (uint8_t*)malloc(encode_buf_size * 2); // Just to be sure.
   if (!deflate_buf)
      GOTO_END_ERROR();

   stream.next_in   = encode_buf;
   stream.avail_in  = encode_buf_size;
   stream.next_out  = deflate_buf + 8;
   stream.avail_out = encode_buf_size * 2;

   deflateInit(&stream, 2);
   if (deflate(&stream, Z_FINISH) != Z_STREAM_END)
   {
      deflateEnd(&stream);
      GOTO_END_ERROR();
   }

   memcpy(deflate_buf + 4, "IDAT", 4);
   dword_write_be(deflate_buf + 0, stream.total_out);
   if (!png_write_idat(file, deflate_buf, stream.total_out + 8))
      GOTO_END_ERROR();

   if (!png_write_iend(file))
      GOTO_END_ERROR();

end:
   if (file)
      fclose(file);
   free(encode_buf);
   free(deflate_buf);
   free(rgba_line);
   free(prev_encoded);
   free(up_filtered);
   free(avg_filtered);
   free(paeth_filtered);
   return ret;
}

bool rpng_save_image_argb(const char *path, const uint32_t *data,
      unsigned width, unsigned height, unsigned pitch)
{
   return rpng_save_image(path, (const uint8_t*)data, width, height, pitch, sizeof(uint32_t));
}

bool rpng_save_image_bgr24(const char *path, const uint8_t *data,
      unsigned width, unsigned height, unsigned pitch)
{
   return rpng_save_image(path, (const uint8_t*)data, width, height, pitch, 3);
}

#endif

