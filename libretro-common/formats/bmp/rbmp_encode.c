/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rbmp_encode.c).
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

#include <stdlib.h>
#include <string.h>

#include <streams/interface_stream.h>
#include <formats/rbmp.h>

/* This TU is a pure encoder: bytes in -> bytes out.  It never opens a
 * path and has no dependency on file_stream.h / the VFS.  The primary
 * entry point is rbmp_save_image_string() (exact-size heap buffer);
 * rbmp_save_image_stream() writes to any already-open intfstream.  The
 * legacy path-based rbmp_save_image() lives in rbmp_file.c as a thin
 * deprecated adapter. */

void form_bmp_header(uint8_t *header,
      unsigned width, unsigned height,
      bool is32bpp)
{
   unsigned line_size  = (width * (is32bpp?4:3) + 3) & ~3;
   unsigned size       = line_size * height + 54;
   unsigned size_array = line_size * height;

   /* Generic BMP stuff. */
   /* signature */
   header[0] = 'B';
   header[1] = 'M';
   /* file size */
   header[2] = (uint8_t)(size >> 0);
   header[3] = (uint8_t)(size >> 8);
   header[4] = (uint8_t)(size >> 16);
   header[5] = (uint8_t)(size >> 24);
   /* reserved */
   header[6] = 0;
   header[7] = 0;
   header[8] = 0;
   header[9] = 0;
   /* offset */
   header[10] = 54;
   header[11] = 0;
   header[12] = 0;
   header[13] = 0;
   /* DIB size */
   header[14] = 40;
   header[15] = 0;
   header[16] = 0;
   header[17] = 0;
   /* Width */
   header[18] = (uint8_t)(width >> 0);
   header[19] = (uint8_t)(width >> 8);
   header[20] = (uint8_t)(width >> 16);
   header[21] = (uint8_t)(width >> 24);
   /* Height */
   header[22] = (uint8_t)(height >> 0);
   header[23] = (uint8_t)(height >> 8);
   header[24] = (uint8_t)(height >> 16);
   header[25] = (uint8_t)(height >> 24);
   /* Color planes */
   header[26] = 1;
   header[27] = 0;
   /* Bits per pixel */
   header[28] = is32bpp ? 32 : 24;
   header[29] = 0;
   /* Compression method */
   header[30] = 0;
   header[31] = 0;
   header[32] = 0;
   header[33] = 0;
   /* Image data size */
   header[34] = (uint8_t)(size_array >> 0);
   header[35] = (uint8_t)(size_array >> 8);
   header[36] = (uint8_t)(size_array >> 16);
   header[37] = (uint8_t)(size_array >> 24);
   /* Horizontal resolution */
   header[38] = 19;
   header[39] = 11;
   header[40] = 0;
   header[41] = 0;
   /* Vertical resolution */
   header[42] = 19;
   header[43] = 11;
   header[44] = 0;
   header[45] = 0;
   /* Palette size */
   header[46] = 0;
   header[47] = 0;
   header[48] = 0;
   header[49] = 0;
   /* Important color count */
   header[50] = 0;
   header[51] = 0;
   header[52] = 0;
   header[53] = 0;
}

static size_t rbmp_line_size(unsigned width, enum rbmp_source_type type)
{
   unsigned bpp = (type == RBMP_SOURCE_TYPE_ARGB8888) ? 4 : 3;
   return ((size_t)width * bpp + 3) & ~(size_t)3;
}

static void dump_line_565_to_24(uint8_t *line, const uint16_t *src, unsigned width)
{
   unsigned i;

   for (i = 0; i < width; i++)
   {
      uint16_t pixel = *src++;
      uint8_t b = (pixel >>  0) & 0x1f;
      uint8_t g = (pixel >>  5) & 0x3f;
      uint8_t r = (pixel >> 11) & 0x1f;
      *line++   = (b << 3) | (b >> 2);
      *line++   = (g << 2) | (g >> 4);
      *line++   = (r << 3) | (r >> 2);
   }
}

static void dump_line_32_to_24(uint8_t *line, const uint32_t *src, unsigned width)
{
   unsigned i;

   for (i = 0; i < width; i++)
   {
      uint32_t pixel = *src++;
      *line++ = (pixel >>  0) & 0xff;
      *line++ = (pixel >>  8) & 0xff;
      *line++ = (pixel >> 16) & 0xff;
   }
}

/* Produce one padded output row.  Returns either @line (row was
 * converted/padded into the scratch buffer) or @src directly when the
 * source row can be emitted as-is, letting callers skip a copy. */
static const uint8_t *rbmp_fill_row(uint8_t *line, const uint8_t *src,
      unsigned width, size_t line_size, unsigned pitch,
      enum rbmp_source_type type)
{
   size_t copy;

   switch (type)
   {
      case RBMP_SOURCE_TYPE_ARGB8888:
         /* ARGB8888 byte order matches the output and a 32bpp row is
          * naturally 4-byte aligned, so no padding exists. */
         return src;
      case RBMP_SOURCE_TYPE_BGR24:
         /* BGR24 byte order matches the output; only row padding may
          * need to be synthesised.  Rows are handed to the writer
          * as-is only when the stride equals the padded row size;
          * otherwise copy exactly the row payload (never the stride,
          * which may be negative or wider than the row) and zero the
          * padding so tightly-allocated source buffers are never read
          * past their final row. */
         if ((int)pitch >= 0 && (size_t)pitch == line_size)
            return src;
         copy = (size_t)width * 3;
         if ((int)pitch >= 0 && (size_t)pitch < copy)
            copy = (size_t)pitch;
         memcpy(line, src, copy);
         memset(line + copy, 0, line_size - copy);
         return line;
      case RBMP_SOURCE_TYPE_XRGB888:
         dump_line_32_to_24(line, (const uint32_t*)(const void*)src, width);
         break;
      case RBMP_SOURCE_TYPE_RGB565:
         dump_line_565_to_24(line, (const uint16_t*)(const void*)src, width);
         break;
      default:
         /* Unknown source type: emit a zeroed (black) row so the file
          * still matches the size declared in its header.  (The old
          * path-based writer emitted a header-only, truncated file
          * here.) */
         memset(line, 0, line_size);
         return line;
   }

   /* Deterministic 0-3 bytes of row padding for the converted formats. */
   memset(line + (size_t)width * 3, 0, line_size - (size_t)width * 3);
   return line;
}

uint8_t *rbmp_save_image_string(
      const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      enum rbmp_source_type type,
      size_t *out_len)
{
   unsigned j;
   size_t line_size;
   size_t _len;
   uint8_t *buf       = NULL;
   uint8_t *dst       = NULL;
   const uint8_t *src = (const uint8_t*)frame;
   /* pitch travels through the public API as unsigned (historical),
    * but callers pass negative strides to walk bottom-up sources
    * top-down; reinterpret it as signed for the row walk, exactly as
    * the old RFILE-based writer did. */
   int s_pitch        = (int)pitch;

   if (!frame || !out_len || !width || !height)
      return NULL;

   line_size = rbmp_line_size(width, type);

   /* BMP output size is exact and fixed:
    * 54-byte header + line_size * height rows. */
   if (line_size > ((size_t)-1 - 54) / height)
      return NULL;
   _len      = 54 + line_size * height;

   if (!(buf = (uint8_t*)malloc(_len)))
      return NULL;

   form_bmp_header(buf, width, height, type == RBMP_SOURCE_TYPE_ARGB8888);

   dst = buf + 54;
   for (j = 0; j < height; j++, src += s_pitch, dst += line_size)
   {
      const uint8_t *row = rbmp_fill_row(dst, src, width,
            line_size, pitch, type);
      if (row != dst)
         memcpy(dst, row, line_size);
   }

   *out_len = _len;
   return buf;
}

bool rbmp_save_image_stream(
      intfstream_t *intf_s,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      enum rbmp_source_type type)
{
   unsigned j;
   uint8_t header[54];
   size_t line_size;
   uint8_t *line      = NULL;
   bool ret           = true;
   const uint8_t *src = (const uint8_t*)frame;
   /* See rbmp_save_image_string regarding the signedness of pitch. */
   int s_pitch        = (int)pitch;

   if (!intf_s || !frame || !width || !height)
      return false;

   line_size = rbmp_line_size(width, type);

   form_bmp_header(header, width, height, type == RBMP_SOURCE_TYPE_ARGB8888);
   if (intfstream_write(intf_s, header, sizeof(header))
         != (int64_t)sizeof(header))
      return false;

   /* Whole-block fast path: when the source stride equals the padded
    * BMP row size and the rows already carry the output byte order
    * (BGR24 with 4-byte-aligned width, or ARGB whose memory layout is
    * BMP's 32bpp layout verbatim), the pixel block on disk is the
    * source buffer byte for byte.  Hand it to the stream in one write:
    * no line buffer, no per-row calls, zero copies on this side. */
   if (     s_pitch > 0
         && (size_t)s_pitch == line_size
         && (   type == RBMP_SOURCE_TYPE_BGR24
             || type == RBMP_SOURCE_TYPE_ARGB8888))
   {
      int64_t block = (int64_t)line_size * height;
      return intfstream_write(intf_s, src, block) == block;
   }

   if (!(line = (uint8_t*)malloc(line_size)))
      return false;

   for (j = 0; j < height; j++, src += s_pitch)
   {
      const uint8_t *row = rbmp_fill_row(line, src, width,
            line_size, pitch, type);
      if (intfstream_write(intf_s, row, line_size) != (int64_t)line_size)
      {
         ret = false;
         break;
      }
   }

   free(line);
   return ret;
}
