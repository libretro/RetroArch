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

#include <retro_miscellaneous.h>

#include "video_windows.h"

void video_windows_plan(const video_views_map_t *map,
      const video_window_settings_t *settings, bool enabled,
      bool supported, unsigned forced_attached, unsigned attached,
      video_windows_plan_t *out)
{
   unsigned s;
   memset(out, 0, sizeof(*out));
   if (!map || !map->num_views)
      return;
   if (attached >= map->num_screens || attached >= RETRO_VIDEO_VIEWS_MAX)
      attached = 0;
   for (s = 0; s < map->num_screens && s < RETRO_VIDEO_VIEWS_MAX; s++)
   {
      unsigned bit  = 1u << s;
      /* screen 0 takes @attached's mode */
      unsigned mode = settings
         ? settings[s ? s : attached].mode : VIDEO_WINDOW_ATTACHED;
      if (!enabled || s == attached)
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

/* Auto's display: an external one before a built-in one, the lowest
 * ordinal among equals; 0 for none */
static unsigned video_windows_auto_display(
      const video_window_display_t *displays, unsigned count,
      bool *external)
{
   unsigned i;
   unsigned best = 0;
   *external     = false;
   for (i = 0; i < count; i++)
   {
      const video_window_display_t *d = &displays[i];
      bool ext;
      if (     !d->ordinal || d->own || !d->valid || !d->on
            || d->is_private || !d->physical)
         continue;
      ext = d->connection == VIDEO_WINDOW_CONNECTION_DIRECT
         || d->connection == VIDEO_WINDOW_CONNECTION_TRANSITIVE;
      if (     !best
            || (ext && !*external)
            || (ext == *external && d->ordinal < best))
      {
         best      = d->ordinal;
         *external = ext;
      }
   }
   return best;
}

/* Display @ordinal, named by a screen's setting */
static bool video_windows_named(const video_window_display_t *displays,
      unsigned count, unsigned ordinal)
{
   unsigned i;
   for (i = 0; i < count; i++)
      if (displays[i].ordinal == ordinal)
         return !displays[i].own && displays[i].valid
            && !displays[i].is_private;
   return false;
}

void video_windows_route(const video_window_display_t *displays,
      unsigned count, const video_window_settings_t *settings,
      unsigned num_screens, video_windows_route_t *out)
{
   unsigned s;
   bool external;
   unsigned automatic = video_windows_auto_display(displays, count,
         &external);
   memset(out, 0, sizeof(*out));
   if (!settings)
      return;
   if (num_screens > RETRO_VIDEO_VIEWS_MAX)
      num_screens = RETRO_VIDEO_VIEWS_MAX;
   for (s = 1; s < num_screens; s++)
   {
      unsigned monitor = settings[s].monitor;
      unsigned display = monitor ? monitor : automatic;
      if (settings[s].mode != VIDEO_WINDOW_SEPARATE)
         continue;
      if (     !display
            || (monitor && !video_windows_named(displays, count, monitor))
            || (out->display && display != out->display))
      {
         out->unplaced |= 1u << s;
         continue;
      }
      out->display  = display;
      out->screens |= 1u << s;
   }
   /* An external monitor takes screen 0; screen 1 stays, with touch */
   if (     external && out->display == automatic
         && !settings[1].monitor && (out->screens & 2u))
   {
      out->screens  = (out->screens & ~2u) | 1u;
      out->attached = 1;
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

unsigned video_window_rect_overlap(const video_window_rect_t *a,
      const video_window_rect_t *b)
{
   int w = MIN(a->x + a->w, b->x + b->w) - MAX(a->x, b->x);
   int h = MIN(a->y + a->h, b->y + b->h) - MAX(a->y, b->y);
   if (w <= 0 || h <= 0)
      return 0;
   return (unsigned)w * (unsigned)h;
}

void video_window_place_rect(video_window_rect_t *rect,
      const video_window_rect_t *area, bool automatic)
{
   /* Partly on it as saved, before the size is clamped */
   bool on = !automatic && video_window_rect_overlap(rect, area);
   rect->w = MIN(rect->w, area->w);
   rect->h = MIN(rect->h, area->h);
   if (!on)
   {
      rect->x = area->x + (area->w - rect->w) / 2;
      rect->y = area->y + (area->h - rect->h) / 2;
      return;
   }
   if (rect->x < area->x)
      rect->x = area->x;
   if (rect->y < area->y)
      rect->y = area->y;
   if (rect->x + rect->w > area->x + area->w)
      rect->x = area->x + area->w - rect->w;
   if (rect->y + rect->h > area->y + area->h)
      rect->y = area->y + area->h - rect->h;
}

void video_windows_steps(video_windows_held_t *held,
      const video_windows_frame_t *frame, unsigned *retire,
      unsigned *create)
{
   unsigned w;
   unsigned wanted = 0;
   *retire         = 0;
   if (frame)
   {
      /* A restore tries the failed ones again */
      if (frame->restore != held->restore)
         held->failed = 0;
      held->restore = frame->restore;
      for (w = 1; w < RETRO_VIDEO_VIEWS_MAX; w++)
      {
         unsigned bit = 1u << w;
         if (!frame->window_screens[w])
            continue;
         wanted |= bit;
         if ((held->live & bit) && held->display[w] != frame->display)
            *retire |= bit;
      }
   }
   held->failed &= wanted;
   *retire      |= held->live & ~wanted;
   *create       = wanted & ~(held->live & ~*retire) & ~held->failed;
}
