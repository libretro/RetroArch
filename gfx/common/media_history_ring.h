/* Logical history entries share immutable, uniquely owned GPU snapshots.
 * Storage is the tail of the owner's existing history-pointer allocation. */
#ifndef GFX_MEDIA_HISTORY_RING_H
#define GFX_MEDIA_HISTORY_RING_H

#include <stddef.h>
#include <boolean.h>
#include <retro_inline.h>

typedef struct gfx_media_history_ring
{
   unsigned *order, *refs;
   unsigned count, head, latest;
   bool valid;
} gfx_media_history_ring_t;

static INLINE void gfx_media_history_ring_init(gfx_media_history_ring_t *r,
      unsigned *storage, unsigned count)
{
   unsigned i;
   r->order = storage;
   r->refs  = storage ? storage + count : NULL;
   r->count = count;
   r->head = r->latest = 0;
   r->valid = false;
   for (i = 0; i < count; i++)
   {
      r->order[i] = i;
      r->refs[i] = 1;
   }
}

static INLINE unsigned gfx_media_history_ring_get(
      const gfx_media_history_ring_t *r, unsigned index)
{
   return r->order[(r->head + index) % r->count];
}

static INLINE unsigned gfx_media_history_ring_target(
      const gfx_media_history_ring_t *r, bool repeated)
{
   unsigned i, oldest = gfx_media_history_ring_get(r, r->count - 1);
   if (repeated && r->valid)
      return r->latest;
   if (r->refs[oldest] == 1)
      return oldest;
   /* An aliased oldest entry leaves at least one unreferenced target. */
   for (i = 0; i < r->count - 1 && r->refs[i]; i++) {}
   return i;
}

static INLINE void gfx_media_history_ring_advance(gfx_media_history_ring_t *r,
      unsigned target)
{
   r->refs[gfx_media_history_ring_get(r, r->count - 1)]--;
   r->head = r->head ? r->head - 1 : r->count - 1;
   r->order[r->head] = target;
   r->refs[target]++;
   r->latest = target;
   r->valid = true;
}

#endif
