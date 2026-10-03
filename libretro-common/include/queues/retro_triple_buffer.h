/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (retro_triple_buffer.h).
 * ---------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_RETRO_TRIPLE_BUFFER_H
#define __LIBRETRO_SDK_RETRO_TRIPLE_BUFFER_H

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>

/* Three buffers between a producer, which fills one with a frame, and
 * a consumer, which reads the newest finished one for as long as it
 * likes. Each side owns one buffer outright; the third waits in the
 * middle, and a single atomic word says which it is and whether it is
 * newer than what the consumer holds. Neither side ever waits for the
 * other: a frame is handed over with one exchange, and the consumer
 * picks up the newest one with another. */

#define RETRO_TRIPLE_BUFFER_FRESH 4

typedef struct retro_triple_buffer
{
   void *buf[3];
   retro_atomic_int_t mid;   /* index of the waiting buffer | FRESH */
   int back;                 /* the producer's */
   int front;                /* the consumer's */
} retro_triple_buffer_t;

static INLINE void retro_triple_buffer_init(retro_triple_buffer_t *f,
      void *a, void *b, void *c)
{
   f->buf[0] = a;
   f->buf[1] = b;
   f->buf[2] = c;
   f->back   = 0;
   f->front  = 2;
   retro_atomic_store_release_int(&f->mid, 1);
}

/* Producer: the buffer the next frame goes into. */
static INLINE void *retro_triple_buffer_back(const retro_triple_buffer_t *f)
{
   return f->buf[f->back];
}

/* Producer: the frame in the back buffer is finished. */
static INLINE void retro_triple_buffer_publish(retro_triple_buffer_t *f)
{
   int old = retro_atomic_exchange_int(&f->mid,
         f->back | RETRO_TRIPLE_BUFFER_FRESH);
   f->back = old & 3;
}

/* Consumer: a frame finished since the last take, the consumer's until
 * its next call, or NULL when none has been. Only the producer sets
 * FRESH and only the consumer clears it, so once it is seen the
 * exchange takes a buffer at least that new. */
static INLINE void *retro_triple_buffer_take(retro_triple_buffer_t *f)
{
   if (!(retro_atomic_load_acquire_int(&f->mid) & RETRO_TRIPLE_BUFFER_FRESH))
      return NULL;
   f->front = retro_atomic_exchange_int(&f->mid, f->front) & 3;
   return f->buf[f->front];
}

/* Consumer: the newest finished frame, the consumer's until its next
 * call. Before the first publish it is the third buffer, as
 * initialised. */
static INLINE void *retro_triple_buffer_front(retro_triple_buffer_t *f)
{
   retro_triple_buffer_take(f);
   return f->buf[f->front];
}

#endif
