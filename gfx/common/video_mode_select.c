/*  RetroArch - A frontend for libretro.
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

#include <stdlib.h>

#include "video_mode_select.h"

int video_mode_find_nearest(void *ctx, video_mode_get_fn get,
      unsigned count, unsigned dims, int int_hz, float hz)
{
   unsigned i;
   int best        = -1;
   float best_diff = 0.0f;
   float want      = hz > 0.0f ? hz : (float)int_hz;

   if (!get || !count)
      return -1;

   /* No rate asked for: the one the display is running */
   if (want <= 0.0f)
      for (i = 0; i < count; i++)
      {
         video_mode_desc_t m;
         if (get(ctx, i, &m) && m.current)
         {
            want = m.refresh;
            break;
         }
      }

   for (i = 0; i < count; i++)
   {
      float diff;
      video_mode_desc_t m;

      if (!get(ctx, i, &m) || m.dims != dims)
         continue;
      diff = m.refresh - want;
      if (diff < 0.0f)
         diff = -diff;
      /* Further off than half a hertz is another mode, unless it is
       * the whole-hertz rate the caller named */
      if (     want > 0.0f
            && diff >= 0.5f
            && !(int_hz > 0 && (int)(m.refresh + 0.001f) == int_hz))
         continue;
      if (m.interlaced)
         diff += 1000.0f;
      if (best < 0 || diff < best_diff || (diff == best_diff && m.current))
      {
         best      = (int)i;
         best_diff = diff;
      }
   }
   return best;
}

static int video_mode_list_cmp(const void *pa, const void *pb)
{
   const video_display_config_t *a = (const video_display_config_t*)pa;
   const video_display_config_t *b = (const video_display_config_t*)pb;
   if (a->dims != b->dims)
      return a->dims < b->dims ? -1 : 1;
   if (a->interlaced != b->interlaced)
      return a->interlaced ? 1 : -1;
   if (a->refreshrate_float != b->refreshrate_float)
      return a->refreshrate_float < b->refreshrate_float ? -1 : 1;
   return 0;
}

void video_mode_list_finish(video_display_config_t *list, unsigned len)
{
   unsigned i;
   if (!list || !len)
      return;
   qsort(list, len, sizeof(*list), video_mode_list_cmp);
   for (i = 0; i < len; i++)
      list[i].idx = i;
}
