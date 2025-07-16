/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmsgpack.h).
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

#ifndef __LIBRETRODB_MSGPACK_H__
#define __LIBRETRODB_MSGPACK_H__

#include <stdint.h>

#include <streams/interface_stream.h>

struct rmsgpack_read_callbacks
{
   int (*read_nil        )(void *);
   int (*read_bool       )(int, void *);
   int (*read_int        )(int64_t, void *);
   int (*read_uint       )(uint64_t, void *);
   int (*read_string     )(char *, uint32_t, void *);
   int (*read_bin        )(void *, uint32_t, void *);
   int (*read_map_start  )(uint32_t, void *);
   int (*read_array_start)(uint32_t, void *);
};

int rmsgpack_write_array_header(intfstream_t *stream, uint32_t size);

int rmsgpack_write_map_header(intfstream_t *stream, uint32_t size);

int rmsgpack_write_string(intfstream_t *stream, const char *s, uint32_t len);

int rmsgpack_write_bin(intfstream_t *stream, const void *s, uint32_t len);

int rmsgpack_write_nil(intfstream_t *stream);

int rmsgpack_write_bool(intfstream_t *stream, int value);

int rmsgpack_write_int(intfstream_t *stream, int64_t value);

int rmsgpack_write_uint(intfstream_t *stream, uint64_t value);

int rmsgpack_read(intfstream_t *stream, struct rmsgpack_read_callbacks *callbacks, void *data);

#endif
