/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rjson_stream.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RJSON_STREAM_H__
#define __LIBRETRO_SDK_FORMAT_RJSON_STREAM_H__

/* Opt-in file I/O adapters for rjson.
 *
 * The codec itself (rjson.c / rjson.h) performs no file I/O and
 * imposes none: parsing and writing run over caller-supplied
 * buffers or caller-supplied read/write callbacks
 * (rjson_open_user / rjsonwriter_open_user).  How bytes reach the
 * parser - and where written bytes go - is the caller's decision.
 *
 * This header is that decision made for the two in-tree stream
 * abstractions.  Including it couples the including translation
 * unit (and only it) to intfstream / filestream; callers with
 * their own I/O simply never include it and wire
 * rjson_open_user / rjsonwriter_open_user themselves. */

#include <retro_inline.h>
#include <boolean.h>

#include <formats/rjson.h>
#include <streams/file_stream.h>
#include <streams/interface_stream.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Reader adapters.  The io block size is picked from the source
 * size the way rjson always did: larger files read through larger
 * blocks. */

static INLINE int rjson_intfstream_io(void *buf, int len, void *user)
{
   return (int)intfstream_read((intfstream_t*)user, buf, (uint64_t)len);
}

static INLINE int rjson_stream_io_size(int64_t size)
{
   return (size > 1024 * 1024 ? 4096 :
          (size >  256 * 1024 ? 2048 : 1024));
}

static INLINE rjson_t *rjson_open_intfstream(intfstream_t *stream)
{
   return rjson_open_user(rjson_intfstream_io, stream,
         rjson_stream_io_size(intfstream_get_size(stream)));
}

static INLINE int rjson_filestream_io(void *buf, int len, void *user)
{
   return (int)filestream_read((RFILE*)user, buf, (int64_t)len);
}

static INLINE rjson_t *rjson_open_filestream(RFILE *file)
{
   return rjson_open_user(rjson_filestream_io, file,
         rjson_stream_io_size(filestream_get_size(file)));
}

/* Writer adapters. */

static INLINE int rjsonwriter_intfstream_io(const void *buf, int len,
      void *user)
{
   return (int)intfstream_write((intfstream_t*)user, buf, (uint64_t)len);
}

static INLINE rjsonwriter_t *rjsonwriter_open_intfstream(
      intfstream_t *stream)
{
   return rjsonwriter_open_user(rjsonwriter_intfstream_io, stream);
}

static INLINE int rjsonwriter_filestream_io(const void *buf, int len,
      void *user)
{
   return (int)filestream_write((RFILE*)user, buf, (int64_t)len);
}

static INLINE rjsonwriter_t *rjsonwriter_open_filestream(RFILE *file)
{
   return rjsonwriter_open_user(rjsonwriter_filestream_io, file);
}

RETRO_END_DECLS

#endif
