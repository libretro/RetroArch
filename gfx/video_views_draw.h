/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* The views compositor, instantiated by each driver that draws views:
 * which chain draws each view, the placement loop, and the lifetime of
 * the canvas and UI targets. Written once; the driver's own types and
 * calls are bound in at compile time, so the result is what the driver
 * would have written by hand - no function pointers, no copies.
 *
 * Define before including, then include once per driver:
 *   VV_FN(name)                     the driver's prefix for name
 *   VV_DRV                          the driver's context type
 *   VV_CHAIN                        the driver's shader chain type
 *   VV_CREATE_STOCK(drv)            a view's stock chain, or NULL
 *   VV_SYNC_PARAMS(drv, chain)      the live preset's parameters into
 *                                   @chain's: edits land in the main chain
 *   VV_DRAW                         what the draws below read for one
 *                                   frame, passed through as @ctx
 *   VV_DIRECT(drv, ctx, view, dst, cut)
 *                                   view @view straight from the frame
 *                                   over @dst, cut to @cut
 *   VV_PASS(drv, ctx, chain, dst, cut, again)
 *                                   @chain's final pass over @dst, cut to
 *                                   @cut; @again for a view placed a
 *                                   second time, which swaps no feedback
 *   VV_TARGET                       the driver's target type
 *   VV_TARGET_MAKE(drv, t, dims)    @t made at @dims; false if it can't be
 *   VV_TARGET_RETIRE(drv, t)        @t let go; false if not yet
 *   VV_ROWS_UP                      defined where a target's rows can
 *                                   count from its bottom: draw_placements
 *                                   then takes rows_up
 * Rectangles are top-left video_views_rect_t's. The macros are undefined
 * again at the end. */

#include <string.h>

#include "video_views.h"
#include "video_defines.h"

/* For each view drawn into a target of size @dims (@all_areas false:
 * the left eye's area only), its chain in @chains and the rectangle its
 * first draw uses in @rects, which its offscreen passes are sized for;
 * NULL for a view not drawn. Stock chains are made on first use; preset
 * chains only by the driver. False when the views can't be drawn. */
static bool VV_FN(pick_chains)(VV_DRV *drv, video_views_chains_t *set,
      const video_views_map_t *map, const video_views_layout_t *layout,
      bool stock, bool all_areas, unsigned dims, VV_CHAIN **chains,
      video_views_rect_t *rects)
{
   unsigned i;
   void **pick = stock ? set->stock : set->preset;

   for (i = 0; i < RETRO_VIDEO_VIEWS_MAX; i++)
      chains[i] = NULL;
   for (i = 0; i < map->num_views; i++)
   {
      if (!video_views_first_drawn(layout, i, all_areas, dims, &rects[i]))
         continue;
      /* A threaded frame queued before the count shrank: the driver
       * keeps no chain past the count. */
      if (i >= set->count)
         return false;
      if (!pick[i] && stock && !set->stock_failed)
      {
         pick[i]           = VV_CREATE_STOCK(drv);
         set->stock_failed = !pick[i];
      }
      if (!pick[i])
         return false;
      chains[i] = (VV_CHAIN*)pick[i];
      if (!stock)
         VV_SYNC_PARAMS(drv, chains[i]);
   }
   return true;
}

/* Draws @layout's placements into a target of size @dims, with the
 * @all_areas and @dims the chains were picked with: with @direct each
 * straight from the frame, else through its view's chain. @rows_up: the
 * target's rows count from its bottom, where the interlaced blend of a
 * canvas of even height would take the eyes the wrong way round. */
static void VV_FN(draw_placements)(VV_DRV *drv, const VV_DRAW *ctx,
      const video_views_layout_t *layout, VV_CHAIN *const *chains,
      bool all_areas, unsigned dims, bool direct
#ifdef VV_ROWS_UP
      , bool rows_up
#endif
      )
{
   unsigned i;
   bool drawn[RETRO_VIDEO_VIEWS_MAX];
#ifdef VV_ROWS_UP
   /* With an even height the bottom row is an odd one from the top, so
    * the eyes change halves. */
   int shift = (rows_up
         && layout->stereo_mode == VIDEO_STEREO_MODE_INTERLACED
         && !(VIDEO_SCALE_H(dims) & 1))
      ? (int)(VIDEO_SCALE_W(dims) / 2) : 0;
#endif

   memset(drawn, 0, sizeof(drawn));
   for (i = 0; i < layout->num_placements; i++)
   {
      video_views_rect_t dst, cut;
      const video_views_placement_t *p = &layout->placements[i];
      if (     (!all_areas && p->area != 0)
            || !chains[p->view]
            || !video_views_visible(layout, p, dims, &cut))
         continue;
      dst = p->dst;
#ifdef VV_ROWS_UP
      if (shift)
      {
         int move = (p->area == 0) ? shift : -shift;
         VIDEO_POS_PUT_X(dst.pos, VIDEO_POS_X(dst.pos) + move);
         VIDEO_POS_PUT_X(cut.pos, VIDEO_POS_X(cut.pos) + move);
      }
#endif
      if (direct)
      {
         VV_DIRECT(drv, ctx, p->view, &dst, &cut);
         continue;
      }
      VV_PASS(drv, ctx, chains[p->view], &dst, &cut, drawn[p->view]);
      drawn[p->view] = true;
   }
}

/* @target at @dims, made or remade as the size changes. */
static bool VV_FN(target_ensure)(VV_DRV *drv, video_views_target_t *t,
      VV_TARGET *target, unsigned dims)
{
   if (t->dims == dims)
      return t->made;
   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return false;
   if (t->made && !VV_TARGET_RETIRE(drv, target))
      return false;
   t->dims = dims;
   t->made = VV_TARGET_MAKE(drv, target, dims);
   return t->made;
}

/* @target let go, so the next ensure makes it again. */
static void VV_FN(target_drop)(VV_DRV *drv, video_views_target_t *t,
      VV_TARGET *target)
{
   if (t->made && !VV_TARGET_RETIRE(drv, target))
      return;
   t->dims = 0;
   t->made = false;
}

#undef VV_FN
#undef VV_DRV
#undef VV_CHAIN
#undef VV_CREATE_STOCK
#undef VV_SYNC_PARAMS
#undef VV_DRAW
#undef VV_DIRECT
#undef VV_PASS
#undef VV_TARGET
#undef VV_TARGET_MAKE
#undef VV_TARGET_RETIRE
#undef VV_ROWS_UP
