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

#include <file/nbio.h>

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

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

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
   PNG_CHUNK_PLTE,
   PNG_CHUNK_IEND
};

static uint32_t dword_be(const uint8_t *buf)
{
   return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] << 0);
}

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

struct idat_buffer
{
   uint8_t *data;
   size_t size;
};

static enum png_chunk_type png_chunk_type(const struct png_chunk *chunk)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(chunk_map); i++)
   {
      if (memcmp(chunk->type, chunk_map[i].id, 4) == 0)
         return chunk_map[i].type;
   }

   return PNG_CHUNK_NOOP;
}

static bool png_parse_ihdr(uint8_t *buf,
      struct png_ihdr *ihdr)
{
   unsigned i;
   bool ret = true;

   buf += 4 + 4;

   ihdr->width       = dword_be(buf + 0);
   ihdr->height      = dword_be(buf + 4);
   ihdr->depth       = buf[8];
   ihdr->color_type  = buf[9];
   ihdr->compression = buf[10];
   ihdr->filter      = buf[11];
   ihdr->interlace   = buf[12];

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
   return ret;
}

static inline void copy_line_rgb(uint32_t *data,
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

static inline void copy_line_rgba(uint32_t *data,
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

static inline void copy_line_bw(uint32_t *data,
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

static inline void copy_line_gray_alpha(uint32_t *data,
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

static inline void copy_line_plt(uint32_t *data,
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

static bool png_realloc_idat(const struct png_chunk *chunk, struct idat_buffer *buf)
{
   uint8_t *new_buffer = (uint8_t*)realloc(buf->data, buf->size + chunk->size);

   if (!new_buffer)
      return false;

   buf->data  = new_buffer;
   return true;
}

static bool png_read_plte_into_buf(uint32_t *buffer, unsigned entries)
{
   unsigned i;
   uint8_t buf[256 * 3];

   for (i = 0; i < entries; i++)
   {
      uint32_t r = buf[3 * i + 0];
      uint32_t g = buf[3 * i + 1];
      uint32_t b = buf[3 * i + 2];
      buffer[i] = (r << 16) | (g << 8) | (b << 0) | (0xffu << 24);
   }

   return true;
}

bool rpng_load_image_argb_iterate(uint8_t *buf,
      struct png_chunk *chunk,
      uint32_t *palette,
      struct png_ihdr *ihdr, struct idat_buffer *idat_buf,
      bool *has_ihdr, bool *has_idat,
      bool *has_iend, bool *has_plte, size_t *increment_size)
{
   unsigned i;

#if 0
   for (i = 0; i < 4; i++)
   {
      fprintf(stderr, "chunktype: %c\n", chunk->type[i]);
   }
#endif

   switch (png_chunk_type(chunk))
   {
      case PNG_CHUNK_NOOP:
      default:
         break;

      case PNG_CHUNK_ERROR:
         return false;

      case PNG_CHUNK_IHDR:
         if (*has_ihdr || *has_idat || *has_iend)
            return false;

         if (chunk->size != 13)
            return false;

         if (!png_parse_ihdr(buf, ihdr))
            return false;

         *has_ihdr = true;
         break;

      case PNG_CHUNK_PLTE:
         {
            unsigned entries = chunk->size / 3;

            if (!*has_ihdr || *has_plte || *has_iend || *has_idat)
               return false;

            if (chunk->size % 3)
               return false;

            if (entries > 256)
               return false;

            buf += 8;

            for (i = 0; i < entries; i++)
               palette[i] = buf[i];

            if (!png_read_plte_into_buf(palette, entries))
               return false;

            *has_plte = true;
         }
         break;

      case PNG_CHUNK_IDAT:
         if (!(*has_ihdr) || *has_iend || (ihdr->color_type == 3 && !(*has_plte)))
            return false;

         if (!png_realloc_idat(chunk, idat_buf))
            return false;

         buf += 8;

         for (i = 0; i < chunk->size; i++)
            idat_buf->data[i + idat_buf->size] = buf[i];

         idat_buf->size += chunk->size;

         *has_idat = true;
         break;

      case PNG_CHUNK_IEND:
         if (!(*has_ihdr) || !(*has_idat))
            return false;

         *has_iend = true;
         return false;
   }

   return true;
}

bool rpng_load_image_argb_process(uint8_t *inflate_buf,
      struct png_ihdr *ihdr,
      struct idat_buffer *idat_buf, uint32_t **data,
      uint32_t *palette,
      unsigned *width, unsigned *height)
{
   z_stream stream = {0};
   size_t inflate_buf_size = 0;

   if (inflateInit(&stream) != Z_OK)
      return false;

   png_pass_geom(ihdr, ihdr->width,
         ihdr->height, NULL, NULL, &inflate_buf_size);
   if (ihdr->interlace == 1) /* To be sure. */
      inflate_buf_size *= 2;

   inflate_buf = (uint8_t*)malloc(inflate_buf_size);
   if (!inflate_buf)
      return false;

   stream.next_in   = idat_buf->data;
   stream.avail_in  = idat_buf->size;
   stream.avail_out = inflate_buf_size;
   stream.next_out  = inflate_buf;

   if (inflate(&stream, Z_FINISH) != Z_STREAM_END)
   {
      inflateEnd(&stream);
      return false;
   }
   inflateEnd(&stream);

   *width  = ihdr->width;
   *height = ihdr->height;
#ifdef GEKKO
   /* we often use these in textures, make sure they're 32-byte aligned */
   *data = (uint32_t*)memalign(32, ihdr->width * ihdr->height * sizeof(uint32_t));
#else
   *data = (uint32_t*)malloc(ihdr->width * ihdr->height * sizeof(uint32_t));
#endif
   if (!*data)
      return false;

   if (ihdr->interlace == 1)
   {
      if (!png_reverse_filter_adam7(*data,
               ihdr, inflate_buf, stream.total_out, palette))
         return false;
   }
   else if (!png_reverse_filter(*data,
            ihdr, inflate_buf, stream.total_out, palette))
      return false;

   return true;
}

bool rpng_load_image_argb(const char *path, uint32_t **data,
      unsigned *width, unsigned *height)
{
   size_t file_len;
   uint8_t *buff_data = NULL;
   struct nbio_t* nbread = NULL;
   uint8_t *inflate_buf = NULL;
   struct idat_buffer idat_buf = {0};
   struct png_ihdr ihdr = {0};
   uint32_t palette[256] = {0};
   bool has_ihdr = false;
   bool has_idat = false;
   bool has_iend = false;
   bool has_plte = false;
   bool ret      = true;
   void* ptr = NULL;
   size_t increment = 0;

   {
      bool looped = false;
      nbread = nbio_open(path, NBIO_READ);
      ptr  = nbio_get_ptr(nbread, &file_len);
      nbio_begin_read(nbread);

      while (!nbio_iterate(nbread)) looped=true;
      ptr = nbio_get_ptr(nbread, &file_len);
      (void)ptr;
      (void)looped;

      buff_data = (uint8_t*)ptr;
   }

   {
      unsigned i;
      char header[8];

      for (i = 0; i < 8; i++)
         header[i] = buff_data[i];

      if (memcmp(header, png_magic, sizeof(png_magic)) != 0)
         return false;

      buff_data += 8;
   }

   while (1)
   {
      struct png_chunk chunk = {0};

      if (!read_chunk_header(buff_data, &chunk))
         GOTO_END_ERROR();

      if (!rpng_load_image_argb_iterate(
            buff_data, &chunk, palette, &ihdr, &idat_buf,
            &has_ihdr, &has_idat, &has_iend, &has_plte,
            &increment))
         break;

      buff_data += 4 + 4 + chunk.size + 4;
   }

#if 0
   fprintf(stderr, "has_ihdr: %d\n", has_ihdr);
   fprintf(stderr, "has_idat: %d\n", has_idat);
   fprintf(stderr, "has_iend: %d\n", has_iend);
#endif

   if (!has_ihdr || !has_idat || !has_iend)
      GOTO_END_ERROR();
   
   rpng_load_image_argb_process(inflate_buf,
         &ihdr, &idat_buf, data, palette,
         width, height);

end:
   nbio_free(nbread);
   if (!ret)
      free(*data);
   if (idat_buf.data)
      free(idat_buf.data);
   if (inflate_buf)
      free(inflate_buf);
   return ret;
}
