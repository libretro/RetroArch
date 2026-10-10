/* surface_outcome_test.c - gfx_surface's direct-video outcomes against
 * the stub driver in stubs_retroarch.c.
 *
 *  1  A dropped in-place update is DROPPED, not DONE, with the frame
 *     still in its slot, and the next submit of it goes through.
 *  2  A surface without slots updates in place, and when the update is
 *     dropped it loads instead: a still has no next frame to make up
 *     for it.
 *  3  Under the wrapper, a still whose update the video thread drops is
 *     posted again from its completion and lands.
 *
 * Built with the unit's flags, since it includes gfx_surface.h. */

#include <stdio.h>
#include <string.h>

#include "../../../gfx/gfx_surface.h"

extern int gt_lend_owned, gt_drop_updates, gt_can_update;
extern int gt_async_mode, gt_async_pending;
extern void gt_async_flush(void);

static int outcome_fails;

static void outcome_check(const char *what, int ok)
{
   printf("%s %s\n", ok ? "[ok]  " : "[FAIL]", what);
   if (!ok)
      outcome_fails++;
}

static void outcome_fill(gfx_surface_t *s, unsigned slot, uint32_t v)
{
   size_t i, n = VIDEO_SCALE_AREA(s->dims);
   for (i = 0; i < n; i++)
      s->slots[slot][i] = v;
}

int gt_surface_outcome_test(void)
{
   const unsigned dims = VIDEO_SCALE_PACK(16, 16);
   gfx_surface_t *s;
   enum gfx_surface_submit_result r;
   uintptr_t first;

   /* distinct handles per load */
   gt_lend_owned   = 1;
   gt_can_update   = 1;
   gt_drop_updates = 0;

   /* 1 */
   if ((s = gfx_surface_new(dims, 2, IMAGE_PIXFMT_8888,
         TEXTURE_FILTER_LINEAR, NULL, NULL)))
   {
      outcome_fill(s, 0, 0x11111111u);
      gfx_surface_submit(s, 0, true);
      outcome_fill(s, 1, 0x22222222u);
      gt_drop_updates = 1;
      r = gfx_surface_submit(s, 1, true);
      outcome_check("a dropped update is DROPPED, its frame kept",
            r == GFX_SURFACE_SUBMIT_DROPPED
            && s->slots[1][0] == 0x22222222u);
      r = gfx_surface_submit(s, 1, true);
      outcome_check("and the same frame goes through on the next submit",
            r == GFX_SURFACE_SUBMIT_DONE);
      gfx_surface_free(s);
   }
   else
      outcome_check("surface made", 0);

   /* 2 */
   if ((s = gfx_surface_new_static(dims, TEXTURE_FILTER_LINEAR)))
   {
      static uint32_t px[16 * 16];
      gfx_surface_src_t src;
      memset(&src, 0, sizeof(src));
      src.pixels      = px;
      src.pixfmt      = IMAGE_PIXFMT_8888;
      src.rgba        = true;
      gfx_surface_submit_external(s, &src, NULL, NULL);
      first           = s->handle;
      r               = gfx_surface_submit_external(s, &src, NULL, NULL);
      outcome_check("a still's second submit updates in place",
            r == GFX_SURFACE_SUBMIT_DONE && s->handle == first);
      gt_drop_updates = 1;
      r               = gfx_surface_submit_external(s, &src, NULL, NULL);
      outcome_check("a still whose update is dropped loads instead",
            r == GFX_SURFACE_SUBMIT_DONE && s->handle && s->handle != first
            && gt_drop_updates == 0);
      gt_drop_updates = 0;
      gfx_surface_free(s);
   }
   else
      outcome_check("static surface made", 0);

   /* 3 */
   gt_async_mode = 1;
   if ((s = gfx_surface_new_static(dims, TEXTURE_FILTER_LINEAR)))
   {
      static uint32_t px[16 * 16];
      gfx_surface_src_t src;
      memset(&src, 0, sizeof(src));
      src.pixels      = px;
      src.pixfmt      = IMAGE_PIXFMT_8888;
      src.rgba        = true;
      gfx_surface_submit_external(s, &src, NULL, NULL);
      gt_async_flush();
      first           = s->handle;
      r               = gfx_surface_submit_external(s, &src, NULL, NULL);
      gt_drop_updates = 1;
      gt_async_flush();
      outcome_check("a still dropped on the video thread is posted again",
            r == GFX_SURFACE_SUBMIT_QUEUED && s->inflight
            && gt_async_pending == 1 && gt_drop_updates == 0);
      gt_async_flush();
      outcome_check("and lands in the same texture",
            !s->inflight && !gt_async_pending && s->handle == first);
      gfx_surface_free(s);
      gt_async_flush();
   }
   else
      outcome_check("static surface made", 0);
   gt_async_mode = 0;

   gt_lend_owned = 0;
   return outcome_fails;
}
