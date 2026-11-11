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
#include <features/features_cpu.h>
#include <queues/task_queue.h>
#endif

#include "sdl2_common.h"
#include "../../retroarch.h"

#include "SDL.h"
#include "SDL_syswm.h"

/* Windows and macOS show the executable's and the bundle's own icon,
 * at full size */
#if !defined(_WIN32) && !defined(__APPLE__)
/* 16x16 ARGB8888 window icon, as sdl3_common.c's */
static const uint32_t sdl2_icon_data[16 * 16] = {
0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
0x00000000,0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,0x00000000,
0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
};

void sdl2_window_set_icon(SDL_Window *win)
{
   SDL_Surface *icon = SDL_CreateRGBSurfaceFrom((void*)sdl2_icon_data,
         16, 16, 32, 16 * sizeof(uint32_t),
         0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

   if (!icon)
      return;

   /* SDL keeps a copy */
   SDL_SetWindowIcon(win, icon);
   SDL_FreeSurface(icon);
}
#else
void sdl2_window_set_icon(SDL_Window *win)
{
}
#endif

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
static void sdl2_video_window_let_go(sdl2_video_window_t *state);
static unsigned sdl2_video_window_count;
/* By screen, its window and SDL's ID for it, which the input driver
 * tells a screen's window by on the frontend's thread */
static retro_atomic_size_t sdl2_video_window_wins[RETRO_VIDEO_VIEWS_MAX];
static retro_atomic_int_t  sdl2_video_window_ids[RETRO_VIDEO_VIEWS_MAX];

/* The displays last published for the screens' Display setting, on the
 * context's thread. dirty: enumerate again; own_dirty: the main window
 * may be on another one. */
static video_window_display_t sdl2_video_monitors[VIDEO_WINDOW_DISPLAYS_MAX];
static unsigned sdl2_video_monitor_count;
static bool sdl2_video_monitors_dirty;
static bool sdl2_video_monitors_own_dirty;

static void sdl2_video_windows_lost(bool removed);

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

/* Not fullscreen, maximised or minimised, by SDL and by what it was
 * asked. While an update waits (apply), the bounds are RetroArch's. */
static bool sdl2_video_window_normal(const sdl2_video_window_t *state,
      Uint32 flags)
{
   return !(flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED
            | SDL_WINDOW_MINIMIZED))
      && !state->common.request.settings.fullscreen
      && !state->apply;
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
   retro_time_t now              = cpu_features_get_time_usec();
   bool placed                   = sdl2_video_window_placed();
   bool normal                   = sdl2_video_window_normal(state, flags);
   if (!(flags & SDL_WINDOW_MINIMIZED))
   {
      SDL_GetWindowSize(state->win, &w, &h);
      SDL_GetWindowPosition(state->win, &x, &y);
      if (placed && VIDEO_POS_PACK(x, y) != state->common.pos)
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
   /* Hidden, or back from a state, by the flags SDL has after its
    * whole pump: SDL and the window manager place it for a while */
   if (     (flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED))
         || (state->flags & ~flags & (SDL_WINDOW_HIDDEN
               | SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED
               | SDL_WINDOW_FULLSCREEN)))
      video_window_unsettled(&state->common, now);
   state->flags        = flags;
   state->judge_at     = now;
   state->judge_pos    = state->common.pos;
   state->judge_dims   = window->dims;
   state->judge_normal = normal;
   state->judge        = true;
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
         video_window_mark_closed_by_user(&state->common);
         return true;
      /* Mapped. create() shows it, which clears HIDDEN before the
       * first check, so only this event tells. */
      case SDL_WINDOWEVENT_SHOWN:
         video_window_unsettled(&state->common,
               cpu_features_get_time_usec());
         break;
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

/* What the last check took, once SDL has pumped again: a window manager
 * may set _NET_WM_STATE a pump after it configures. Normal only if it
 * was at the check and still is. */
void sdl2_video_windows_judge(void)
{
   unsigned i;
   for (i = 1; sdl2_video_window_count && i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      bool normal;
      sdl2_video_window_t *state = sdl2_video_windows[i];
      if (!state || !state->judge)
         continue;
      state->judge = false;
      normal       = state->judge_normal
         && sdl2_video_window_normal(state, SDL_GetWindowFlags(state->win));
      video_window_moved(&state->common, state->judge_pos,
            sdl2_video_window_placed(), state->judge_dims, normal,
            state->judge_at);
   }
}

Uint32 sdl2_video_windows_flags(void)
{
   unsigned i;
   Uint32 flags = 0;
   for (i = 1; sdl2_video_window_count && i < RETRO_VIDEO_VIEWS_MAX; i++)
      if (sdl2_video_windows[i] && !sdl2_video_windows[i]->common.closed)
         flags |= SDL_GetWindowFlags(sdl2_video_windows[i]->win);
   return flags;
}

void sdl2_video_monitors_changed(bool removed)
{
   sdl2_video_monitors_dirty = true;
   sdl2_video_windows_lost(removed);
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
 * partly off it are moved onto it; bounds not on it at all, automatic
 * ones and ones with no position set are centred on it. Fullscreen
 * covers it. */
static int sdl2_video_window_place(SDL_Window *primary,
      const video_window_settings_t *settings, int display, SDL_Rect *out)
{
   SDL_Rect bounds;
   video_window_rect_t rect, area;
   int count      = SDL_GetNumVideoDisplays();
   bool automatic = !settings->dims || !settings->pos_set;

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
   if (display < 0 && !settings->monitor && !automatic)
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
   video_window_place_rect(&rect, &area, automatic);
   out->x = rect.x;
   out->y = rect.y;
   out->w = rect.w;
   out->h = rect.h;
   return display;
}

/* A display came, went or changed. On X11 a window one that went left
 * on none is placed again from its request; the window system may move
 * the others. Neither move is the user's, so neither is saved. */
static void sdl2_video_windows_lost(bool removed)
{
   unsigned i;
   video_window_rect_t areas[VIDEO_WINDOW_DISPLAYS_MAX];
   unsigned n       = 0;
   retro_time_t now = cpu_features_get_time_usec();
   bool x11         = removed
      && string_is_equal(SDL_GetCurrentVideoDriver(), "x11");

   if (!sdl2_video_window_count)
      return;
   if (x11)
   {
      int j;
      int count = SDL_GetNumVideoDisplays();
      for (j = 0; j < count && n < VIDEO_WINDOW_DISPLAYS_MAX; j++)
      {
         SDL_Rect bounds;
         if (SDL_GetDisplayBounds(j, &bounds) != 0)
            continue;
         areas[n].x = bounds.x;
         areas[n].y = bounds.y;
         areas[n].w = bounds.w;
         areas[n].h = bounds.h;
         n++;
      }
   }
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      SDL_Rect rect;
      video_window_rect_t was;
      sdl2_video_window_t *state = sdl2_video_windows[i];
      if (!state || state->common.closed)
         continue;
      SDL_GetWindowPosition(state->win, &was.x, &was.y);
      SDL_GetWindowSize(state->win, &was.w, &was.h);
      if (     !x11
            || !sdl2_video_window_normal(state, SDL_GetWindowFlags(state->win))
            || !video_window_rect_lost(&was, areas, n))
      {
         video_window_unsettled(&state->common, now);
         continue;
      }
      sdl2_video_window_place(state->primary,
            &state->common.request.settings, -1, &rect);
      video_window_placed(&state->common, VIDEO_POS_PACK(rect.x, rect.y),
            VIDEO_SCALE_PACK(rect.w, rect.h), now);
      SDL_SetWindowSize(state->win, rect.w, rect.h);
      SDL_SetWindowPosition(state->win, rect.x, rect.y);
      /* SDL 2 has it where it was put without an event to say so */
      sdl2_video_window_check(state);
   }
}

/* SDL's x11 places a window by its frame, which a window manager puts
 * back some pumps after the window leaves fullscreen */
static bool sdl2_video_window_framed(SDL_Window *win)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
   int top  = 0;
   int left = 0;
   return string_is_equal(SDL_GetCurrentVideoDriver(), "x11")
      && SDL_GetWindowBordersSize(win, &top, &left, NULL, NULL) == 0
      && (top || left);
#else
   return false;
#endif
}

/* Puts the window where its request has it */
static void sdl2_video_window_apply(sdl2_video_window_t *state)
{
   SDL_Rect rect;
   int display                        = -1;
   int via                            = state->via;
   bool left_fullscreen               = state->left_fullscreen;
   SDL_Window *win                    = state->win;
   const video_window_settings_t *now = &state->common.request.settings;

   state->apply           = false;
   state->via             = -1;
   state->left_fullscreen = false;
   /* fullscreen on no monitor in particular is on the one it is on */
   if (now->fullscreen && !now->monitor)
      display = SDL_GetWindowDisplayIndex(win);
   display = sdl2_video_window_place(state->primary, now, display, &rect);
   if (now->fullscreen)
   {
      if (state->fullscreen && SDL_GetWindowDisplayIndex(win) == display)
         return;
      video_window_placed(&state->common, VIDEO_POS_PACK(rect.x, rect.y),
            VIDEO_SCALE_PACK(rect.w, rect.h), cpu_features_get_time_usec());
      /* window managers keep a fullscreen window where it is, so it
       * moves by way of a window, a step at a time: SDL 3's x11 under
       * sdl2-compat waits for a step to be answered before it takes the
       * next */
      if (state->fullscreen)
      {
         SDL_SetWindowFullscreen(win, 0);
         state->fullscreen      = false;
         state->left_fullscreen = true;
         state->waiting         = SDL_WINDOW_FULLSCREEN;
         state->apply           = true;
         return;
      }
      if (via < 0 && !left_fullscreen)
      {
         SDL_GetWindowPosition(win, &state->windowed.x, &state->windowed.y);
         SDL_GetWindowSize(win, &state->windowed.w, &state->windowed.h);
         state->windowed_set = true;
         state->framed       = state->framed
            || sdl2_video_window_framed(win);
      }
      /* SDL fills the display the window is at, or else the one it was
       * last moved to */
      if (     via != display
            && (via >= 0 || SDL_GetWindowDisplayIndex(win) != display))
      {
         SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
               SDL_WINDOWPOS_CENTERED_DISPLAY(display));
         if (     sdl2_video_window_placed()
               && SDL_GetWindowDisplayIndex(win) != display)
         {
            state->via   = display;
            state->apply = true;
            return;
         }
      }
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
      /* a window again with no position of its own: where it stood
       * before, at its own size if it has one, moved onto its monitor */
      if ((!now->dims || !now->pos_set) && state->windowed_set)
      {
         video_window_settings_t place = *now;
         place.pos     = VIDEO_POS_PACK(state->windowed.x,
               state->windowed.y);
         place.pos_set = true;
         if (!now->dims)
            place.dims = VIDEO_SCALE_PACK(state->windowed.w,
                  state->windowed.h);
         sdl2_video_window_place(state->primary, &place, -1, &rect);
      }
      state->windowed_set = false;
      video_window_placed(&state->common, VIDEO_POS_PACK(rect.x, rect.y),
            VIDEO_SCALE_PACK(rect.w, rect.h), cpu_features_get_time_usec());
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
   bool frame;
   /* SDL 3's x11 under sdl2-compat leaves fullscreen when the manager
    * answers; a restore or move before then has SDL wait for it, and a
    * move then asks for fullscreen again */
   if (flags & state->waiting & SDL_WINDOW_FULLSCREEN)
      return;
   if (     (flags & SDL_WINDOW_MAXIMIZED) && !(flags & SDL_WINDOW_MINIMIZED)
         && !(state->waiting & SDL_WINDOW_MAXIMIZED))
   {
      SDL_RestoreWindow(state->win);
      state->waiting |= SDL_WINDOW_MAXIMIZED;
   }
   if (flags & SDL_WINDOW_MINIMIZED)
      return;
   frame = state->framed && !state->fullscreen
      && !state->common.request.settings.fullscreen;
   /* a window manager may keep it maximised, never put its frame back,
    * or keep it off a display on the way to fullscreen */
   if (     (flags & state->waiting)
         || (frame && !sdl2_video_window_framed(state->win))
         || (     state->via >= 0
               && SDL_GetWindowDisplayIndex(state->win) != state->via))
   {
      retro_time_t now = cpu_features_get_time_usec();
      if (!state->wait_by)
         state->wait_by = now + VIDEO_WINDOW_SETTLE_USEC;
      if (now < state->wait_by)
         return;
   }
   if (frame)
      state->framed = false;
   state->wait_by = 0;
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

   if (     request && request->screen < RETRO_VIDEO_VIEWS_MAX
         && sdl2_video_windows[request->screen]
         && !sdl2_video_windows[request->screen]->common.owner)
      sdl2_video_window_let_go(sdl2_video_windows[request->screen]);
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
   state->thread = SDL_ThreadID();

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
         video_window_title_screen(request) + 1);
   if (!(state->win = SDL_CreateWindow(title, rect.x, rect.y, rect.w, rect.h,
               flags)))
   {
      free(state);
      return false;
   }

   state->id             = SDL_GetWindowID(state->win);
   state->primary        = primary;
   state->fullscreen     = request->settings.fullscreen;
   state->via            = -1;
#if SDL_VERSION_ATLEAST(2, 0, 5)
   /* no frame is known for it yet, so one is waited for */
   state->framed         = state->fullscreen
      && string_is_equal(SDL_GetCurrentVideoDriver(), "x11");
#endif
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
   retro_atomic_store_release_size(&sdl2_video_window_wins[request->screen],
         (size_t)(uintptr_t)state->win);
   retro_atomic_store_release_int(&sdl2_video_window_ids[request->screen],
         (int)state->id);

   sdl2_window_set_icon(state->win);
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
   video_window_placed(&state->common, state->common.pos, window->dims,
         cpu_features_get_time_usec());
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
               && now->pos_set    == was->pos_set
               && now->monitor    == was->monitor
               && now->fullscreen == was->fullscreen))
      return;
   state->common.request = *request;
   state->apply          = true;
   /* A drag not yet out does not overwrite this */
   state->judge             = false;
   state->common.save_dims  = 0;
   state->common.save_moved = false;
   /* An edit does not wait for the frame; a fullscreen one, or one
    * while SDL still leaves fullscreen, keeps expecting it */
   if (     !state->fullscreen && !now->fullscreen
         && !(state->waiting & SDL_WINDOW_FULLSCREEN))
      state->framed = false;
   /* It ends a timed wait for SDL, but for a step on the way to
    * fullscreen, which SDL's x11 must finish first */
   if (state->wait_by && state->via < 0)
      state->wait_by = cpu_features_get_time_usec();
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
   Uint32 id = *(const Uint32*)data;
   switch (event->type)
   {
      case SDL_WINDOWEVENT:
         return event->window.windowID != id;
      case SDL_MOUSEMOTION:
         return event->motion.windowID != id;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
         return event->button.windowID != id;
      case SDL_MOUSEWHEEL:
         return event->wheel.windowID != id;
      default:
         break;
   }
   return 1;
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
   /* the input gate's last, so a poll on another thread fails closed */
   retro_atomic_store_release_size(
         &sdl2_video_window_wins[state->common.request.screen], 0);
   retro_atomic_store_release_int(
         &sdl2_video_window_ids[state->common.request.screen], 0);
   /* nor is what is still queued for it, which the main window's
    * handling would take */
   SDL_FilterEvents(sdl2_video_window_unqueue, &id);
   free(state);
   memset(window, 0, sizeof(*window));
}

bool sdl2_video_window_is_screen(const SDL_Window *win, Uint32 id)
{
   unsigned i;
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
      if (     (win && retro_atomic_load_acquire_size(
                  &sdl2_video_window_wins[i]) == (size_t)(uintptr_t)win)
            || (id  && (Uint32)retro_atomic_load_acquire_int(
                  &sdl2_video_window_ids[i]) == id))
         return true;
   return false;
}

/* On Windows a window is its thread's, which alone takes it or lets it
 * go; only the main thread keeps one, as that outlasts the driver */
static bool sdl2_video_window_foreign(const sdl2_video_window_t *state)
{
#ifdef _WIN32
   return state->thread != SDL_ThreadID() || !task_is_on_main_thread();
#else
   return false;
#endif
}

static void sdl2_video_window_let_go(sdl2_video_window_t *state)
{
   video_window_native_t window;
   if (sdl2_video_window_foreign(state))
      return;
   window               = state->common.kept;
   state->common.native = &window;
   sdl2_video_window_destroy(NULL, &window);
   SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void sdl2_video_windows_release_kept(void)
{
   unsigned i;
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
      if (sdl2_video_windows[i] && !sdl2_video_windows[i]->common.owner)
         sdl2_video_window_let_go(sdl2_video_windows[i]);
}

void sdl2_video_window_keep(void *owner, video_window_native_t *window)
{
   sdl2_video_window_t *state = window
      ? (sdl2_video_window_t*)window->state : NULL;
   if (!state || state->common.owner != owner)
      return;
   if (     sdl2_video_window_foreign(state)
         || SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
      sdl2_video_window_destroy(owner, window);
   else
      video_window_hold(&state->common, window);
}

bool sdl2_video_window_take(void *owner, unsigned screen,
      video_window_native_t *window, unsigned *display)
{
   sdl2_video_window_t *state = screen < RETRO_VIDEO_VIEWS_MAX
      ? sdl2_video_windows[screen] : NULL;
   /* One the user closed meanwhile is taken too, for its close to land */
   if (     !owner || !state || state->common.owner || !window
         || window->state || sdl2_video_window_foreign(state))
      return false;
   video_window_adopt(&state->common, owner, window);
   *display = state->common.request.display;
   /* The context holds the subsystem now */
   SDL_QuitSubSystem(SDL_INIT_VIDEO);
   if (     state->common.kept_focus && window->presentable
         && !state->common.closed)
      SDL_RaiseWindow(state->win);
   return true;
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
