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

void video_window_mark_closed(video_window_common_t *window)
{
   video_window_native_t *native = window->native;
   if (window->closed)
      return;
   window->closed       = true;
   window->pending     &= ~(VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN)
                          | VIDEO_WINDOW_EVENT_BIT(RESUME));
   window->pending     |= VIDEO_WINDOW_EVENT_BIT(CLOSE);
   if (native->focused)
      window->pending  |= VIDEO_WINDOW_EVENT_BIT(FOCUS_LOSS);
   native->focused     = false;
   native->presentable = false;
}

unsigned video_windows_drain(void *owner, video_window_at_t at,
      video_window_event_t *events, unsigned capacity)
{
   static const unsigned char order[] = {
      VIDEO_WINDOW_EVENT_FOCUS_LOSS, VIDEO_WINDOW_EVENT_CLOSE,
      VIDEO_WINDOW_EVENT_GEOMETRY,   VIDEO_WINDOW_EVENT_SUSPEND,
      VIDEO_WINDOW_EVENT_RESUME,     VIDEO_WINDOW_EVENT_FOCUS_GAIN
   };
   unsigned i, j;
   unsigned count = 0;

   if (!events)
      return 0;
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX && count < capacity; i++)
   {
      video_window_common_t *window = at(i);
      if (!window || window->owner != owner)
         continue;
      for (j = 0; j < sizeof(order) / sizeof(order[0]) && count < capacity;
            j++)
      {
         unsigned bit = 1u << order[j];
         if (window->pending & bit)
         {
            video_window_event_t *event = &events[count++];
            event->screen     = window->request.screen;
            event->generation = window->request.generation;
            event->kind       = order[j];
            event->pos        = window->pos;
            event->dims       = window->native->dims;
            window->pending  &= ~bit;
         }
      }
   }
   return count;
}

void video_windows_sweep(void *owner, video_window_at_t at,
      void (*destroy)(void *owner, video_window_native_t *window))
{
   unsigned i;
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      video_window_common_t *window = at(i);
      if (window && window->owner == owner)
         destroy(owner, window->native);
   }
}
