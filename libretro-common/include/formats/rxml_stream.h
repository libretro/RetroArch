/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rxml_stream.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RXML_STREAM_H__
#define __LIBRETRO_SDK_FORMAT_RXML_STREAM_H__

/* Opt-in file I/O adapter for rxml.
 *
 * The codec itself (rxml.c / rxml.h) performs no file I/O and
 * imposes none: documents parse from caller-supplied buffers
 * (rxml_load_document_string / rxml_load_document_owned and the
 * incremental rxml_parse_* API).  How the bytes are read is the
 * caller's decision.
 *
 * This header is that decision made for filestream.  Including it
 * couples the including translation unit (and only it) to
 * filestream; callers with their own I/O read the file themselves
 * and hand the buffer to rxml_load_document_owned. */

#include <stdlib.h>
#include <stdint.h>

#include <retro_inline.h>

#include <formats/rxml.h>
#include <streams/file_stream.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Read @path whole through filestream and parse it, handing the
 * buffer to the document.  Returns NULL when the file cannot be
 * opened or read, its size does not fit this platform, allocation
 * fails, or the document is malformed. */
static INLINE rxml_document_t *rxml_load_document_filestream(
      const char *path)
{
   char *memory_buffer     = NULL;
   int64_t len             = 0;
   RFILE *file             = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return NULL;

   len                     = filestream_get_size(file);
   /* filestream_get_size returns -1 on error.  Unchecked, this
    * flows through (size_t)(len + 1) as malloc(0) on 64-bit
    * (returning a tiny non-NULL block) or as a wrapped value on
    * 32-bit; either way memory_buffer[len] = '\0' writes far
    * out-of-bounds.  Reject negative sizes and any size that
    * would not fit in size_t on this platform. */
   if (len < 0 || (uint64_t)len >= (uint64_t)((size_t)-1))
      goto error;
   memory_buffer           = (char*)malloc((size_t)(len + 1));
   if (!memory_buffer)
      goto error;

   memory_buffer[len]      = '\0';
   if (filestream_read(file, memory_buffer, len) != len)
      goto error;

   filestream_close(file);
   file                    = NULL;

   /* The document takes the buffer: the tree points into it, and
    * the parse frees it on failure. */
   return rxml_load_document_owned(memory_buffer, (size_t)len);

error:
   free(memory_buffer);
   if (file)
      filestream_close(file);
   return NULL;
}

RETRO_END_DECLS

#endif
