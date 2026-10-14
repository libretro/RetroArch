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

#include <string.h>

#include "video_windows.h"

void video_windows_plan(const video_views_map_t *map,
      const video_window_settings_t *settings, bool enabled,
      bool supported, unsigned forced_attached,
      video_windows_plan_t *out)
{
   unsigned s;
   memset(out, 0, sizeof(*out));
   if (!map || !map->num_views)
      return;
   for (s = 0; s < map->num_screens && s < RETRO_VIDEO_VIEWS_MAX; s++)
   {
      unsigned bit = 1u << s;
      unsigned mode = settings ? settings[s].mode : VIDEO_WINDOW_ATTACHED;
      if (!enabled || s == 0)
         out->main_mask |= bit;
      else if (mode == VIDEO_WINDOW_HIDDEN)
         out->hidden_mask |= bit;
      else if (mode == VIDEO_WINDOW_SEPARATE && supported
            && !(forced_attached & bit))
         out->separate_mask |= bit;
      else
         out->main_mask |= bit;
   }
}
