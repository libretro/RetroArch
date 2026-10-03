/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng_file.c).
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

/* Path-based convenience adapters for the pure PNG encoder in
 * rpng_encode.c.  This TU is the only part of the rpng encode surface
 * that opens a path; the encoder itself only speaks intfstreams and
 * memory buffers.
 *
 * Deprecated: new callers should prefer the *_string entry points plus
 * a single filestream_write_file() at the edge (one open/bulk-write/
 * close instead of interleaving many small chunk writes with the
 * compressor), or open an intfstream themselves and call
 * rpng_save_image_stream_fmt() / rpng_save_image_stream(). */

#include <stdlib.h>

#include <libretro.h>
#include <streams/interface_stream.h>
#include <formats/rpng.h>

static bool rpng_save_image_file(const char *path, const uint8_t *data,
      unsigned width, unsigned height, signed pitch,
      enum rpng_pixfmt fmt, const struct rpng_hdr_metadata *hdr)
{
   bool ret             = false;
   intfstream_t *intf_s = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!intf_s)
      return false;

   ret = rpng_save_image_stream_fmt(data, intf_s,
         width, height, pitch, fmt, hdr);

   intfstream_close(intf_s);
   free(intf_s);
   return ret;
}

/* Straight RGBA byte order - R,G,B,A ascending in memory, which on a
 * little-endian host is the uint32_t 0xAABBGGRR, the mirror of what
 * rpng_save_image_argb() takes.  Callers holding GL_RGBA / VK_FORMAT_
 * R8G8B8A8 surfaces would otherwise have to swizzle a whole frame into a
 * scratch buffer just to have the encoder swizzle it back. */
bool rpng_save_image_rgba(const char *path, const uint8_t *data,
      unsigned width, unsigned height, unsigned pitch)
{
   return rpng_save_image_file(path, data,
         width, height, (signed)pitch, RPNG_PIXFMT_RGBA32, NULL);
}

bool rpng_save_image_argb(const char *path, const uint32_t *data,
      unsigned width, unsigned height, unsigned pitch)
{
   return rpng_save_image_file(path, (const uint8_t*)data,
         width, height, (signed)pitch, RPNG_PIXFMT_ARGB32, NULL);
}

bool rpng_save_image_bgr24(const char *path, const uint8_t *data,
      unsigned width, unsigned height, unsigned pitch)
{
   return rpng_save_image_file(path, data,
         width, height, (signed)pitch, RPNG_PIXFMT_BGR24, NULL);
}

bool rpng_save_image_rgb48_hdr(const char *path, const uint16_t *data,
      unsigned width, unsigned height, unsigned pitch,
      const struct rpng_hdr_metadata *hdr)
{
   return rpng_save_image_file(path, (const uint8_t*)data,
         width, height, (signed)pitch, RPNG_PIXFMT_RGB48, hdr);
}
