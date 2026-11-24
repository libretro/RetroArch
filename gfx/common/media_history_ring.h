/* Logical history entries share immutable, uniquely owned GPU snapshots.
 * Storage belongs to the owner and is allocated with its history or clone. */
#ifndef GFX_MEDIA_HISTORY_RING_H
#define GFX_MEDIA_HISTORY_RING_H

#include <stddef.h>
#include <stdint.h>
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

static INLINE const void *gfx_media_history_ring_data(
      const gfx_media_history_ring_t *r, const void *base, size_t stride,
      const void *data)
{
   if (r && r->count)
   {
      uintptr_t begin = (uintptr_t)base;
      uintptr_t address = (uintptr_t)data;
      if (address >= begin && address - begin < r->count * stride)
      {
         size_t offset = (size_t)(address - begin);
         return (const char*)base
            + gfx_media_history_ring_get(r, (unsigned)(offset / stride)) * stride
            + offset % stride;
      }
   }
   return data;
}

#endif
