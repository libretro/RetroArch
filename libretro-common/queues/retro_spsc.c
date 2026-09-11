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

   q->capacity    = cap;
   q->cached_tail = 0;
   q->cached_head = 0;
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
   q->cached_tail = 0;
   q->cached_head = 0;
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
    * The header says producer-only, but audio_driver.c also calls
    * read_avail from the producer side as a fill-level query, and the
    * same could happen here.  Keep BOTH loads acquire so a cross-side
    * caller still gets a happens-before with the other thread's
    * release; the query functions are not hot enough to justify the
    * relaxed own-counter load used in the data-path functions below. */
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
   /* acquire on tail too: see retro_spsc_write_avail - this is called
    * from the producer side in audio_driver.c as a fill query. */
   size_t tail = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->tail);
   return head - tail;
}

size_t retro_spsc_write(retro_spsc_t *q, const void *data, size_t bytes)
{
   size_t mask, head_idx, first;
   const uint8_t *src = (const uint8_t*)data;
   /* head is ours (relaxed).  Room is first computed from our private
    * copy of tail, which is never ahead of the real one; only when it
    * says there is not enough do we acquire-load the consumer's tail
    * (their cache line), which pairs with their release-store so the
    * freed space is really free. */
   size_t head  = retro_atomic_load_relaxed_size(&q->head);
   size_t avail = q->capacity - (head - q->cached_tail);
   if (avail < bytes)
   {
      q->cached_tail = retro_atomic_load_acquire_size(&q->tail);
      avail          = q->capacity - (head - q->cached_tail);
   }
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
   /* tail is ours (relaxed).  Available from our private copy of head
    * first, never ahead of the real one; only when it says too little
    * do we acquire-load the producer's head, which pairs with their
    * release-store so the bytes are really there. */
   size_t tail  = retro_atomic_load_relaxed_size(&q->tail);
   size_t avail = q->cached_head - tail;
   if (avail < bytes)
   {
      q->cached_head = retro_atomic_load_acquire_size(&q->head);
      avail          = q->cached_head - tail;
   }
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
   /* Consumer side, same scheme as retro_spsc_read; the cast is only
    * because peek takes a const queue and the cached copy is state. */
   retro_spsc_t *w  = (retro_spsc_t*)q;
   size_t tail  = retro_atomic_load_relaxed_size(&w->tail);
   size_t avail = w->cached_head - tail;
   if (avail < bytes)
   {
      w->cached_head = retro_atomic_load_acquire_size(&w->head);
      avail          = w->cached_head - tail;
   }
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

size_t retro_spsc_write_begin(retro_spsc_t *q, void **ptr)
{
   size_t mask, head_idx, span;
   /* head is ours (relaxed).  Room from the private copy of tail
    * first; re-read the consumer's tail only when that says none.
    * See retro_spsc_write. */
   size_t head  = retro_atomic_load_relaxed_size(&q->head);
   size_t avail = q->capacity - (head - q->cached_tail);
   if (avail == 0)
   {
      q->cached_tail = retro_atomic_load_acquire_size(&q->tail);
      avail          = q->capacity - (head - q->cached_tail);
   }

   mask     = q->capacity - 1;
   head_idx = head & mask;
   span     = q->capacity - head_idx;
   if (span > avail)
      span = avail;
   *ptr = q->buffer + head_idx;
   return span;
}

void retro_spsc_write_end(retro_spsc_t *q, size_t bytes)
{
   if (bytes == 0)
      return;
   /* head is written only by the producer, i.e. by this thread, so
    * reading our own last store needs no ordering: a relaxed load is
    * enough (and on the MSVC/Apple/__sync backends the acquire load is
    * a locked RMW, so this also removes a lock-prefixed instruction
    * from every span commit).
    * Release: the caller's stores into the span happen-before the
    * consumer's acquire-load of head, same pairing as retro_spsc_write. */
   retro_atomic_store_release_size(&q->head,
         retro_atomic_load_relaxed_size(&q->head) + bytes);
}

size_t retro_spsc_read_begin(retro_spsc_t *q, const void **ptr)
{
   size_t mask, tail_idx, span;
   /* tail is ours (relaxed).  Available from the private copy of
    * head first; re-read the producer's head only when that says
    * none.  See retro_spsc_read. */
   size_t tail  = retro_atomic_load_relaxed_size(&q->tail);
   size_t avail = q->cached_head - tail;
   if (avail == 0)
   {
      q->cached_head = retro_atomic_load_acquire_size(&q->head);
      avail          = q->cached_head - tail;
   }

   mask     = q->capacity - 1;
   tail_idx = tail & mask;
   span     = q->capacity - tail_idx;
   if (span > avail)
      span = avail;
   *ptr = q->buffer + tail_idx;
   return span;
}

void retro_spsc_read_end(retro_spsc_t *q, size_t bytes)
{
   if (bytes == 0)
      return;
   /* tail is written only by the consumer, i.e. by this thread, so a
    * relaxed load of our own counter suffices; see retro_spsc_write_end.
    * Release: our reads of the span happen-before the producer's
    * acquire-load of tail sees the space as free. */
   retro_atomic_store_release_size(&q->tail,
         retro_atomic_load_relaxed_size(&q->tail) + bytes);
}

size_t retro_spsc_skip(retro_spsc_t *q, size_t bytes)
{
   /* tail is ours (relaxed).  Available from our private copy of head
    * first, never ahead of the real one; only when it says too little
    * do we acquire-load the producer's head, which pairs with their
    * release-store so the bytes are really there. */
   size_t tail  = retro_atomic_load_relaxed_size(&q->tail);
   size_t avail = q->cached_head - tail;
   if (avail < bytes)
   {
      q->cached_head = retro_atomic_load_acquire_size(&q->head);
      avail          = q->cached_head - tail;
   }

   if (bytes > avail)
      bytes = avail;
   if (bytes == 0)
      return 0;

   retro_atomic_store_release_size(&q->tail, tail + bytes);
   return bytes;
}
