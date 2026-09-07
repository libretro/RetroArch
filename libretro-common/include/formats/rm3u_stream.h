/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rm3u_stream.h).
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_FORMAT_RM3U_STREAM_H__
#define __LIBRETRO_SDK_FORMAT_RM3U_STREAM_H__

/* Opt-in file I/O adapters for rm3u.
 *
 * The codec itself (rm3u.c / rm3u.h) performs no file I/O and
 * imposes none: contents parse from a caller-supplied buffer and
 * render to a caller-owned string.  This header is the filestream
 * wiring, reproducing the behaviour of the old built-in load /
 * save / is-m3u entry points with fewer filesystem operations:
 *
 *  - rm3u_is_m3u_filestream() answers "extension + exists +
 *    non-empty" with one stat where the old check used two.
 *  - rm3u_save_filestream() writes the whole rendered file in one
 *    write where the old writer issued one or two per entry -
 *    a real difference through network VFS backends, where every
 *    operation is a round trip.
 *
 * Including this header couples the including translation unit
 * (and only it) to filestream; callers with their own I/O read and
 * write the bytes themselves around rm3u_parse / rm3u_dump. */

#include <stdlib.h>
#include <stdint.h>

#include <retro_inline.h>
#include <boolean.h>

#include <file/file_path.h>
#include <formats/rm3u.h>
#include <streams/file_stream.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* True when @path names an M3U file by extension AND a file of
 * non-zero size exists there.  The old semantics of the codec's
 * is-m3u check, in a single stat: a missing file has no size, so
 * one path_get_size() answers existence and emptiness at once. */
static INLINE bool rm3u_is_m3u_filestream(const char *path)
{
   if (!rm3u_is_m3u(path))
      return false;
   return path_get_size(path) > 0;
}

/* Creates an M3U handle for @path and parses the file's contents,
 * reproducing the old loading behaviour exactly:
 * - @path must carry the M3U extension, else NULL
 * - a missing file yields an empty handle (success)
 * - a read or parse failure yields NULL
 * - Returned rm3u_t object must be free'd using rm3u_free() */
static INLINE rm3u_t *rm3u_load_filestream(const char *path)
{
   rm3u_t *m3u       = NULL;
   uint8_t *file_buf = NULL;
   int64_t file_len  = 0;
   bool ok           = false;

   if (!(m3u = rm3u_init(path)))
      return NULL;

   /* Checks run against the handle's canonicalised path, as the
    * old loader's did */
   if (!rm3u_is_m3u(rm3u_get_path(m3u)))
   {
      rm3u_free(m3u);
      return NULL;
   }

   /* If file does not exist, no action is required */
   if (!path_is_valid(rm3u_get_path(m3u)))
      return m3u;

   if (filestream_read_file(rm3u_get_path(m3u),
         (void**)&file_buf, &file_len) < 0)
   {
      rm3u_free(m3u);
      return NULL;
   }

   /* filestream_read_file NUL-terminates */
   ok = rm3u_parse(m3u,
         (file_len > 0) ? (const char*)file_buf : NULL);

   if (file_buf)
      free(file_buf);

   if (!ok)
   {
      rm3u_free(m3u);
      return NULL;
   }
   return m3u;
}

/* Renders and writes the M3U file to its path in a single write.
 * - Returns false in the event of an error */
static INLINE bool rm3u_save_filestream(rm3u_t *m3u,
      enum rm3u_label_type label_type)
{
   RFILE *file   = NULL;
   size_t _len   = 0;
   int64_t wrote = 0;
   char *data    = rm3u_dump(m3u, label_type, &_len);

   if (!data)
      return false;

   if (!(file = filestream_open(rm3u_get_path(m3u),
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      free(data);
      return false;
   }

   wrote = filestream_write(file, data, (int64_t)_len);
   filestream_close(file);
   free(data);

   return (wrote == (int64_t)_len);
}

RETRO_END_DECLS

#endif
