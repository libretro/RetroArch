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

/* Path-based convenience adapter for the pure BMP encoder in
 * rbmp_encode.c.  This TU is the only part of the rbmp encode surface
 * that touches the filesystem; the encoder itself only speaks buffers
 * and intfstreams.
 *
 * Deprecated: new callers should prefer rbmp_save_image_string() plus a
 * single filestream_write_file() at the edge (one open/bulk-write/close
 * instead of a write per row), or open an intfstream themselves and use
 * rbmp_save_image_stream(). */

#include <stdlib.h>

#include <libretro.h>
#include <streams/interface_stream.h>
#include <formats/rbmp.h>

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
