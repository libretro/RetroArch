/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng_encode.c).
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

#include <libretro.h>
#include <encodings/crc32.h>
#include <streams/interface_stream.h>
#include <streams/trans_stream.h>

#include "rpng_internal.h"

#undef GOTO_END_ERROR
#define GOTO_END_ERROR() do { \
   fprintf(stderr, "[RPNG]: Error in line %d.\n", __LINE__); \
   ret = false; \
   goto end; \
} while (0)

double DEFLATE_PADDING = 1.1;
int PNG_ROUGH_HEADER = 100;

static void dword_write_be(uint8_t *buf, uint32_t val)
{
   *buf++ = (uint8_t)(val >> 24);
   *buf++ = (uint8_t)(val >> 16);
   *buf++ = (uint8_t)(val >>  8);
   *buf++ = (uint8_t)(val >>  0);
}

static bool png_write_crc_string(intfstream_t *intf_s, const uint8_t *data, size_t len)
{
   uint8_t crc_raw[4] = {0};
   uint32_t crc       = encoding_crc32(0, data, len);

   dword_write_be(crc_raw, crc);
   return intfstream_write(intf_s, crc_raw, sizeof(crc_raw)) == sizeof(crc_raw);
}

static bool png_write_ihdr_string(intfstream_t *intf_s, const struct png_ihdr *ihdr)
{
   uint8_t ihdr_raw[21];

   ihdr_raw[0]  = '0';                 /* Size */
   ihdr_raw[1]  = '0';
   ihdr_raw[2]  = '0';
   ihdr_raw[3]  = '0';
   ihdr_raw[4]  = 'I';
   ihdr_raw[5]  = 'H';
   ihdr_raw[6]  = 'D';
   ihdr_raw[7]  = 'R';
   ihdr_raw[8]  =   0;                 /* Width */
   ihdr_raw[9]  =   0;
   ihdr_raw[10] =   0;
   ihdr_raw[11] =   0;
   ihdr_raw[12] =   0;                 /* Height */
   ihdr_raw[13] =   0;
   ihdr_raw[14] =   0;
   ihdr_raw[15] =   0;
   ihdr_raw[16] =   ihdr->depth;       /* Depth */
   ihdr_raw[17] =   ihdr->color_type;
   ihdr_raw[18] =   ihdr->compression;
   ihdr_raw[19] =   ihdr->filter;
   ihdr_raw[20] =   ihdr->interlace;

   dword_write_be(ihdr_raw +  0, sizeof(ihdr_raw) - 8);
   dword_write_be(ihdr_raw +  8, ihdr->width);
   dword_write_be(ihdr_raw + 12, ihdr->height);
   if (intfstream_write(intf_s, ihdr_raw, sizeof(ihdr_raw)) != sizeof(ihdr_raw))
      return false;

   return png_write_crc_string(intf_s, ihdr_raw + sizeof(uint32_t),
         sizeof(ihdr_raw) - sizeof(uint32_t));
}

static bool png_write_idat_string(intfstream_t* intf_s, const uint8_t *data, size_t len)
{
   if (intfstream_write(intf_s, data, len) != (ssize_t)len)
      return false;
   return png_write_crc_string(intf_s, data + sizeof(uint32_t), len - sizeof(uint32_t));
}

static bool png_write_iend_string(intfstream_t* intf_s)
{
   const uint8_t data[] = {
      0, 0, 0, 0,
      'I', 'E', 'N', 'D',
   };

   if (intfstream_write(intf_s, data, sizeof(data)) != sizeof(data))
      return false;

   return png_write_crc_string(intf_s, data + sizeof(uint32_t),
         sizeof(data) - sizeof(uint32_t));
}

static void copy_argb_line(uint8_t *dst, const uint32_t *src, unsigned width)
{
   unsigned i;
   for (i = 0; i < width; i++)
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
   unsigned i;
   for (i = 0; i < width; i++, dst += 3, src += 3)
   {
      dst[2] = src[0];
      dst[1] = src[1];
      dst[0] = src[2];
   }
}

static unsigned count_sad(const uint8_t *data, size_t len)
{
   size_t i;
   unsigned cnt = 0;
   for (i = 0; i < len; i++)
   {
      if (data[i])
         cnt += abs((int8_t)data[i]);
   }
   return cnt;
}

static unsigned filter_up(uint8_t *target, const uint8_t *line,
      const uint8_t *prev, unsigned width, unsigned bpp)
{
   unsigned i;
   width *= bpp;
   for (i = 0; i < width; i++)
      target[i] = line[i] - prev[i];

   return count_sad(target, width);
}

static unsigned filter_sub(uint8_t *target, const uint8_t *line,
      unsigned width, unsigned bpp)
{
   unsigned i;
   width *= bpp;
   for (i = 0; i < bpp; i++)
      target[i] = line[i];
   for (i = bpp; i < width; i++)
      target[i] = line[i] - line[i - bpp];

   return count_sad(target, width);
}

static unsigned filter_avg(uint8_t *target, const uint8_t *line,
      const uint8_t *prev, unsigned width, unsigned bpp)
{
   unsigned i;
   width *= bpp;
   for (i = 0; i < bpp; i++)
      target[i] = line[i] - (prev[i] >> 1);
   for (i = bpp; i < width; i++)
      target[i] = line[i] - ((line[i - bpp] + prev[i]) >> 1);

   return count_sad(target, width);
}

static unsigned filter_paeth(uint8_t *target,
      const uint8_t *line, const uint8_t *prev,
      unsigned width, unsigned bpp)
{
   unsigned i;
   width *= bpp;
   for (i = 0; i < bpp; i++)
      target[i] = line[i] - paeth(0, prev[i], 0);
   for (i = bpp; i < width; i++)
      target[i] = line[i] - paeth(line[i - bpp], prev[i], prev[i - bpp]);

   return count_sad(target, width);
}

bool rpng_save_image_stream(const uint8_t *data, intfstream_t* intf_s,
      unsigned width, unsigned height, signed pitch, unsigned bpp)
{
   unsigned h;
   struct png_ihdr ihdr = {0};
   bool ret = true;
   const struct trans_stream_backend *stream_backend = NULL;
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
   void *stream            = NULL;
   uint32_t total_in       = 0;
   uint32_t total_out      = 0;

   if (!intf_s)
      GOTO_END_ERROR();

   stream_backend = trans_stream_get_zlib_deflate_backend();

   if (intfstream_write(intf_s, png_magic, sizeof(png_magic)) != sizeof(png_magic))
      GOTO_END_ERROR();

   ihdr.width = width;
   ihdr.height = height;
   ihdr.depth = 8;
   ihdr.color_type = bpp == sizeof(uint32_t) ? 6 : 2; /* RGBA or RGB */
   if (!png_write_ihdr_string(intf_s, &ihdr))
      GOTO_END_ERROR();

   encode_buf_size = (width * bpp + 1) * height;
   encode_buf      = (uint8_t*)malloc(encode_buf_size);
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
   for (h = 0; h < height;
         h++, encode_target += width * bpp, data += pitch)
   {
      if (bpp == sizeof(uint32_t))
         copy_argb_line(rgba_line, (const uint32_t*)data, width);
      else
         copy_bgr24_line(rgba_line, data, width);

      /* Try every filtering method, and choose the method
       * which has most entries as zero.
       *
       * This is probably not very optimal, but it's very
       * simple to implement.
       */
      {
         unsigned none_score  = count_sad(rgba_line, width * bpp);
         unsigned up_score    = filter_up(up_filtered, rgba_line, prev_encoded, width, bpp);
         unsigned sub_score   = filter_sub(sub_filtered, rgba_line, width, bpp);
         unsigned avg_score   = filter_avg(avg_filtered, rgba_line, prev_encoded, width, bpp);
         unsigned paeth_score = filter_paeth(paeth_filtered, rgba_line, prev_encoded, width, bpp);

         uint8_t filter       = 0;
         unsigned min_sad     = none_score;
         const uint8_t *chosen_filtered = rgba_line;

         if (sub_score < min_sad)
         {
            filter = 1;
            chosen_filtered = sub_filtered;
            min_sad = sub_score;
         }

         if (up_score < min_sad)
         {
            filter = 2;
            chosen_filtered = up_filtered;
            min_sad = up_score;
         }

         if (avg_score < min_sad)
         {
            filter = 3;
            chosen_filtered = avg_filtered;
            min_sad = avg_score;
         }

         if (paeth_score < min_sad)
         {
            filter = 4;
            chosen_filtered = paeth_filtered;
         }

         *encode_target++ = filter;
         memcpy(encode_target, chosen_filtered, width * bpp);

         memcpy(prev_encoded, rgba_line, width * bpp);
      }
   }

   deflate_buf = (uint8_t*)malloc(encode_buf_size * 2); /* Just to be sure. */
   if (!deflate_buf)
      GOTO_END_ERROR();

   stream = stream_backend->stream_new();

   if (!stream)
      GOTO_END_ERROR();

   stream_backend->set_in(
         stream,
         encode_buf,
         (unsigned)encode_buf_size);
   stream_backend->set_out(
         stream,
         deflate_buf + 8,
         (unsigned)(encode_buf_size * 2));

   if (!stream_backend->trans(stream, true, &total_in, &total_out, NULL))
      GOTO_END_ERROR();

   memcpy(deflate_buf + 4, "IDAT", 4);
   dword_write_be(deflate_buf + 0,        ((uint32_t)total_out));
   if (!png_write_idat_string(intf_s, deflate_buf, ((size_t)total_out + 8)))
      GOTO_END_ERROR();

   if (!png_write_iend_string(intf_s))
      GOTO_END_ERROR();
end:
   free(encode_buf);
   free(deflate_buf);
   free(rgba_line);
   free(prev_encoded);
   free(up_filtered);
   free(sub_filtered);
   free(avg_filtered);
   free(paeth_filtered);

   if (stream_backend)
   {
      if (stream)
      {
         if (stream_backend->stream_free)
            stream_backend->stream_free(stream);
      }
   }
   return ret;
}

bool rpng_save_image_argb(const char *path, const uint32_t *data,
      unsigned width, unsigned height, unsigned pitch)
{
   bool ret                      = false;
   intfstream_t* intf_s          = NULL;

   intf_s = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   ret = rpng_save_image_stream((const uint8_t*) data, intf_s,
                                width, height,
                                (signed) pitch, sizeof(uint32_t));
   intfstream_close(intf_s);
   free(intf_s);
   return ret;
}

bool rpng_save_image_bgr24(const char *path, const uint8_t *data,
      unsigned width, unsigned height, unsigned pitch)
{
   bool ret                      = false;
   intfstream_t* intf_s          = NULL;

   intf_s = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   ret = rpng_save_image_stream(data, intf_s, width, height,
                                (signed) pitch, 3);
   intfstream_close(intf_s);
   free(intf_s);
   return ret;
}


uint8_t* rpng_save_image_bgr24_string(const uint8_t *data,
      unsigned width, unsigned height, signed pitch, uint64_t* bytes)
{
   bool ret                    = false;
   uint8_t* buf                = NULL;
   uint8_t* output             = NULL;
   int buf_length              = 0;
   intfstream_t* intf_s        = NULL;

   buf_length = (int)(width*height*3*DEFLATE_PADDING)+PNG_ROUGH_HEADER;
   buf        = (uint8_t*)malloc(buf_length*sizeof(uint8_t));
   if (!buf)
      GOTO_END_ERROR();

   intf_s = intfstream_open_writable_memory(buf,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         buf_length);

   ret = rpng_save_image_stream((const uint8_t*)data,
            intf_s, width, height, pitch, 3);

   *bytes = intfstream_get_ptr(intf_s);
   intfstream_rewind(intf_s);
   output = (uint8_t*)malloc((size_t)((*bytes)*sizeof(uint8_t)));
   if (!output)
      GOTO_END_ERROR();
   intfstream_read(intf_s, output, *bytes);

end:
   if (buf)
      free(buf);
   if (intf_s)
   {
      intfstream_close(intf_s);
      free(intf_s);
   }
   if (ret == false)
   {
      if (output)
         free(output);
      return NULL;
   }
   return output;
}

