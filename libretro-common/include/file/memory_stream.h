/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memory_stream.h).
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

#ifndef _LIBRETRO_SDK_FILE_MEMORY_STREAM_H
#define _LIBRETRO_SDK_FILE_MEMORY_STREAM_H

#include <stdint.h>
#include <stddef.h>

typedef struct memstream memstream_t;

memstream_t *memstream_open(void);

void memstream_close(memstream_t * stream);

size_t memstream_read(memstream_t * stream, void *data, size_t bytes);

size_t memstream_write(memstream_t * stream, const void *data, size_t bytes);

int memstream_getc(memstream_t * stream);

char *memstream_gets(memstream_t * stream, char *buffer, size_t len);

size_t memstream_pos(memstream_t * stream);

int memstream_seek(memstream_t * stream, int offset, int whence);

void memstream_set_buffer(uint8_t *buffer, size_t size);

size_t memstream_get_last_size(void);

#endif
