/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rbmp_file.c).
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

/* Stream and path adapters for the pure BMP encoder in
 * formats/bmp/rbmp_encode.c.  This TU holds every part of the rbmp
 * encode surface that touches a stream or the filesystem; the encoder
 * itself is buffer-only (rbmp_save_image_string / rbmp_row_size /
 * rbmp_encode_row) with no stream or VFS dependency at all.
 *
 * rbmp_save_image() is deprecated: new callers should prefer
 * rbmp_save_image_string() plus a single filestream_write_file() at
 * the edge (one open/bulk-write/close instead of a write per row), or
 * open an intfstream themselves and use rbmp_save_image_stream(). */

#include <stdlib.h>

#include <libretro.h>
#include <streams/interface_stream.h>
#include <formats/rbmp.h>

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

   line_size = rbmp_row_size(width, type);

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
      const uint8_t *row = rbmp_encode_row(line, src, width,
            pitch, type);
      if (intfstream_write(intf_s, row, line_size) != (int64_t)line_size)
      {
         ret = false;
         break;
      }
   }

   free(line);
   return ret;
}

bool rbmp_save_image(
      const char *filename,
      const void *frame,
      unsigned width, unsigned height,
      unsigned pitch, enum rbmp_source_type type)
{
   bool ret             = false;
   intfstream_t *intf_s = intfstream_open_file(filename,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!intf_s)
      return false;

   ret = rbmp_save_image_stream(intf_s, frame,
         width, height, pitch, type);

   intfstream_close(intf_s);
   free(intf_s);

   return ret;
}
