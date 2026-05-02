/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_spsc.c).
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

#include <stdlib.h>
#include <string.h>
#include <retro_spsc.h>

/* Round @v up to the next power of 2.  Returns 0 if @v == 0 or if
 * the next power of 2 would overflow (caller checks @v <= SIZE_MAX/2). */
static size_t spsc_round_up_pow2(size_t v)
{
   size_t r;
   if (v == 0)
      return 0;
   /* If already a power of 2, return as-is. */
   if ((v & (v - 1)) == 0)
      return v;
   /* Round up.  Loop terminates because v <= SIZE_MAX/2 means r
    * cannot overflow. */
   r = 1;
   while (r < v)
      r <<= 1;
   return r;
}

bool retro_spsc_init(retro_spsc_t *q, size_t min_capacity)
{
   size_t cap;

   if (!q || min_capacity == 0 || min_capacity > (SIZE_MAX / 2))
      return false;

   cap = spsc_round_up_pow2(min_capacity);
   if (cap == 0 || cap > (SIZE_MAX / 2))
      return false;

   q->buffer = (uint8_t*)malloc(cap);
   if (!q->buffer)
      return false;

   q->capacity = cap;
   retro_atomic_size_init(&q->head, 0);
   retro_atomic_size_init(&q->tail, 0);
   return true;
}

void retro_spsc_free(retro_spsc_t *q)
{
   if (!q)
      return;
   if (q->buffer)
   {
      free(q->buffer);
      q->buffer = NULL;
   }
   q->capacity = 0;
}

void retro_spsc_clear(retro_spsc_t *q)
{
   if (!q)
      return;
   /* Quiescence is the caller's responsibility (documented).
    * Under that assumption, no other thread is touching head or
    * tail, so plain init is correct here -- and necessary, because
    * plain assignment to a retro_atomic_size_t is illegal under
    * the C11 stdatomic backend. */
   retro_atomic_size_init(&q->head, 0);
   retro_atomic_size_init(&q->tail, 0);
}

size_t retro_spsc_write_avail(const retro_spsc_t *q)
{
   /* Producer query.  We read our own head (which we wrote) and the
    * consumer's tail (which they wrote with a release-store).
    * acquire-load on tail so any reads we do based on the freed
    * space are ordered after the consumer's tail publication.
    *
    * We can read our own head without an acquire because we are the
    * only writer to it; but retro_atomic.h does not expose relaxed
    * loads, so use acquire.  Cost is one extra fence on weak-memory
    * targets, which is negligible compared to the actual work
    * surrounding this query. */
   size_t head = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->head);
   size_t tail = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->tail);
   /* head - tail is well-defined modular arithmetic on size_t.
    * Invariant: 0 <= head - tail <= capacity. */
   return q->capacity - (head - tail);
}

size_t retro_spsc_read_avail(const retro_spsc_t *q)
{
   /* Consumer query.  acquire-load on head pairs with the producer's
    * release-store on head, so the data writes that preceded the
    * release are visible by the time we read them. */
   size_t head = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->head);
   size_t tail = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->tail);
   return head - tail;
}

size_t retro_spsc_write(retro_spsc_t *q, const void *data, size_t bytes)
{
   size_t mask, head_idx, first;
   const uint8_t *src = (const uint8_t*)data;
   /* read tail first to know how much room there is */
   size_t head  = retro_atomic_load_acquire_size(&q->head);
   size_t tail  = retro_atomic_load_acquire_size(&q->tail);
   size_t avail = q->capacity - (head - tail);
   if (bytes > avail)
      bytes = avail;
   if (bytes == 0)
      return 0;

   mask     = q->capacity - 1;
   head_idx = head & mask;

   /* first = bytes from head_idx to end-of-buffer */
   first = q->capacity - head_idx;
   if (first > bytes)
      first = bytes;

   memcpy(q->buffer + head_idx, src, first);
   memcpy(q->buffer, src + first, bytes - first);

   /* Publish: release-store ensures the memcpys above are globally
    * visible before the consumer observes the new head. */
   retro_atomic_store_release_size(&q->head, head + bytes);
   return bytes;
}

size_t retro_spsc_read(retro_spsc_t *q, void *data, size_t bytes)
{
   size_t mask, tail_idx, first;
   uint8_t *dst = (uint8_t*)data;
   /* acquire on head pairs with producer's release-store; this is
    * what makes the subsequent memcpys safe to read. */
   size_t head  = retro_atomic_load_acquire_size(&q->head);
   size_t tail  = retro_atomic_load_acquire_size(&q->tail);
   size_t avail = head - tail;
   if (bytes > avail)
      bytes = avail;
   if (bytes == 0)
      return 0;

   mask     = q->capacity - 1;
   tail_idx = tail & mask;

   first = q->capacity - tail_idx;
   if (first > bytes)
      first = bytes;

   memcpy(dst, q->buffer + tail_idx, first);
   memcpy(dst + first, q->buffer, bytes - first);

   /* Publish: release-store so the producer can re-use this space. */
   retro_atomic_store_release_size(&q->tail, tail + bytes);
   return bytes;
}

size_t retro_spsc_peek(const retro_spsc_t *q, void *data, size_t bytes)
{
   size_t mask, tail_idx, first;
   uint8_t *dst = (uint8_t*)data;
   size_t head  = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->head);
   size_t tail = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->tail);
   size_t avail = head - tail;
   if (bytes > avail)
      bytes = avail;
   if (bytes == 0)
      return 0;

   mask     = q->capacity - 1;
   tail_idx = tail & mask;

   first    = q->capacity - tail_idx;
   if (first > bytes)
      first = bytes;

   memcpy(dst, q->buffer + tail_idx, first);
   memcpy(dst + first, q->buffer, bytes - first);
   /* No tail update: peek does not consume. */
   return bytes;
}
