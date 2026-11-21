/* Per-texture viewer history, used only by its render owner after publication.
 * A newly allocated texture starts with no snapshots, even if an old handle
 * or chain address is reused. Producers reset copies when uploading pixels. */
#ifndef GFX_MEDIA_HISTORY_H
#define GFX_MEDIA_HISTORY_H

#include <stdint.h>
#include <boolean.h>
#include <retro_inline.h>

typedef struct gfx_media_history
{
   uintptr_t chain;
   unsigned copies;
   bool managed;
} gfx_media_history_t;

static INLINE void gfx_media_history_begin(gfx_media_history_t *h,
      uintptr_t chain, bool same_source, bool ready)
{
   if (!h->managed || h->chain != chain || !same_source || !ready)
      h->copies = 0;
   h->chain = chain;
}

static INLINE bool gfx_media_history_needed(const gfx_media_history_t *h,
      unsigned slots)
{
   return !h || !h->managed || h->copies < slots;
}

static INLINE void gfx_media_history_written(gfx_media_history_t *h)
{
   if (h && h->managed)
      h->copies++;
}

#endif
