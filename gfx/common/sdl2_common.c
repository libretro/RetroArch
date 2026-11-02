/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
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
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#ifdef HAVE_VIDEO_WINDOWS
#include <encodings/utf.h>
#endif

#include "sdl2_common.h"
#include "../../retroarch.h"

#include "SDL.h"
#include "SDL_syswm.h"

void sdl2_set_handles(void *data, enum rarch_display_type display_type)
{
   /* SysWMinfo headers are broken on OSX. */
   SDL_SysWMinfo info;
   SDL_Window *window = (SDL_Window*)data;
   SDL_VERSION(&info.version);

   if (SDL_GetWindowWMInfo(window, &info) != 1)
      return;

   video_driver_display_userdata_set((uintptr_t)window);

   switch (display_type)
   {
      case RARCH_DISPLAY_WIN32:
#if defined(_WIN32)
         video_driver_display_type_set(RARCH_DISPLAY_WIN32);
         video_driver_display_set(0);
         video_driver_window_set((uintptr_t)info.info.win.window);
#endif
         break;
      case RARCH_DISPLAY_X11:
#if defined(HAVE_X11)
         video_driver_display_type_set(RARCH_DISPLAY_X11);
         video_driver_display_set((uintptr_t)info.info.x11.display);
         video_driver_window_set((uintptr_t)info.info.x11.window);
#endif
         break;
      case RARCH_DISPLAY_OSX:
#ifdef HAVE_COCOA
         video_driver_display_type_set(RARCH_DISPLAY_OSX);
         video_driver_display_set(0);
         video_driver_window_set((uintptr_t)info.info.cocoa.window);
#endif
         break;
      case RARCH_DISPLAY_WAYLAND:
/* SDL_VIDEO_DRIVER_WAYLAND is defined by SDL2 */
#if defined(HAVE_WAYLAND) && defined(SDL_VIDEO_DRIVER_WAYLAND)
         video_driver_display_type_set(RARCH_DISPLAY_WAYLAND);
         video_driver_display_set((uintptr_t)info.info.wl.display);
         video_driver_window_set((uintptr_t)info.info.wl.surface);
#endif
         break;
      default:
      case RARCH_DISPLAY_NONE:
         break;
   }
}

#ifdef HAVE_VIDEO_WINDOWS
/* Secondary screens' windows, by screen */
static sdl2_video_window_t *sdl2_video_windows[RETRO_VIDEO_VIEWS_MAX];
static unsigned sdl2_video_window_count;

/* The displays last published for the screens' Display setting, on the
 * context's thread. dirty: enumerate again; own_dirty: the main window
 * may be on another one. */
static video_window_display_t sdl2_video_monitors[VIDEO_WINDOW_DISPLAYS_MAX];
static unsigned sdl2_video_monitor_count;
static bool sdl2_video_monitors_dirty;
static bool sdl2_video_monitors_own_dirty;

static video_window_common_t *sdl2_video_window_at(unsigned screen)
{
   return sdl2_video_windows[screen]
      ? &sdl2_video_windows[screen]->common : NULL;
}

/* Where the compositor places windows, a position is not theirs */
static bool sdl2_video_window_placed(void)
{
   return !string_is_equal(SDL_GetCurrentVideoDriver(), "wayland");
}

/* Where it is, its size and whether it is presentable, as SDL has them
 * now: a minimised or hidden window is skipped, never closed */
static void sdl2_video_window_check(sdl2_video_window_t *state)
{
   int x, y;
   bool presentable;
   int w                         = 0;
   int h                         = 0;
   video_window_native_t *window = state->common.native;
   Uint32 flags                  = SDL_GetWindowFlags(state->win);
   if (!(flags & SDL_WINDOW_MINIMIZED))
   {
      SDL_GetWindowSize(state->win, &w, &h);
      SDL_GetWindowPosition(state->win, &x, &y);
      if (     sdl2_video_window_placed()
            && VIDEO_POS_PACK(x, y) != state->common.pos)
      {
         state->common.pos      = VIDEO_POS_PACK(x, y);
         state->common.pending |= VIDEO_WINDOW_EVENT_BIT(GEOMETRY);
      }
   }
   if (w > 0 && h > 0 && VIDEO_SCALE_PACK(w, h) != window->dims)
   {
      window->dims           = VIDEO_SCALE_PACK(w, h);
      state->common.pending |= VIDEO_WINDOW_EVENT_BIT(GEOMETRY);
   }
   presentable = w > 0 && h > 0
      && !(flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN));
   if (presentable == window->presentable)
      return;
   window->presentable = presentable;
   if (presentable)
   {
      state->common.pending &= ~VIDEO_WINDOW_EVENT_BIT(SUSPEND);
      state->common.pending |=  VIDEO_WINDOW_EVENT_BIT(RESUME);
   }
   else
   {
      state->common.pending &= ~VIDEO_WINDOW_EVENT_BIT(RESUME);
      state->common.pending |=  VIDEO_WINDOW_EVENT_BIT(SUSPEND);
   }
}

bool sdl2_video_window_event(const SDL_Event *event, bool *quit)
{
   unsigned i;
   video_window_native_t *window;
   sdl2_video_window_t *state = NULL;

   for (i = 1; sdl2_video_window_count && i < RETRO_VIDEO_VIEWS_MAX
         && !state; i++)
      if (     sdl2_video_windows[i]
            && sdl2_video_windows[i]->id == event->window.windowID)
         state = sdl2_video_windows[i];
   if (!state)
   {
      if (     event->window.event == SDL_WINDOWEVENT_CLOSE
#if SDL_VERSION_ATLEAST(2, 0, 22)
            && SDL_GetHintBoolean(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE,
               SDL_TRUE)
#endif
         )
         *quit = true;
      return false;
   }
   if (state->common.closed)
      return true;
   window = state->common.native;

   switch (event->window.event)
   {
      case SDL_WINDOWEVENT_CLOSE:
         video_window_mark_closed(&state->common);
         return true;
      case SDL_WINDOWEVENT_FOCUS_GAINED:
         window->focused        = true;
         state->common.pending |= VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN);
         break;
      case SDL_WINDOWEVENT_FOCUS_LOST:
         window->focused        = false;
         state->common.pending &= ~VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN);
         state->common.pending |=  VIDEO_WINDOW_EVENT_BIT(FOCUS_LOSS);
         break;
      default:
         break;
   }
   sdl2_video_window_check(state);
   return true;
}

void sdl2_video_monitors_changed(void)
{
   sdl2_video_monitors_dirty = true;
}

void sdl2_video_monitors_moved(void)
{
   sdl2_video_monitors_own_dirty = true;
}

/* In SDL's display order, which a window's monitor setting counts in
 * (sdl2_video_window_place()). Enumerated only when dirty; else only
 * the main window's is found again. An @init list always goes out:
 * another driver's may be up. */
void sdl2_video_monitors_publish(SDL_Window *primary, bool init)
{
   video_window_display_t list[VIDEO_WINDOW_DISPLAYS_MAX];
   unsigned i;
   int own;
   unsigned count = sdl2_video_monitor_count;

   if (init)
      sdl2_video_monitors_dirty = true;
   else if (!sdl2_video_monitors_dirty && !sdl2_video_monitors_own_dirty)
      return;
   own = primary ? SDL_GetWindowDisplayIndex(primary) : -1;

   if (sdl2_video_monitors_dirty)
   {
      int n = SDL_GetNumVideoDisplays();
      count = n > 0 ? (unsigned)n : 0;
      if (count > VIDEO_WINDOW_DISPLAYS_MAX)
         count = VIDEO_WINDOW_DISPLAYS_MAX;
      for (i = 0; i < count; i++)
      {
         SDL_Rect bounds;
         video_window_display_t *display = &list[i];
         const char *name                = SDL_GetDisplayName((int)i);
         memset(display, 0, sizeof(*display));
         display->ordinal                = i + 1;
         display->valid                  = true;
         display->on                     = true;
         display->physical               = true;
         display->monitor                = true;
         if (name)
            utf8cpy(display->name, sizeof(display->name), name,
                  sizeof(display->name));
         if (     SDL_GetDisplayBounds((int)i, &bounds) == 0
               && VIDEO_SCALE_FITS(bounds.w, bounds.h))
            display->dims = VIDEO_SCALE_PACK(bounds.w, bounds.h);
      }
   }
   else
      memcpy(list, sdl2_video_monitors, count * sizeof(*list));
   for (i = 0; i < count; i++)
      list[i].own = (int)i == own;
   sdl2_video_monitors_dirty     = false;
   sdl2_video_monitors_own_dirty = false;
   if (     !init
         && count == sdl2_video_monitor_count
         && !memcmp(list, sdl2_video_monitors, count * sizeof(*list)))
      return;
   memcpy(sdl2_video_monitors, list, count * sizeof(*list));
   sdl2_video_monitor_count = count;
   video_driver_publish_window_displays(list, count);
}

/* Where 'settings' put a window, in SDL's window coordinates: on
 * display 'display' if not -1, else the preferred monitor (display
 * N-1), else with an automatic monitor the one the saved bounds are
 * mostly on, else the main window's, else the first. Saved bounds
 * partly off it are moved onto it; bounds not on it at all, and
 * automatic ones, are centred on it. Fullscreen covers it. */
static int sdl2_video_window_place(SDL_Window *primary,
      const video_window_settings_t *settings, int display, SDL_Rect *out)
{
   SDL_Rect bounds;
   video_window_rect_t rect, area;
   int count = SDL_GetNumVideoDisplays();

   out->x = VIDEO_POS_X(settings->pos);
   out->y = VIDEO_POS_Y(settings->pos);
   out->w = VIDEO_SCALE_W(settings->dims);
   out->h = VIDEO_SCALE_H(settings->dims);
   if (!settings->dims)
   {
      SDL_GetWindowSize(primary, &out->w, &out->h);
      if (out->w <= 0 || out->h <= 0)
      {
         out->w = 640;
         out->h = 480;
      }
   }

   if (display < 0 && settings->monitor && (int)settings->monitor <= count)
      display = settings->monitor - 1;
   if (display < 0 && !settings->monitor && settings->dims)
   {
      int i;
      int best = 0;
      for (i = 0; i < count; i++)
      {
         SDL_Rect inter;
         if (     SDL_GetDisplayBounds(i, &bounds) == 0
               && SDL_IntersectRect(out, &bounds, &inter)
               && inter.w * inter.h > best)
         {
            best    = inter.w * inter.h;
            display = i;
         }
      }
   }
   if (display < 0)
      display = SDL_GetWindowDisplayIndex(primary);
   if (display < 0)
      display = 0;
   if (SDL_GetDisplayBounds(display, &bounds) != 0)
      return display;

   if (settings->fullscreen)
   {
      *out = bounds;
      return display;
   }
   rect.x = out->x;
   rect.y = out->y;
   rect.w = out->w;
   rect.h = out->h;
   area.x = bounds.x;
   area.y = bounds.y;
   area.w = bounds.w;
   area.h = bounds.h;
   video_window_place_rect(&rect, &area, !settings->dims);
   out->x = rect.x;
   out->y = rect.y;
   out->w = rect.w;
   out->h = rect.h;
   return display;
}

/* Puts the window where its request has it */
static void sdl2_video_window_apply(sdl2_video_window_t *state)
{
   SDL_Rect rect;
   int display                        = -1;
   SDL_Window *win                    = state->win;
   const video_window_settings_t *now = &state->common.request.settings;

   state->apply = false;
   /* fullscreen on no monitor in particular is on the one it is on */
   if (now->fullscreen && !now->monitor)
      display = SDL_GetWindowDisplayIndex(win);
   display = sdl2_video_window_place(state->primary, now, display, &rect);
   if (now->fullscreen)
   {
      if (state->fullscreen && SDL_GetWindowDisplayIndex(win) == display)
         return;
      /* window managers keep a fullscreen window where it is, so it
       * moves by way of a window */
      if (state->fullscreen)
         SDL_SetWindowFullscreen(win, 0);
      else
      {
         SDL_GetWindowPosition(win, &state->windowed.x, &state->windowed.y);
         SDL_GetWindowSize(win, &state->windowed.w, &state->windowed.h);
         state->windowed_set = true;
      }
      if (SDL_GetWindowDisplayIndex(win) != display)
         SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
               SDL_WINDOWPOS_CENTERED_DISPLAY(display));
      SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
   }
   else if (state->fullscreen)
   {
      /* placed once SDL has it as a window again, frame and all */
      SDL_SetWindowFullscreen(win, 0);
      state->fullscreen = false;
      state->waiting    = SDL_WINDOW_FULLSCREEN;
      state->apply      = true;
      return;
   }
   else
   {
      /* a window again with automatic bounds: where it stood before */
      if (!now->dims && state->windowed_set)
         rect = state->windowed;
      state->windowed_set = false;
      SDL_SetWindowSize(win, rect.w, rect.h);
      SDL_SetWindowPosition(win, rect.x, rect.y);
   }
   state->fullscreen = now->fullscreen;
   /* SDL 2 has it where it was put without an event to say so */
   sdl2_video_window_check(state);
}

/* Its request is made once SDL has the window restored, and placed as
 * a window once SDL has it so: from its restored size, never its
 * minimised or maximised one */
static void sdl2_video_window_settle(sdl2_video_window_t *state)
{
   Uint32 flags = SDL_GetWindowFlags(state->win);
   if (     (flags & SDL_WINDOW_MAXIMIZED) && !(flags & SDL_WINDOW_MINIMIZED)
         && !(state->waiting & SDL_WINDOW_MAXIMIZED))
   {
      SDL_RestoreWindow(state->win);
      state->waiting |= SDL_WINDOW_MAXIMIZED;
   }
   if (flags & (SDL_WINDOW_MINIMIZED | state->waiting))
      return;
   state->waiting = 0;
   sdl2_video_window_apply(state);
}

/* usable() is asked on the app thread too: SDL video stays on this one */
static retro_atomic_int_t sdl2_video_windows_ok;

void sdl2_video_windows_probe(void)
{
   const char *driver = SDL_GetCurrentVideoDriver();
   retro_atomic_store_release_int(&sdl2_video_windows_ok,
             string_is_equal(driver, "x11")
          || string_is_equal(driver, "wayland")
          || string_is_equal(driver, "windows"));
}

bool sdl2_video_windows_supported(void)
{
   return retro_atomic_load_acquire_int(&sdl2_video_windows_ok) != 0;
}

bool sdl2_video_window_create(void *owner, SDL_Window *primary,
      const video_window_request_t *request, video_window_native_t *window)
{
   char title[64];
   SDL_Rect rect;
   int display;
   video_window_settings_t windowed;
   sdl2_video_window_t *state;
   Uint32 flags       = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
      | SDL_WINDOW_HIDDEN;

   if (     !owner || !primary || !request || !window || window->state
         || !sdl2_video_windows_supported()
         || !request->generation
         || !request->screen
         || request->screen >= RETRO_VIDEO_VIEWS_MAX
         || request->settings.mode != VIDEO_WINDOW_SEPARATE
         || sdl2_video_windows[request->screen]
         || (request->settings.dims
            && (  !VIDEO_SCALE_W(request->settings.dims)
               || !VIDEO_SCALE_H(request->settings.dims))))
      return false;
   if (!(state = (sdl2_video_window_t*)calloc(1, sizeof(*state))))
      return false;

   /* made at its window size, filling the display from there */
   windowed            = request->settings;
   windowed.fullscreen = false;
   display = sdl2_video_window_place(primary, &windowed, -1, &rect);
   if (request->settings.fullscreen)
   {
      flags  |= SDL_WINDOW_FULLSCREEN_DESKTOP;
      rect.x  = rect.y = SDL_WINDOWPOS_CENTERED_DISPLAY(display);
   }
   snprintf(title, sizeof(title), "RetroArch - Screen %u",
         request->screen + 1);
   if (!(state->win = SDL_CreateWindow(title, rect.x, rect.y, rect.w, rect.h,
               flags)))
   {
      free(state);
      return false;
   }

   state->id             = SDL_GetWindowID(state->win);
   state->primary        = primary;
   state->fullscreen     = request->settings.fullscreen;
   state->common.owner   = owner;
   state->common.native  = window;
   state->common.request = *request;
   memset(window, 0, sizeof(*window));
   window->state         = state;
   window->surface       = state->win;
   window->kind          = VIDEO_WINDOW_NATIVE_SDL2;
   window->generation    = request->generation;
   window->configured    = true;
   sdl2_video_windows[request->screen] = state;
   sdl2_video_window_count++;

#ifdef SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN
   /* the main window keeps the focus */
   SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "1");
#endif
   SDL_ShowWindow(state->win);
#ifdef SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN
   SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "0");
#endif
   /* SDL 2 puts a new window's frame where its drawable area was asked
    * for; shown, it knows the frame */
   if (sdl2_video_window_placed() && !request->settings.fullscreen)
      SDL_SetWindowPosition(state->win, rect.x, rect.y);

   /* as it came up; its events keep these from here on */
   if (sdl2_video_window_placed())
   {
      SDL_GetWindowPosition(state->win, &rect.x, &rect.y);
      state->common.pos = VIDEO_POS_PACK(rect.x, rect.y);
   }
   SDL_GetWindowSize(state->win, &rect.w, &rect.h);
   window->dims = VIDEO_SCALE_PACK(rect.w, rect.h);
   sdl2_video_window_check(state);
   return true;
}

void sdl2_video_window_update(void *owner, video_window_native_t *window,
      const video_window_request_t *request)
{
   const video_window_settings_t *now;
   const video_window_settings_t *was;
   sdl2_video_window_t *state = window
      ? (sdl2_video_window_t*)window->state : NULL;

   if (     !state || state->common.owner != owner || !request
         || state->common.closed
         || request->screen     != state->common.request.screen
         || request->generation != state->common.request.generation)
      return;
   now = &request->settings;
   was = &state->common.request.settings;
   if (     (now->dims && (!VIDEO_SCALE_W(now->dims) || !VIDEO_SCALE_H(now->dims)))
         || (     now->pos        == was->pos
               && now->dims       == was->dims
               && now->monitor    == was->monitor
               && now->fullscreen == was->fullscreen))
      return;
   state->common.request = *request;
   state->apply          = true;
   sdl2_video_window_settle(state);
}

unsigned sdl2_video_window_poll(void *owner, video_window_event_t *events,
      unsigned capacity)
{
   unsigned i;
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      sdl2_video_window_t *state = sdl2_video_windows[i];
      if (     state && state->apply && state->common.owner == owner
            && !state->common.closed)
         sdl2_video_window_settle(state);
   }
   return video_windows_drain(owner, sdl2_video_window_at, events,
         capacity);
}

static int SDLCALL sdl2_video_window_unqueue(void *data, SDL_Event *event)
{
   return    event->type != SDL_WINDOWEVENT
          || event->window.windowID != *(const Uint32*)data;
}

void sdl2_video_window_destroy(void *owner, video_window_native_t *window)
{
   Uint32 id;
   sdl2_video_window_t *state = window
      ? (sdl2_video_window_t*)window->state : NULL;

   if (!state || state->common.owner != owner)
      return;
   /* out of the table first: nothing destroying it sends is reported */
   sdl2_video_windows[state->common.request.screen] = NULL;
   sdl2_video_window_count--;
   id = state->id;
   SDL_DestroyWindow(state->win);
   /* nor is what is still queued for it, which the main window's
    * handling would take */
   SDL_FilterEvents(sdl2_video_window_unqueue, &id);
   free(state);
   memset(window, 0, sizeof(*window));
}

void sdl2_video_windows_free(void *owner,
      void (*destroy)(void *owner, video_window_native_t *window))
{
   video_windows_sweep(owner, sdl2_video_window_at, destroy);
   /* The context is going, and its displays with it */
   sdl2_video_monitor_count = 0;
   video_driver_publish_window_displays(NULL, 0);
}
#endif
