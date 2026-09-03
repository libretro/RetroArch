/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rbmp.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RBMP_H__
#define __LIBRETRO_SDK_FORMAT_RBMP_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

#include <boolean.h>
#include <streams/interface_stream.h>

RETRO_BEGIN_DECLS

enum rbmp_source_type
{
   RBMP_SOURCE_TYPE_DONT_CARE,
   RBMP_SOURCE_TYPE_BGR24,
   RBMP_SOURCE_TYPE_XRGB888,
   RBMP_SOURCE_TYPE_RGB565,
   RBMP_SOURCE_TYPE_ARGB8888
};

typedef struct rbmp rbmp_t;

/* Pure encode: returns a heap buffer holding the complete BMP file and
 * stores its exact size in *out_len.  BMP output size is fixed
 * (54 + ((width * bpp + 3) & ~3) * height) so the buffer is allocated
 * exactly once and never trimmed.  Caller frees.  Returns NULL on
 * allocation failure or invalid arguments.  No file I/O is performed;
 * pair with e.g. filestream_write_file() to save to disk with a single
 * bulk write. */
uint8_t *rbmp_save_image_string(
      const void *frame,
      unsigned width,
      unsigned height,
      unsigned pitch,
      enum rbmp_source_type type,
      size_t *out_len);

/* Row primitives, implemented (like rbmp_save_image_string) in the
 * pure encoder TU formats/bmp/rbmp_encode.c, which has no stream or
 * VFS dependency of any kind.
 *
 * rbmp_row_size(): padded on-disk size of one BMP row for the given
 * source type ((width * bpp + 3) & ~3).
 *
 * rbmp_encode_row(): produce one padded output row from @row.  @line
 * must hold at least rbmp_row_size() bytes of scratch.  Returns @line
 * when the row was converted/padded into the scratch, or @row itself
 * when the source bytes can be emitted as-is (letting callers skip a
 * copy); NULL on invalid arguments.  @pitch follows the historical
 * unsigned-carrying-a-signed convention of the other entry points and
 * is only consulted for BGR24 payload bounding. */
size_t rbmp_row_size(unsigned width, enum rbmp_source_type type);

const uint8_t *rbmp_encode_row(uint8_t *line, const void *row,
      unsigned width, unsigned pitch, enum rbmp_source_type type);

/* Encode into an already-open intfstream (file, memory, or custom).
 * Writes the 54-byte header followed by one padded row per line; peak
 * extra memory is one row.  Returns false on short writes.
 * Implemented in file/rbmp_file.c over the row primitives above, so
 * the encoder TU itself stays stream-free. */
bool rbmp_save_image_stream(
      intfstream_t *intf_s,
      const void *frame,
      unsigned width,
      unsigned height,
      unsigned pitch,
      enum rbmp_source_type type);

/* Deprecated path-based convenience wrapper (open + stream encode +
 * close), implemented in file/rbmp_file.c so the pure encoder TU carries no
 * filesystem dependency.  Prefer rbmp_save_image_string() +
 * filestream_write_file(), or rbmp_save_image_stream(). */
bool rbmp_save_image(
      const char *filename,
      const void *frame,
      unsigned width,
      unsigned height,
      unsigned pitch,
      enum rbmp_source_type type);

int rbmp_process_image(rbmp_t *rbmp, void **buf,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba);

void form_bmp_header(uint8_t *header,
      unsigned width, unsigned height,
      bool is32bpp);

bool rbmp_set_buf_ptr(rbmp_t *rbmp, void *data);

void rbmp_free(rbmp_t *rbmp);

rbmp_t *rbmp_alloc(void);

RETRO_END_DECLS

#endif
