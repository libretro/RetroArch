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
      bool supported, unsigned forced_attached,
      const video_windows_route_t *route, video_windows_plan_t *out)
{
   unsigned s;
   memset(out, 0, sizeof(*out));
   if (!map || !map->num_views)
      return;
   for (s = 0; s < map->num_screens && s < RETRO_VIDEO_VIEWS_MAX; s++)
   {
      unsigned bit  = 1u << s;
      unsigned mode = settings ? settings[s].mode : VIDEO_WINDOW_ATTACHED;
      bool separate = route ? (route->screens & bit) != 0
         : mode == VIDEO_WINDOW_SEPARATE;
      if (!enabled || (!route && !s))
         out->main_mask |= bit;
      else if (mode == VIDEO_WINDOW_HIDDEN)
         out->hidden_mask |= bit;
      else if (separate && supported && !(forced_attached & bit))
         out->separate_mask |= bit;
      else
         out->main_mask |= bit;
   }
}

void video_windows_display_key(const video_window_display_t *displays,
      unsigned index, char *out, size_t size)
{
   unsigned j;
   size_t len;
   unsigned n       = 1;
   const char *name = displays[index].name;
   if (!size)
      return;
   for (j = 0; j < index; j++)
      if (!strcmp(displays[j].name, name))
         n++;
   len = strlen(name);
   if (n > 1 && size > 3 && len > size - 3)
      len = size - 3;
   else if (len > size - 1)
      len = size - 1;
   memcpy(out, name, len);
   if (n > 1 && n < 10 && len + 2 < size)
   {
      out[len++] = ' ';
      out[len++] = (char)('0' + n);
   }
   out[len] = '\0';
}

#define VIDEO_WINDOWS_ORDINAL_BIT(o) \
   (((o) && (o) <= 32) ? 1u << ((o) - 1) : 0u)

/* The display @name names, if a window may go there; 0 for none */
static unsigned video_windows_named(const video_window_display_t *displays,
      unsigned count, const char *name)
{
   unsigned i;
   char key[VIDEO_WINDOW_DISPLAY_NAME_SIZE];
   for (i = 0; i < count && i < VIDEO_WINDOW_DISPLAYS_MAX; i++)
   {
      video_windows_display_key(displays, i, key, sizeof(key));
      if (!strcmp(key, name))
         return (displays[i].valid && !displays[i].is_private)
            ? displays[i].ordinal : 0;
   }
   return 0;
}

static bool video_windows_refused(const video_window_display_t *displays,
      unsigned count, unsigned ordinal)
{
   unsigned i;
   for (i = 0; i < count; i++)
      if (displays[i].ordinal == ordinal)
         return displays[i].refused;
   return false;
}

/* Count, names and on/valid flags, so a list that only moved RetroArch
 * or marked a refusal is the same list */
static unsigned video_windows_names_hash(
      const video_window_display_t *displays, unsigned count)
{
   unsigned i;
   uint32_t h = 2166136261u;
   for (i = 0; i < count; i++)
   {
      const unsigned char *c = (const unsigned char*)displays[i].name;
      for (; *c; c++)
         h = (h ^ *c) * 16777619u;
      /* Above any byte, so it ends the name too */
      h = (h ^ (0x100u | (displays[i].on ? 1u : 0u)
            | (displays[i].valid ? 2u : 0u))) * 16777619u;
   }
   h ^= count;
   return h ? (unsigned)h : 1u;
}

void video_windows_host_reset(video_windows_host_t *host)
{
   memset(host, 0, sizeof(*host));
}

/* A display Auto may pick */
static bool video_windows_place_eligible(const video_window_display_t *d)
{
   return d->ordinal && d->valid && d->on && !d->is_private;
}

static bool video_windows_place_external(const video_window_display_t *d)
{
   return d->physical
      && (   d->connection == VIDEO_WINDOW_CONNECTION_DIRECT
          || d->connection == VIDEO_WINDOW_CONNECTION_TRANSITIVE);
}

/* Screen @s's Auto display, whatever RetroArch's own: for the first an
 * external monitor, for the second the built-in second panel unless a
 * monitor took the first (@first), else the default display, or @own
 * where none is listed */
static unsigned video_windows_place_auto(const video_windows_place_in_t *in,
      unsigned own, unsigned first, unsigned s)
{
   unsigned i;
   unsigned external   = 0;
   unsigned def        = 0;
   unsigned panel      = 0;
   bool first_external = false;
   for (i = 0; i < in->count && i < VIDEO_WINDOW_DISPLAYS_MAX; i++)
   {
      const video_window_display_t *d = &in->displays[i];
      bool ext = video_windows_place_external(d);
      if (first && d->ordinal == first)
         first_external = ext;
      if (!video_windows_place_eligible(d))
         continue;
      if (ext && (!external || d->ordinal < external))
         external = d->ordinal;
      if (d->is_default && (!def || d->ordinal < def))
         def = d->ordinal;
      if (     d->physical && !d->is_default && !ext
            && (!panel || d->ordinal < panel))
         panel = d->ordinal;
   }
   if (!s && external)
      return external;
   if (s == 1 && !first_external && panel)
      return panel;
   return def ? def : own;
}

/* Screen @s's display by its setting, 0 when Off */
static unsigned video_windows_place_screen(
      const video_windows_place_in_t *in, unsigned own, unsigned first,
      unsigned s)
{
   const char *key = in->screen_display ? in->screen_display[s] : "";
   unsigned d      = 0;
   if (!strcmp(key, VIDEO_WINDOW_DISPLAY_OFF))
      return 0;
   if (*key)
      d = video_windows_named(in->displays, in->count, key);
   return d ? d : video_windows_place_auto(in, own, first, s);
}

/* Each screen's display and the menu's; false without an own display */
static bool video_windows_place_targets(const video_windows_place_in_t *in,
      video_windows_route_t *r)
{
   unsigned i, s;
   unsigned want[RETRO_VIDEO_VIEWS_MAX];
   unsigned screens = MIN(in->num_screens, RETRO_VIDEO_VIEWS_MAX);
   memset(r, 0, sizeof(*r));
   if (!in->displays)
      return false;
   for (i = 0; i < in->count && !r->own; i++)
      if (in->displays[i].own)
         r->own = in->displays[i].ordinal;
   if (!r->own)
      return false;
   for (s = 0; s < RETRO_VIDEO_VIEWS_MAX; s++)
      want[s] = video_windows_place_screen(in, r->own, s ? want[0] : 0, s);
   for (s = 0; s < screens; s++)
      r->target[s] = want[s];
   /* A game without a map is shown even when its screen is Off */
   if (in->mapless && screens && !r->target[0])
      r->target[0] = video_windows_place_auto(in, r->own, 0, 0);
   if (in->menu_follows < VIDEO_MENU_FOLLOWS_SEPARATE)
   {
      s       = in->menu_follows;
      r->menu = want[s] ? want[s]
         : video_windows_place_auto(in, r->own, want[0], s);
   }
   else
   {
      const char *key = in->menu_display ? in->menu_display : "";
      if (*key && strcmp(key, VIDEO_WINDOW_DISPLAY_OFF))
         r->menu = video_windows_named(in->displays, in->count, key);
      if (!r->menu)
         r->menu = video_windows_place_auto(in, r->own, 0, 0);
   }
   return true;
}

/* A display the Presentation may go on, the main window being on @host */
static bool video_windows_place_free(const video_windows_place_in_t *in,
      unsigned unable, unsigned host, unsigned d)
{
   return d && d != host && !(unable & VIDEO_WINDOWS_ORDINAL_BIT(d))
      && !video_windows_refused(in->displays, in->count, d);
}

/* The Presentation's display: the lowest screen's, else the menu's when
 * @menu is in use */
static unsigned video_windows_place_pick(const video_windows_place_in_t *in,
      const video_windows_route_t *r, unsigned unable, unsigned host,
      bool menu)
{
   unsigned s;
   /* A game without a map draws in the main window only */
   if (!in->mapless)
      for (s = 0; s < RETRO_VIDEO_VIEWS_MAX; s++)
         if (video_windows_place_free(in, unable, host, r->target[s]))
            return r->target[s];
   return (menu && video_windows_place_free(in, unable, host, r->menu))
      ? r->menu : 0;
}

/* The Presentation, what each window shows and the logo, with the main
 * window on @host */
static void video_windows_place_route(const video_windows_place_in_t *in,
      unsigned unable, unsigned host, video_windows_route_t *r)
{
   unsigned s;
   unsigned lowest = RETRO_VIDEO_VIEWS_MAX;
   bool no_game    = !in->num_screens;
   bool menu_main;
   r->display      = video_windows_place_pick(in, r, unable, host,
         in->menu_open || no_game);
   r->screens      = 0;
   r->main_screens = 0;
   r->ui           = 0;
   for (s = 0; s < RETRO_VIDEO_VIEWS_MAX; s++)
   {
      if (!r->target[s])
         continue;
      if (lowest == RETRO_VIDEO_VIEWS_MAX)
         lowest = s;
      if (!in->mapless && r->display && r->target[s] == r->display)
         r->screens      |= 1u << s;
      else
         r->main_screens |= 1u << s;
   }
   /* Counted as open, so the menu keeps its window while closed */
   if (video_windows_place_pick(in, r, unable, host, true) == r->menu)
      r->ui |= VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU);
   menu_main = !(r->ui & VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU));
   /* Notifications go with the menu while it is open or RetroArch has
    * a window on its display, else with the game's lowest screen */
   if (     no_game
         || (   in->notifications == VIDEO_NOTIFICATIONS_WITH_MENU
             && (   in->menu_open
                 || r->menu == host
                 || r->menu == r->display)))
   {
      if (!menu_main)
         r->ui |= VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_NOTIFICATIONS);
   }
   else if (lowest < RETRO_VIDEO_VIEWS_MAX && (r->screens & (1u << lowest)))
      r->ui |= VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_NOTIFICATIONS);
   r->logo = !no_game && !in->mapless && !r->main_screens
      && !(in->menu_open && menu_main);
}

/* The screens' and the menu's display settings; Notifications moves
 * nothing */
static unsigned video_windows_settings_hash(
      const video_windows_place_in_t *in)
{
   unsigned s;
   uint32_t h = 2166136261u;
   for (s = 0; s <= RETRO_VIDEO_VIEWS_MAX; s++)
   {
      const unsigned char *c = (const unsigned char*)(
            s < RETRO_VIDEO_VIEWS_MAX
            ? (in->screen_display ? in->screen_display[s] : "")
            : (in->menu_display   ? in->menu_display      : ""));
      for (; *c; c++)
         h = (h ^ *c) * 16777619u;
      /* Above any byte, so it ends the key too */
      h = (h ^ 0x100u) * 16777619u;
   }
   h = (h ^ in->menu_follows) * 16777619u;
   return h ? (unsigned)h : 1u;
}

static unsigned video_windows_refusals(const video_windows_place_in_t *in)
{
   unsigned i;
   unsigned bits = 0;
   for (i = 0; i < in->count; i++)
      if (in->displays[i].refused)
         bits |= VIDEO_WINDOWS_ORDINAL_BIT(in->displays[i].ordinal);
   return bits;
}

/* The main window's display: a game without a map's own; else a
 * refusing display this game may use, the lowest screen's before the
 * menu's; else the lowest shown screen's; else the menu's */
static unsigned video_windows_place_decide(
      const video_windows_place_in_t *in, const video_windows_route_t *r,
      unsigned refused)
{
   unsigned s;
   if (in->num_screens && in->mapless)
      return r->target[0];
   for (s = 0; s < RETRO_VIDEO_VIEWS_MAX; s++)
      if (refused & VIDEO_WINDOWS_ORDINAL_BIT(r->target[s]))
         return r->target[s];
   if (refused & VIDEO_WINDOWS_ORDINAL_BIT(r->menu))
      return r->menu;
   for (s = 0; s < RETRO_VIDEO_VIEWS_MAX; s++)
      if (r->target[s])
         return r->target[s];
   return r->menu;
}

void video_windows_place(video_windows_host_t *host,
      const video_windows_place_in_t *in, video_windows_route_t *out)
{
   unsigned names, settings, refused;
   unsigned move = 0;
   bool event    = false;
   if (!video_windows_place_targets(in, out))
      return;
   names    = video_windows_names_hash(in->displays, in->count);
   settings = video_windows_settings_hash(in);
   refused  = video_windows_refusals(in);
   if (names != host->names)
   {
      host->names       = names;
      host->unable      = 0;
      host->refused     = 0;
      host->moving      = 0;
      host->make        = 0;
      host->keep        = 0;
      event             = true;
   }
   if (settings != host->settings)
   {
      host->settings    = settings;
      event             = true;
   }
   if (     in->num_screens != host->num_screens
         || in->mapless     != host->mapless
         || in->switching   != host->switching)
   {
      /* Content start and close are events, and so is any map change
       * soon after a start; later ones move nothing, nor let a make the
       * start asked move. Content replaced is a close and a start once
       * the new one is up, never sooner. */
      if (!in->switching)
      {
         if (host->switching || !in->num_screens || !host->num_screens)
         {
            host->start_ends = in->num_screens
               ? in->now + VIDEO_WINDOWS_MOVE_USEC : 0;
            event            = true;
         }
         else if (in->now < host->start_ends)
            event            = true;
         else if (host->asked < host->start_ends)
            host->make       = 0;
      }
      host->num_screens = in->num_screens;
      host->mapless     = in->mapless;
      host->switching   = in->switching;
   }
   if (refused & ~host->refused)
      event             = true;
   host->refused       |= refused;
   if (host->moving)
   {
      /* Decided again when it ends: an event meanwhile was deferred */
      if (out->own == host->moving)
      {
         host->moving   = 0;
         event          = true;
      }
      else if (in->move_failed
            || in->now - host->asked >= VIDEO_WINDOWS_MOVE_USEC)
      {
         host->unable  |= VIDEO_WINDOWS_ORDINAL_BIT(host->moving);
         host->moving   = 0;
         host->host     = out->own;
         host->keep     = 0;
         event          = true;
      }
   }
   /* A display RetroArch is on took it, even after the move was given
    * up */
   host->unable        &= ~VIDEO_WINDOWS_ORDINAL_BIT(out->own);
   /* A move under way finishes first */
   if (event && !host->moving)
   {
      unsigned d = video_windows_place_decide(in, out, host->refused);
      if (host->unable & VIDEO_WINDOWS_ORDINAL_BIT(d))
         d = out->own;
      host->host = d;
      host->make = 0;
      if (d != out->own)
      {
         /* Onto the Presentation's display: under it, which stays */
         if (in->shown == d)
         {
            host->keep        = d;
            host->keep_frames = VIDEO_WINDOWS_HOLD_FRAMES;
            move              = d;
         }
         else
         {
            /* A display RetroArch leaves that stays in use gets its
             * Presentation first */
            video_windows_place_route(in, host->unable, d, out);
            if (out->display == out->own)
            {
               host->make  = out->own;
               host->asked = in->now;
            }
            else
               move        = d;
         }
      }
   }
   if (host->make)
   {
      if (out->own != host->make)
         host->make = 0;
      else if (in->shown == host->make
            || in->now - host->asked >= VIDEO_WINDOWS_MOVE_USEC)
      {
         host->make = 0;
         move       = host->host;
      }
   }
   if (move)
   {
      host->moving = move;
      host->asked  = in->now;
   }
   if (host->keep && !host->moving)
   {
      if (out->own != host->keep || !host->keep_frames)
         host->keep = 0;
      else
         host->keep_frames--;
   }
   /* While a move is pending the main window shows what it will */
   video_windows_place_route(in, host->unable,
         (host->moving || host->make) ? host->host : out->own, out);
   if (host->keep)
   {
      /* Not drawn: every shown screen stays in the main window */
      out->keep          = host->keep;
      out->display       = host->keep;
      out->main_screens |= out->screens;
      out->screens       = 0;
      out->ui            = 0;
      out->logo          = in->num_screens && !in->mapless
         && !out->main_screens && !in->menu_open;
   }
   out->move = move;
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
         if (     !frame->window_screens[w] && !frame->window_ui[w]
               && !(frame->window_keep & bit))
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
