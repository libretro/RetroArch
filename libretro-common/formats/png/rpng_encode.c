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
   fprintf(stderr, "[RPNG] Error in line %d.\n", __LINE__); \
   ret = false; \
   goto end; \
} while (0)

static const double DEFLATE_PADDING = 1.1;
static const int PNG_ROUGH_HEADER  = 100;

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
      /* Use conditional instead of abs() to avoid undefined behaviour
       * when the value is -128 (INT8_MIN). */
      int8_t val = (int8_t)data[i];
      cnt += val < 0 ? -val : val;
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

/* Size of the per-chunk deflate output buffer.  A screenshot-sized
 * encode will fill this many times over and produce multiple IDAT
 * chunks; smaller than zlib's default window (32 KiB) to keep
 * peak memory low while large enough to amortise chunk-header
 * overhead (12 bytes per IDAT). */
#define IDAT_CHUNK_SIZE 16384

/* Emit one IDAT chunk.  `chunk_buf` is laid out as:
 *     bytes [0..4): length field (filled in here, big-endian)
 *     bytes [4..8): literal "IDAT"
 *     bytes [8..8+payload_len): the deflate output produced by
 *                               this chunk's worth of trans() calls
 * Matches the layout png_write_idat_string expects. */
static bool flush_idat_chunk(intfstream_t *intf_s,
      uint8_t *chunk_buf, size_t payload_len)
{
   if (payload_len == 0)
      return true; /* empty chunk -- nothing to emit, not an error */
   dword_write_be(chunk_buf + 0, (uint32_t)payload_len);
   memcpy(chunk_buf + 4, "IDAT", 4);
   return png_write_idat_string(intf_s, chunk_buf, payload_len + 8);
}

bool rpng_save_image_stream(const uint8_t *data, intfstream_t* intf_s,
      unsigned width, unsigned height, signed pitch, unsigned bpp)
{
   unsigned h;
   struct png_ihdr ihdr = {0};
   bool ret = true;
   const struct trans_stream_backend *stream_backend = NULL;
   uint8_t *rgba_line        = NULL;
   uint8_t *up_filtered      = NULL;
   uint8_t *sub_filtered     = NULL;
   uint8_t *avg_filtered     = NULL;
   uint8_t *paeth_filtered   = NULL;
   uint8_t *prev_encoded     = NULL;
   /* filter_line holds [filter_byte][filtered_row] and is what we
    * feed into deflate one row at a time. */
   uint8_t *filter_line      = NULL;
   /* chunk_buf is the IDAT-chunk staging buffer:
    *   [0..4):        length field (filled in at flush time)
    *   [4..8):        "IDAT"
    *   [8..8+IDAT_CHUNK_SIZE): deflate output */
   uint8_t *chunk_buf        = NULL;
   void *stream              = NULL;
   size_t line_len           = (size_t)width * bpp;
   /* How many bytes deflate has produced into the current chunk_buf
    * since the last set_out.  Reset to 0 after every flush_idat_chunk. */
   size_t chunk_fill         = 0;
   enum trans_stream_error err = TRANS_STREAM_ERROR_NONE;

   if (!intf_s)
      GOTO_END_ERROR();

   stream_backend = trans_stream_get_zlib_deflate_backend();

   if (intfstream_write(intf_s, png_magic, sizeof(png_magic)) != sizeof(png_magic))
      GOTO_END_ERROR();

   ihdr.width      = width;
   ihdr.height     = height;
   ihdr.depth      = 8;
   ihdr.color_type = bpp == sizeof(uint32_t) ? 6 : 2; /* RGBA or RGB */
   if (!png_write_ihdr_string(intf_s, &ihdr))
      GOTO_END_ERROR();

   /* Per-row scratch.  ~width*bpp each -- trivial compared to the
    * frame-sized encode_buf the old full-buffer path allocated. */
   prev_encoded   = (uint8_t*)calloc(1, line_len);
   rgba_line      = (uint8_t*)malloc(line_len);
   up_filtered    = (uint8_t*)malloc(line_len);
   sub_filtered   = (uint8_t*)malloc(line_len);
   avg_filtered   = (uint8_t*)malloc(line_len);
   paeth_filtered = (uint8_t*)malloc(line_len);
   filter_line    = (uint8_t*)malloc(line_len + 1);
   chunk_buf      = (uint8_t*)malloc(IDAT_CHUNK_SIZE + 8);
   if (!prev_encoded || !rgba_line || !up_filtered || !sub_filtered
         || !avg_filtered || !paeth_filtered || !filter_line
         || !chunk_buf)
      GOTO_END_ERROR();

   stream = stream_backend->stream_new();
   if (!stream)
      GOTO_END_ERROR();

   /* Point deflate's output at our chunk staging area (after the
    * 8-byte chunk header).  We re-point it every time we flush
    * a chunk so the driver doesn't need to know the chunk layout. */
   stream_backend->set_out(stream,
         chunk_buf + 8, (uint32_t)IDAT_CHUNK_SIZE);

   for (h = 0; h < height; h++, data += pitch)
   {
      uint32_t rd, wn;
      uint8_t filter;
      unsigned none_score, up_score, sub_score, avg_score, paeth_score;
      unsigned min_sad;
      const uint8_t *chosen_filtered;

      if (bpp == sizeof(uint32_t))
         copy_argb_line(rgba_line, (const uint32_t*)data, width);
      else
         copy_bgr24_line(rgba_line, data, width);

      /* Filter selection unchanged from the previous implementation:
       * try every filter, pick the one with lowest sum-of-abs-deviation. */
      none_score  = count_sad(rgba_line, line_len);
      up_score    = filter_up   (up_filtered,    rgba_line, prev_encoded, width, bpp);
      sub_score   = filter_sub  (sub_filtered,   rgba_line,               width, bpp);
      avg_score   = filter_avg  (avg_filtered,   rgba_line, prev_encoded, width, bpp);
      paeth_score = filter_paeth(paeth_filtered, rgba_line, prev_encoded, width, bpp);

      filter          = 0;
      min_sad         = none_score;
      chosen_filtered = rgba_line;
      if (sub_score < min_sad)   { filter = 1; chosen_filtered = sub_filtered;   min_sad = sub_score;   }
      if (up_score < min_sad)    { filter = 2; chosen_filtered = up_filtered;    min_sad = up_score;    }
      if (avg_score < min_sad)   { filter = 3; chosen_filtered = avg_filtered;   min_sad = avg_score;   }
      if (paeth_score < min_sad) { filter = 4; chosen_filtered = paeth_filtered;                        }

      filter_line[0] = filter;
      memcpy(filter_line + 1, chosen_filtered, line_len);
      memcpy(prev_encoded,    rgba_line,       line_len);

      /* Feed this row into deflate. The loop handles the case where
       * our chunk buffer fills mid-row (BUFFER_FULL): flush IDAT,
       * point deflate at a fresh output buffer, and keep going.
       *
       * When trans() returns success with err=AGAIN, zlib has
       * consumed what we gave it but hasn't finalized (no Z_FINISH
       * was requested) -- that's the normal "ok, send more data
       * next time" signal.  We break out and feed the next row. */
      stream_backend->set_in(stream, filter_line, (uint32_t)(line_len + 1));
      for (;;)
      {
         bool ok = stream_backend->trans(stream, false, &rd, &wn, &err);
         chunk_fill += wn;

         if (ok)
         {
            /* All input consumed.  If the output buffer also happens
             * to be exactly full (avail_in=0 AND avail_out=0 on the
             * same call, which the trans API reports as success
             * with AGAIN rather than BUFFER_FULL), flush proactively
             * -- otherwise the next row's trans() would find
             * avail_out=0 and error out. */
            if (chunk_fill >= IDAT_CHUNK_SIZE)
            {
               if (!flush_idat_chunk(intf_s, chunk_buf, chunk_fill))
                  GOTO_END_ERROR();
               chunk_fill = 0;
               stream_backend->set_out(stream,
                     chunk_buf + 8, (uint32_t)IDAT_CHUNK_SIZE);
            }
            break;
         }

         if (err != TRANS_STREAM_ERROR_BUFFER_FULL)
            GOTO_END_ERROR();

         /* Output filled mid-row.  chunk_fill should equal
          * IDAT_CHUNK_SIZE.  Flush and re-point. */
         if (!flush_idat_chunk(intf_s, chunk_buf, chunk_fill))
            GOTO_END_ERROR();
         chunk_fill = 0;
         stream_backend->set_out(stream,
               chunk_buf + 8, (uint32_t)IDAT_CHUNK_SIZE);
      }
   }

   /* All rows consumed.  Drain deflate with Z_FINISH, emitting IDATs
    * on BUFFER_FULL, final partial on NONE (Z_STREAM_END). */
   stream_backend->set_in(stream, NULL, 0);
   for (;;)
   {
      uint32_t rd = 0, wn = 0;
      bool ok = stream_backend->trans(stream, true, &rd, &wn, &err);
      chunk_fill += wn;

      if (!ok)
      {
         /* BUFFER_FULL during flush-drain with avail_in=0 shouldn't
          * strictly be reachable, but handle defensively. */
         if (err != TRANS_STREAM_ERROR_BUFFER_FULL)
            GOTO_END_ERROR();
         if (!flush_idat_chunk(intf_s, chunk_buf, chunk_fill))
            GOTO_END_ERROR();
         chunk_fill = 0;
         stream_backend->set_out(stream,
               chunk_buf + 8, (uint32_t)IDAT_CHUNK_SIZE);
         continue;
      }
      if (err == TRANS_STREAM_ERROR_AGAIN)
      {
         /* Z_OK during Z_FINISH with avail_in=0 means deflate has
          * more output to emit but our buffer ran out of space.
          * Flush the full chunk and give it more room. */
         if (!flush_idat_chunk(intf_s, chunk_buf, chunk_fill))
            GOTO_END_ERROR();
         chunk_fill = 0;
         stream_backend->set_out(stream,
               chunk_buf + 8, (uint32_t)IDAT_CHUNK_SIZE);
         continue;
      }
      /* err == NONE: Z_STREAM_END.  Flush whatever's in the buffer
       * and we're done.  flush_idat_chunk tolerates chunk_fill==0. */
      if (!flush_idat_chunk(intf_s, chunk_buf, chunk_fill))
         GOTO_END_ERROR();
      break;
   }

   if (!png_write_iend_string(intf_s))
      GOTO_END_ERROR();

end:
   free(rgba_line);
   free(prev_encoded);
   free(up_filtered);
   free(sub_filtered);
   free(avg_filtered);
   free(paeth_filtered);
   free(filter_line);
   free(chunk_buf);

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
   bool ret             = false;
   intfstream_t *intf_s = NULL;
   size_t _len          = (size_t)(width * height * 3 * DEFLATE_PADDING) + PNG_ROUGH_HEADER;
   uint8_t *buf         = (uint8_t*)malloc(_len * sizeof(uint8_t));
   if (!buf)
      GOTO_END_ERROR();

   intf_s = intfstream_open_memory(buf,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         _len);

   ret    = rpng_save_image_stream((const uint8_t*)data,
            intf_s, width, height, pitch, 3);
   *bytes = intfstream_get_ptr(intf_s);

   /* Trim the buffer to the actual written size instead of
    * allocating a second buffer and copying. */
   if (ret && *bytes > 0)
   {
      uint8_t *trimmed = (uint8_t*)realloc(buf, (size_t)*bytes);
      if (trimmed)
         buf = trimmed;
      /* If realloc fails, the original (oversized) buf is still valid */
   }

end:
   if (intf_s)
   {
      intfstream_close(intf_s);
      free(intf_s);
   }
   if (!ret)
   {
      if (buf)
         free(buf);
      return NULL;
   }
   return buf;
}

