/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fifo_queue.h).
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

#ifndef __LIBRETRO_SDK_FIFO_BUFFER_H
#define __LIBRETRO_SDK_FIFO_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include <retro_common_api.h>
#include <retro_inline.h>

RETRO_BEGIN_DECLS

struct fifo_buffer
{
   uint8_t *buffer;
   size_t size;
   size_t first;
   size_t end;
};

typedef struct fifo_buffer fifo_buffer_t;

fifo_buffer_t *fifo_new(size_t size);

static INLINE void fifo_clear(fifo_buffer_t *buffer)
{
   buffer->first = 0;
   buffer->end   = 0;
}

void fifo_write(fifo_buffer_t *buffer, const void *in_buf, size_t size);

void fifo_read(fifo_buffer_t *buffer, void *in_buf, size_t size);

static INLINE void fifo_free(fifo_buffer_t *buffer)
{
   if (!buffer)
      return;

   free(buffer->buffer);
   free(buffer);
}

static INLINE size_t fifo_read_avail(fifo_buffer_t *buffer)
{
   return (buffer->end + ((buffer->end < buffer->first) ? buffer->size : 0)) - buffer->first;
}

static INLINE size_t fifo_write_avail(fifo_buffer_t *buffer)
{
   return (buffer->size - 1) - ((buffer->end + ((buffer->end < buffer->first) ? buffer->size : 0)) - buffer->first);
}

RETRO_END_DECLS

#endif
