/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
 *  Copyright (C)      2026 - Rob Loach
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

#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#ifdef HAVE_VIDEO_WINDOWS
#include <encodings/utf.h>
#endif

#include <SDL3/SDL.h>

#include "sdl3_common.h"
#include "../../configuration.h"
#include "../../input/input_driver.h"
#include "../../retroarch.h"
#include "../../version.h"

/* 16x16 ARGB8888 window icon, matching x_ctx.c, wayland_common.c, ui_qt.cpp. */
static const uint32_t sdl3_icon_data[16 * 16] = {
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

void sdl3_set_app_metadata(void)
{
   SDL_SetAppMetadata("RetroArch", PACKAGE_VERSION, "com.libretro.RetroArch");
   SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, "libretro");
   SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, "https://www.retroarch.com");
   SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");
}

/* Sets the window icon so that the RetroArch icon appears in the taskbar. */
static void sdl3_window_set_icon(SDL_Window *win)
{
   SDL_Surface *icon = SDL_CreateSurfaceFrom(16, 16,
         SDL_PIXELFORMAT_ARGB8888,
         (void*)sdl3_icon_data, 16 * sizeof(uint32_t));

   if (!icon)
      return;

   SDL_SetWindowIcon(win, icon);
   SDL_DestroySurface(icon);
}

/* Keeps track of the window position to allow video_window_save_positions. */
static void sdl3_window_save_position(SDL_Window *win)
{
   int x, y, w, h;
   settings_t *settings = config_get_ptr();

   if (!win || !settings || !settings->bools.video_window_save_positions)
      return;

   if (SDL_GetWindowFlags(win) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))
      return;

   if (!SDL_GetWindowPosition(win, &x, &y) || !SDL_GetWindowSize(win, &w, &h)
       || w <= 0 || h <= 0)
      return;

   settings->uints.window_position_pos  = VIDEO_POS_PACK(x, y);
   settings->uints.window_position_dims = VIDEO_SCALE_PACK(w, h);
}

#ifdef HAVE_VIDEO_WINDOWS
/* Secondary screens' windows, by screen */
static sdl3_video_window_t *sdl3_video_windows[RETRO_VIDEO_VIEWS_MAX];
static unsigned sdl3_video_window_count;

/* The displays last published for the screens' Display setting and
 * their ids, on the context's thread. dirty: enumerate again;
 * own_dirty: the main window may be on another one. */
static video_window_display_t sdl3_video_monitors[VIDEO_WINDOW_DISPLAYS_MAX];
static SDL_DisplayID sdl3_video_monitor_ids[VIDEO_WINDOW_DISPLAYS_MAX];
static unsigned sdl3_video_monitor_count;
static bool sdl3_video_monitors_dirty;
static bool sdl3_video_monitors_own_dirty;

static video_window_common_t *sdl3_video_window_at(unsigned screen)
{
   return sdl3_video_windows[screen]
      ? &sdl3_video_windows[screen]->common : NULL;
}

/* Where the compositor places windows, a position is not theirs */
static bool sdl3_video_window_placed(void)
{
   return !string_is_equal(SDL_GetCurrentVideoDriver(), "wayland");
}

/* Where it is, its size and whether it is presentable, as SDL has them
 * now: a minimised, hidden or suspended window is skipped, never closed */
static void sdl3_video_window_check(sdl3_video_window_t *state)
{
   int x, y;
   bool presentable;
   int w                         = 0;
   int h                         = 0;
   video_window_native_t *window = state->common.native;
   SDL_WindowFlags flags         = SDL_GetWindowFlags(state->win);
   if (     !(flags & SDL_WINDOW_MINIMIZED)
         && sdl3_video_window_placed()
         && SDL_GetWindowPosition(state->win, &x, &y)
         && VIDEO_POS_PACK(x, y) != state->common.pos)
   {
      state->common.pos      = VIDEO_POS_PACK(x, y);
      state->common.pending |= VIDEO_WINDOW_EVENT_BIT(GEOMETRY);
   }
   if (     !(flags & SDL_WINDOW_MINIMIZED)
         && SDL_GetWindowSizeInPixels(state->win, &w, &h)
         && w > 0 && h > 0
         && VIDEO_SCALE_PACK(w, h) != window->dims)
   {
      window->dims           = VIDEO_SCALE_PACK(w, h);
      state->common.pending |= VIDEO_WINDOW_EVENT_BIT(GEOMETRY);
   }
   presentable = w > 0 && h > 0 && !(flags & (SDL_WINDOW_MINIMIZED
         | SDL_WINDOW_HIDDEN | SDL_WINDOW_OCCLUDED));
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

/* A secondary's window event goes to it alone. The main window's close
 * request quits as SDL would for its last window: SDL counted windows
 * when it pumped, and a secondary destroyed since may have kept it
 * from quitting. */
static bool sdl3_video_window_event(const SDL_Event *event, bool *quit)
{
   unsigned i;
   video_window_native_t *window;
   sdl3_video_window_t *state = NULL;

   for (i = 1; sdl3_video_window_count && i < RETRO_VIDEO_VIEWS_MAX
         && !state; i++)
      if (     sdl3_video_windows[i]
            && sdl3_video_windows[i]->id == event->window.windowID)
         state = sdl3_video_windows[i];
   if (!state)
   {
      if (     event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
            && SDL_GetHintBoolean(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, true))
         *quit = true;
      return false;
   }
   if (state->common.closed)
      return true;
   window = state->common.native;

   switch (event->type)
   {
      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
         video_window_mark_closed(&state->common);
         return true;
      case SDL_EVENT_WINDOW_FOCUS_GAINED:
         window->focused        = true;
         state->common.pending |= VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN);
         break;
      case SDL_EVENT_WINDOW_FOCUS_LOST:
         window->focused        = false;
         state->common.pending &= ~VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN);
         state->common.pending |=  VIDEO_WINDOW_EVENT_BIT(FOCUS_LOSS);
         break;
      default:
         break;
   }
   sdl3_video_window_check(state);
   return true;
}
#endif

/* Events nothing reads, which would otherwise fill the queue */
static void sdl3_flush_unread_events(void)
{
   SDL_FlushEvents(SDL_EVENT_TERMINATING,   SDL_EVENT_SYSTEM_THEME_CHANGED);
#if SDL_VERSION_ATLEAST(3, 4, 0)
   SDL_FlushEvents(SDL_EVENT_PINCH_BEGIN,   SDL_EVENT_PINCH_END);
#endif
   SDL_FlushEvent(SDL_EVENT_CLIPBOARD_UPDATE);
   SDL_FlushEvents(SDL_EVENT_DROP_FILE,     SDL_EVENT_DROP_POSITION);
   SDL_FlushEvents(SDL_EVENT_AUDIO_DEVICE_ADDED,
         SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED);
   SDL_FlushEvents(SDL_EVENT_RENDER_TARGETS_RESET,
         SDL_EVENT_RENDER_DEVICE_LOST);
}

bool sdl3_pump_input_events(void)
{
   /* The joypad and input polls run back to back: one backend round
    * trip serves both. */
   static Uint64 last_ns = 0;
   Uint64 now_ns;

   /* Two threads pumping at once deadlock in the X11 backend. */
   if (     video_driver_thread_wrapper_active()
         && SDL_WasInit(SDL_INIT_VIDEO))
      return false;
   now_ns = SDL_GetTicksNS();
   if (last_ns && now_ns - last_ns < SDL_NS_PER_MS)
      return true;
   last_ns = now_ns;
   SDL_PumpEvents();
   sdl3_flush_unread_events();
   return true;
}

void sdl3_pump_window_events(bool *quit, bool *resize)
{
   SDL_Event event;

   SDL_PumpEvents();

   /* Only consume quit and window events here. The SDL3 input driver
    * handles the keyboard/mouse events within the same queue. */
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0)
      *quit = true;

   /* Check if the display size was changed. */
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_FIRST, SDL_EVENT_WINDOW_LAST) > 0)
   {
#ifdef HAVE_VIDEO_WINDOWS
      if (sdl3_video_window_event(&event, quit))
         continue;
#endif

      if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
         *resize = true;

      if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_MOVED)
         sdl3_window_save_position(SDL_GetWindowFromID(event.window.windowID));

      /* It may be on another display now */
      if (     event.type == SDL_EVENT_WINDOW_MOVED
            || event.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED)
      {
         video_driver_window_output_changed();
#ifdef HAVE_VIDEO_WINDOWS
         sdl3_video_monitors_own_dirty = true;
#endif
      }
   }

   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_DISPLAY_FIRST, SDL_EVENT_DISPLAY_LAST) > 0)
   {
      if (event.type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED)
         *resize = true;
#ifdef HAVE_VIDEO_WINDOWS
      else if (   event.type == SDL_EVENT_DISPLAY_ADDED
               || event.type == SDL_EVENT_DISPLAY_REMOVED
               || event.type == SDL_EVENT_DISPLAY_MOVED
               || event.type == SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED)
         sdl3_video_monitors_dirty = true;
#endif
   }

    /* Clear out the input queue if we're not using the
     * SDL driver. */
   if (!input_driver_is_sdl3())
   {
      SDL_FlushEvents(SDL_EVENT_KEY_DOWN,         SDL_EVENT_MOUSE_REMOVED);
      SDL_FlushEvents(SDL_EVENT_FINGER_DOWN,      SDL_EVENT_FINGER_CANCELED);
      SDL_FlushEvents(SDL_EVENT_PEN_PROXIMITY_IN, SDL_EVENT_PEN_AXIS);
   }

   sdl3_flush_unread_events();
}

void sdl3_set_handles(SDL_Window *window)
{
   SDL_PropertiesID props = SDL_GetWindowProperties(window);
   const char *sdl_driver = SDL_GetCurrentVideoDriver();
   enum rarch_display_type display_type = RARCH_DISPLAY_NONE;

   if (!props)
      return;

   if (string_is_equal(sdl_driver, "windows"))
      display_type = RARCH_DISPLAY_WIN32;
   else if (string_is_equal(sdl_driver, "cocoa"))
      display_type = RARCH_DISPLAY_OSX;
   else if (string_is_equal(sdl_driver, "x11"))
      display_type = RARCH_DISPLAY_X11;
   else if (string_is_equal(sdl_driver, "wayland"))
      display_type = RARCH_DISPLAY_WAYLAND;
   else if (string_is_equal(sdl_driver, "kmsdrm"))
      display_type = RARCH_DISPLAY_KMS;

   video_driver_display_userdata_set((uintptr_t)window);

   switch (display_type)
   {
      case RARCH_DISPLAY_WIN32:
#if defined(_WIN32)
         video_driver_display_type_set(RARCH_DISPLAY_WIN32);
         video_driver_display_set(0);
         video_driver_window_set((uintptr_t)SDL_GetPointerProperty(props,
               SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL));
#endif
         break;
      case RARCH_DISPLAY_X11:
#if defined(HAVE_X11)
         video_driver_display_type_set(RARCH_DISPLAY_X11);
         video_driver_display_set((uintptr_t)SDL_GetPointerProperty(props,
               SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL));
         video_driver_window_set((uintptr_t)SDL_GetNumberProperty(props,
               SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
#endif
         break;
      case RARCH_DISPLAY_OSX:
#ifdef HAVE_COCOA
         video_driver_display_type_set(RARCH_DISPLAY_OSX);
         video_driver_display_set(0);
         video_driver_window_set((uintptr_t)SDL_GetPointerProperty(props,
               SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL));
#endif
         break;
      case RARCH_DISPLAY_WAYLAND:
#if defined(HAVE_WAYLAND)
         video_driver_display_type_set(RARCH_DISPLAY_WAYLAND);
         video_driver_display_set((uintptr_t)SDL_GetPointerProperty(props,
               SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL));
         video_driver_window_set((uintptr_t)SDL_GetPointerProperty(props,
               SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL));
#endif
         break;
      case RARCH_DISPLAY_KMS:
#if defined(HAVE_KMS)
         /* SDL3's kmsdrm backend owns the DRM device; expose the fd so
          * the KMS display server can query it, mirroring drm_ctx. */
         video_driver_display_type_set(RARCH_DISPLAY_KMS);
         video_driver_display_set((uintptr_t)SDL_GetNumberProperty(props,
               SDL_PROP_WINDOW_KMSDRM_DRM_FD_NUMBER, 0));
         video_driver_window_set(0);
#endif
         break;
      default:
      case RARCH_DISPLAY_NONE:
         break;
   }
}

/* The display the 1-based @monitor_index names, or 0 for the index
 * the system chooses - none configured, or one that is not there. */
static SDL_DisplayID sdl3_monitor_display(unsigned monitor_index)
{
   int count;
   SDL_DisplayID *displays;
   SDL_DisplayID display = 0;

   if (monitor_index == 0)
      return 0;

   displays = SDL_GetDisplays(&count);
   if (displays && (int)monitor_index <= count)
      display = displays[monitor_index - 1];
   SDL_free(displays);

   return display;
}

#ifdef HAVE_VIDEO_WINDOWS
/* In SDL_GetDisplays() order, which a window's monitor setting counts
 * in (sdl3_video_window_place()). Enumerated only when dirty; else
 * only the main window's is found again. An @init list always goes
 * out: another driver's may be up. */
void sdl3_video_monitors_publish(SDL_Window *primary, bool init)
{
   video_window_display_t list[VIDEO_WINDOW_DISPLAYS_MAX];
   unsigned i;
   SDL_DisplayID own;
   unsigned count = sdl3_video_monitor_count;

   if (init)
      sdl3_video_monitors_dirty = true;
   else if (!sdl3_video_monitors_dirty && !sdl3_video_monitors_own_dirty)
      return;
   own = primary ? SDL_GetDisplayForWindow(primary) : 0;

   if (sdl3_video_monitors_dirty)
   {
      int n              = 0;
      SDL_DisplayID *ids = SDL_GetDisplays(&n);
      count              = 0;
      for (i = 0; ids && (int)i < n && count < VIDEO_WINDOW_DISPLAYS_MAX;
            i++)
      {
         SDL_Rect bounds;
         video_window_display_t *display = &list[count];
         const char *name                = SDL_GetDisplayName(ids[i]);
         memset(display, 0, sizeof(*display));
         sdl3_video_monitor_ids[count]   = ids[i];
         display->ordinal                = ++count;
         display->valid                  = true;
         display->on                     = true;
         display->physical               = true;
         display->monitor                = true;
         if (name)
            utf8cpy(display->name, sizeof(display->name), name,
                  sizeof(display->name));
         if (     SDL_GetDisplayBounds(ids[i], &bounds)
               && VIDEO_SCALE_FITS(bounds.w, bounds.h))
            display->dims = VIDEO_SCALE_PACK(bounds.w, bounds.h);
      }
      SDL_free(ids);
   }
   else
      memcpy(list, sdl3_video_monitors, count * sizeof(*list));
   for (i = 0; i < count; i++)
      list[i].own = own && sdl3_video_monitor_ids[i] == own;
   sdl3_video_monitors_dirty     = false;
   sdl3_video_monitors_own_dirty = false;
   if (     !init
         && count == sdl3_video_monitor_count
         && !memcmp(list, sdl3_video_monitors, count * sizeof(*list)))
      return;
   memcpy(sdl3_video_monitors, list, count * sizeof(*list));
   sdl3_video_monitor_count = count;
   video_driver_publish_window_displays(list, count);
}
#endif

/* Moves the window to the provided 1-based monitor. If 0
 * is provided, leave as is. */
static void sdl3_window_move_to_monitor(SDL_Window *win,
      unsigned monitor_index)
{
   SDL_DisplayID display;

   if (!win || monitor_index == 0)
      return;

   /* Out-of-range index (e.g. the configured monitor was unplugged):
    * leave the window where the system placed it. */
   if (!(display = sdl3_monitor_display(monitor_index)))
      return;

   SDL_SetWindowPosition(win,
         SDL_WINDOWPOS_CENTERED_DISPLAY(display),
         SDL_WINDOWPOS_CENTERED_DISPLAY(display));
   SDL_SyncWindow(win);
}

/* The display a window's size and mode are decided against: the one it
 * is on once it exists - the user may have dragged it - and before
 * that the configured monitor, which is where it is put. */
static SDL_DisplayID sdl3_target_display(SDL_Window *win)
{
   SDL_DisplayID display;

   if (win)
      return SDL_GetDisplayForWindow(win);

   display = sdl3_monitor_display(config_get_ptr()->uints.video_monitor_index);
   return display ? display : SDL_GetPrimaryDisplay();
}

/* Creates the SDL3 window. */
static SDL_Window *sdl3_window_create(unsigned width, unsigned height,
      bool fullscreen, SDL_WindowFlags backend_flags)
{
   SDL_Window *win;
   settings_t *settings = config_get_ptr();
   SDL_WindowFlags flags = backend_flags
         | SDL_WINDOW_RESIZABLE
         | SDL_WINDOW_HIGH_PIXEL_DENSITY;

   if (fullscreen)
   {
      flags |= SDL_WINDOW_FULLSCREEN;
      /* Desktop full screen at the desktop's size: a window made
       * smaller is only grown by a window manager, and without one it
       * keeps the size it was made at. The size is the configured
       * monitor's, which is where the window goes below. So is a 0x0
       * size's, which asks for the desktop's. */
      if (settings->bools.video_windowed_fullscreen || !width || !height)
      {
         const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(
               sdl3_target_display(NULL));
         if (mode)
         {
            width  = (unsigned)mode->w;
            height = (unsigned)mode->h;
         }
      }
   }

   if (!settings->bools.video_window_show_decorations)
      flags |= SDL_WINDOW_BORDERLESS;

   if (!(win = SDL_CreateWindow("RetroArch", width, height, flags)))
      return NULL;

   sdl3_window_set_icon(win);

   /* Set either the window position, or the active monitor. This is ignored
    * in Wayland where windows are not manually positioned. */
   if (settings->bools.video_window_save_positions
         && !fullscreen
         && VIDEO_SCALE_W(settings->uints.window_position_dims)
         && VIDEO_SCALE_H(settings->uints.window_position_dims)
         && !string_is_equal(SDL_GetCurrentVideoDriver(), "wayland"))
   {
      SDL_SetWindowPosition(win,
            VIDEO_POS_X(settings->uints.window_position_pos),
            VIDEO_POS_Y(settings->uints.window_position_pos));
      SDL_SyncWindow(win);
   }
   else
      sdl3_window_move_to_monitor(win, settings->uints.video_monitor_index);

   /* SDL_EVENT_TEXT_INPUT is emitted for windows that opted in for
    * it. The SDL3 input driver handles those events for menu
    * text entry and core keyboard callbacks. */
#ifndef WEBOS
   if (!sdl3_uses_screen_keyboard())
      SDL_StartTextInput(win);
#endif

   return win;
}

/* Applies the fullscreen state. */
static void sdl3_window_apply_fullscreen(SDL_Window *win,
      unsigned width, unsigned height, bool fullscreen)
{
   SDL_DisplayMode mode;
   settings_t *settings = config_get_ptr();
   SDL_DisplayID display = sdl3_target_display(win);
   unsigned mode_w       = width;
   unsigned mode_h       = height;
   bool exclusive;

   /* 0x0 asks for the desktop's size */
   if (fullscreen && (!mode_w || !mode_h))
   {
      const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(display);
      if (desktop)
      {
         mode_w = (unsigned)desktop->w;
         mode_h = (unsigned)desktop->h;
      }
   }

   exclusive = fullscreen
         && !settings->bools.video_windowed_fullscreen
         && SDL_GetClosestFullscreenDisplayMode(display,
               (int)mode_w, (int)mode_h,
               settings->floats.video_refresh_rate, false, &mode);

   SDL_SetWindowFullscreenMode(win, exclusive ? &mode : NULL);
   SDL_SetWindowFullscreen(win, fullscreen);

   /* The cursor is the frontend's, applied through sdl3_show_mouse(). */
   if (!fullscreen)
   {
      /* Leaving fullscreen is asynchronous, and size requests
       * are dropped while still fullscreen, so we wait until
       * it has completed. */
      SDL_SyncWindow(win);
      SDL_SetWindowSize(win, width, height);
   }
}

bool sdl3_window_set_video_mode(SDL_Window **win,
      unsigned dims, bool fullscreen,
      SDL_WindowFlags backend_flags)
{
   unsigned width  = VIDEO_SCALE_W(dims);
   unsigned height = VIDEO_SCALE_H(dims);
   if (*win)
      SDL_SetWindowBordered(*win, config_get_ptr()->bools.video_window_show_decorations);
   else if (!(*win = sdl3_window_create(width, height, fullscreen, backend_flags)))
      return false;

   sdl3_window_apply_fullscreen(*win, width, height, fullscreen);

   sdl3_set_handles(*win);

   return true;
}

void sdl3_window_get_video_size(SDL_Window *win,
      unsigned *dims)
{
   const SDL_DisplayMode *mode;

   if (win)
   {
      int w, h;
      SDL_GetWindowSizeInPixels(win, &w, &h);
      *dims = VIDEO_SCALE_PACK(w, h);
      return;
   }

   /* The window doesn't exist yet, so report the desktop size, which
    * a mode gives in points. */
   mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
   if (mode)
   {
      float d = mode->pixel_density > 0.0f ? mode->pixel_density : 1.0f;
      *dims   = VIDEO_SCALE_PACK((unsigned)(mode->w * d + 0.5f),
            (unsigned)(mode->h * d + 0.5f));
   }
}

float sdl3_window_get_refresh_rate(SDL_Window *win)
{
   const SDL_DisplayMode *mode;

   if (!win)
      return 0.0f;

   mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(win));
   return mode ? mode->refresh_rate : 0.0f;
}

void sdl3_window_update_title(SDL_Window *win)
{
   char title[128];

   if (!win)
      return;

   title[0] = '\0';
   video_driver_get_window_title(title, sizeof(title));

   if (title[0])
      SDL_SetWindowTitle(win, title);
}

bool sdl3_window_has_focus(SDL_Window *win)
{
   if (!win)
      return false;
   /* This only checks for keyboard focus, as SDL_WINDOW_MOUSE_FOCUS will
    * pause when the mouse leaves the window too. */
   return (SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

bool sdl3_suppress_screensaver(void *data, bool enable)
{
   return enable ? SDL_DisableScreenSaver() : SDL_EnableScreenSaver();
}

void sdl3_input_driver(const char *joypad_name)
{
   /* no input driver of this window's own: the frontend starts the
    * one that goes with an SDL 3 window */
   input_driver_video_window(INPUT_WINDOW_SDL3, NULL);
}

void sdl3_ctx_input_driver(void *data,
      const char *name)
{
   sdl3_input_driver(name);
}

bool sdl3_next_window_is_sdl3(void)
{
   /* The driver in front, and through it the context the drivers ask
    * sdl3_ctx_enabled() about. A core's gl/glcore/vulkan request
    * swaps the setting and keeps the user's choice in
    * cached_driver_id, which sdl3_ctx_enabled() honours. A driver
    * with no context driver (d3d*, metal, ...) never makes an SDL3
    * window, whatever the context setting still says. */
   const char *video = config_get_ptr()->arrays.video_driver;
   if (string_is_equal(video, "sdl3"))
      return true;
   if (string_is_equal(video, "vulkan"))
      return sdl3_ctx_enabled("vk_sdl3");
   if (     string_is_equal(video, "gl")
         || string_is_equal(video, "glcore")
         || string_is_equal(video, "gl1"))
      return sdl3_ctx_enabled("gl_sdl3");
   return false;
}

bool sdl3_ctx_enabled(const char *ctx_ident)
{
   settings_t *settings = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();

   /* Determine the user's desired video driver. When a core requests
    * gl/glcore/vulkan, the user's original selection is stashed in
    * cached_driver_id. Use that if it's available. */
   const char *video_ident = video_st->cached_driver_id[0]
         ? video_st->cached_driver_id
         : settings->arrays.video_driver;

   /* Keep the hardware rendering within the SDL3 window. When SDL3 isn't
    * selected, will pass off to the next native context instead. */
   return string_is_equal(video_ident, "sdl3")
       || string_is_equal(settings->arrays.video_context_driver, ctx_ident);
}

/* Shared ctx callbacks only need the SDL_Window*, which
 * every SDL3 driver data struct keeps as its first member. */
static SDL_Window *sdl3_ctx_window(void *data)
{
   return data ? *(SDL_Window**)data : NULL;
}

void sdl3_ctx_get_video_size(void *data, unsigned *dims)
{
   if (data)
      sdl3_window_get_video_size(sdl3_ctx_window(data), dims);
}

float sdl3_ctx_get_refresh_rate(void *data)
{
   return sdl3_window_get_refresh_rate(sdl3_ctx_window(data));
}

void sdl3_ctx_update_title(void *data)
{
   sdl3_window_update_title(sdl3_ctx_window(data));
}

bool sdl3_ctx_has_focus(void *data)
{
   return sdl3_window_has_focus(sdl3_ctx_window(data));
}

void sdl3_ctx_check_window(void *data, bool *quit, bool *resize,
      unsigned *dims)
{
   SDL_Window *win = sdl3_ctx_window(data);

   sdl3_pump_window_events(quit, resize);
#ifdef HAVE_VIDEO_WINDOWS
   sdl3_video_monitors_publish(win, false);
#endif

   if (*resize && win)
      sdl3_window_get_video_size(win, dims);
}

bool sdl3_ctx_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   float scale;
   SDL_Window *win;

   /* This currently only reports on the display's metric DPI. */
   if (type != DISPLAY_METRIC_DPI)
   {
      *value = 0.0f;
      return false;
   }

   /* Find the scale from the window, or the primary display if the
    * window doesn't exist yet. */
   if ((win = sdl3_ctx_window(data)))
      scale = SDL_GetWindowDisplayScale(win);
   else
      scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

   /* SDL3 only exposes the desktop scale factor, not the physical
    * DPI, so use a 96 DPI baseline. For reference, X11 has Xft.dpi,
    * and Wayland uses the compositor scale. */
   if (scale <= 0.0f)
   {
      *value = 0.0f;
      return false;
   }

   *value = scale * 96.0f;
   return true;
}

void sdl3_show_mouse(void *data, bool state)
{
   if (state)
      SDL_ShowCursor();
   else
      SDL_HideCursor();
}

SDL_Window *sdl3_get_window(void)
{
   gfx_ctx_ident_t ident_info;

   if (string_is_equal(video_driver_get_ident(), "sdl3"))
   {
      sdl3_video_t *video_ptr = (sdl3_video_t*)video_driver_get_ptr();
      return video_ptr != NULL ? video_ptr->window : NULL;
   }

   /* gl/gl1/glcore/vulkan running on the SDL3 context drivers. They
    * register their SDL_Window as the display userdata via sdl3_set_handles. */
   ident_info.ident = NULL;
   video_context_driver_get_ident(&ident_info);
   if (string_is_equal(ident_info.ident, "gl_sdl3") || string_is_equal(ident_info.ident, "vk_sdl3"))
      return (SDL_Window*)video_driver_display_userdata_get();

   return NULL;
}

/* Whether SDL_StartTextInput() on this device means "pop up a screen
 * keyboard" rather than "quietly enable text events".
 *
 * SDL_HasScreenKeyboardSupport() alone is not that question. The X11
 * backend answers it true for the whole Steam Deck whether or not a
 * USB keyboard is plugged in, and the GDK and OpenVR backends do the
 * same; only Wayland checks for a physical keyboard first. Where one
 * is attached the user is typing on it, so keep text input running in
 * the background as on any desktop - otherwise SDL_EVENT_TEXT_INPUT
 * never fires, sdl3_translate_control_key() returns 0 for printable
 * keys, and nothing the user types reaches the keyboard line. */
bool sdl3_uses_screen_keyboard(void)
{
   return SDL_HasScreenKeyboardSupport() && !SDL_HasKeyboard();
}

#ifdef HAVE_VIDEO_WINDOWS
/* Where 'settings' put a window, in SDL's window coordinates: on
 * 'display' if given, else the preferred monitor (SDL's display N-1,
 * as for the main window), else with an automatic monitor the one the
 * saved bounds are mostly on, else the main window's, else the primary
 * display.
 * Saved bounds partly off it are moved onto it; bounds not on it at
 * all, and automatic ones, are centred on it. Fullscreen covers it.
 * 'density' turns the saved size, in pixels, into window coordinates. */
static SDL_DisplayID sdl3_video_window_place(SDL_Window *primary,
      const video_window_settings_t *settings, SDL_DisplayID display,
      float density, SDL_Rect *out)
{
   SDL_Rect bounds;
   video_window_rect_t rect, area;
   int i, count            = 0;
   SDL_DisplayID *displays = SDL_GetDisplays(&count);

   if (density <= 0.0f)
      density = 1.0f;
   out->x = VIDEO_POS_X(settings->pos);
   out->y = VIDEO_POS_Y(settings->pos);
   out->w = (int)(VIDEO_SCALE_W(settings->dims) / density);
   out->h = (int)(VIDEO_SCALE_H(settings->dims) / density);
   if (     !settings->dims
         && (  !SDL_GetWindowSize(primary, &out->w, &out->h)
            || out->w <= 0 || out->h <= 0))
   {
      out->w = 640;
      out->h = 480;
   }

   if (!displays)
      count = 0;
   if (!display && settings->monitor && (int)settings->monitor <= count)
      display = displays[settings->monitor - 1];
   if (!display && !settings->monitor && settings->dims)
   {
      int best = 0;
      for (i = 0; i < count; i++)
      {
         SDL_Rect inter;
         if (     SDL_GetDisplayBounds(displays[i], &bounds)
               && SDL_GetRectIntersection(out, &bounds, &inter)
               && inter.w * inter.h > best)
         {
            best    = inter.w * inter.h;
            display = displays[i];
         }
      }
   }
   SDL_free(displays);
   if (!display)
      display = SDL_GetDisplayForWindow(primary);
   if (!display)
      display = SDL_GetPrimaryDisplay();
   if (!SDL_GetDisplayBounds(display, &bounds))
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
static void sdl3_video_window_apply(sdl3_video_window_t *state)
{
   SDL_Rect rect;
   SDL_DisplayID display              = 0;
   SDL_Window *win                    = state->win;
   const video_window_settings_t *now = &state->common.request.settings;

   state->apply = false;
   /* fullscreen on no monitor in particular is on the one it is on */
   if (now->fullscreen && !now->monitor)
      display = SDL_GetDisplayForWindow(win);
   display = sdl3_video_window_place(sdl3_ctx_window(state->common.owner),
         now, display, SDL_GetWindowPixelDensity(win), &rect);
   if (now->fullscreen)
   {
      if (state->fullscreen && SDL_GetDisplayForWindow(win) == display)
         return;
      /* window managers keep a fullscreen window where it is, so it
       * moves by way of a window */
      if (state->fullscreen)
         SDL_SetWindowFullscreen(win, false);
      else
         state->windowed_set =
               SDL_GetWindowPosition(win, &state->windowed.x,
                  &state->windowed.y)
            && SDL_GetWindowSize(win, &state->windowed.w,
                  &state->windowed.h);
      /* SDL fills the display the window is at */
      if (SDL_GetDisplayForWindow(win) != display)
         SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
               SDL_WINDOWPOS_CENTERED_DISPLAY(display));
      SDL_SetWindowFullscreen(win, true);
   }
   else if (state->fullscreen)
   {
      /* placed once SDL has it as a window again, frame and all */
      SDL_SetWindowFullscreen(win, false);
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
}

/* Its request is made once SDL has the window restored, and placed as
 * a window once SDL has it so: from its restored size, never its
 * minimised or maximised one */
static void sdl3_video_window_settle(sdl3_video_window_t *state)
{
   SDL_WindowFlags flags = SDL_GetWindowFlags(state->win);
   if (     (flags & SDL_WINDOW_MAXIMIZED) && !(flags & SDL_WINDOW_MINIMIZED)
         && !(state->waiting & SDL_WINDOW_MAXIMIZED))
   {
      SDL_RestoreWindow(state->win);
      state->waiting |= SDL_WINDOW_MAXIMIZED;
   }
   if (flags & (SDL_WINDOW_MINIMIZED | state->waiting))
      return;
   state->waiting = 0;
   sdl3_video_window_apply(state);
}

/* SDL documents SDL_GetCurrentVideoDriver() for the main thread only */
static retro_atomic_int_t sdl3_video_windows_ok;

void sdl3_video_windows_probe(void)
{
   const char *driver = SDL_GetCurrentVideoDriver();
   retro_atomic_store_release_int(&sdl3_video_windows_ok,
             string_is_equal(driver, "x11")
          || string_is_equal(driver, "wayland")
          || string_is_equal(driver, "windows"));
}

bool sdl3_video_windows_supported(void)
{
   return retro_atomic_load_acquire_int(&sdl3_video_windows_ok) != 0;
}

bool sdl3_video_window_create(void *owner, SDL_WindowFlags flags,
      const video_window_request_t *request, video_window_native_t *window)
{
   char title[64];
   SDL_Rect rect;
   int w, h;
   SDL_DisplayID display;
   SDL_PropertiesID props;
   video_window_settings_t windowed;
   sdl3_video_window_t *state;
   SDL_Window *primary = sdl3_ctx_window(owner);

   if (     !primary || !request || !window || window->state
         || !sdl3_video_windows_supported()
         || !request->generation
         || !request->screen
         || request->screen >= RETRO_VIDEO_VIEWS_MAX
         || request->settings.mode != VIDEO_WINDOW_SEPARATE
         || sdl3_video_windows[request->screen]
         || (request->settings.dims
            && (  !VIDEO_SCALE_W(request->settings.dims)
               || !VIDEO_SCALE_H(request->settings.dims))))
      return false;
   if (!(state = (sdl3_video_window_t*)calloc(1, sizeof(*state))))
      return false;

   /* made at its window size, filling the display from there */
   windowed            = request->settings;
   windowed.fullscreen = false;
   display = sdl3_video_window_place(primary, &windowed, 0,
         SDL_GetWindowPixelDensity(primary), &rect);
   flags  |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
      | SDL_WINDOW_HIDDEN;
   if (request->settings.fullscreen)
   {
      flags  |= SDL_WINDOW_FULLSCREEN;
      rect.x  = rect.y = SDL_WINDOWPOS_CENTERED_DISPLAY(display);
   }
   snprintf(title, sizeof(title), "RetroArch - Screen %u",
         request->screen + 1);
   props = SDL_CreateProperties();
   SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
   SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, flags);
   SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, rect.x);
   SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, rect.y);
   SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, rect.w);
   SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
         rect.h);
   state->win = SDL_CreateWindowWithProperties(props);
   SDL_DestroyProperties(props);
   if (!state->win)
   {
      free(state);
      return false;
   }

   state->id             = SDL_GetWindowID(state->win);
   state->fullscreen     = request->settings.fullscreen;
   state->common.owner   = owner;
   state->common.native  = window;
   state->common.request = *request;
   memset(window, 0, sizeof(*window));
   window->state         = state;
   window->surface       = state->win;
   window->kind          = VIDEO_WINDOW_NATIVE_SDL3;
   window->generation    = request->generation;
   window->configured    = true;
   sdl3_video_windows[request->screen] = state;
   sdl3_video_window_count++;

   sdl3_window_set_icon(state->win);
   /* the main window keeps the focus */
   SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
   SDL_ShowWindow(state->win);
   SDL_ResetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN);

   /* as it came up; its events keep these from here on */
   if (     sdl3_video_window_placed()
         && SDL_GetWindowPosition(state->win, &rect.x, &rect.y))
      state->common.pos = VIDEO_POS_PACK(rect.x, rect.y);
   if (SDL_GetWindowSizeInPixels(state->win, &w, &h))
      window->dims = VIDEO_SCALE_PACK(w, h);
   sdl3_video_window_check(state);
   return true;
}

void sdl3_video_window_update(void *owner, video_window_native_t *window,
      const video_window_request_t *request)
{
   const video_window_settings_t *now;
   const video_window_settings_t *was;
   sdl3_video_window_t *state = window
      ? (sdl3_video_window_t*)window->state : NULL;

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
   sdl3_video_window_settle(state);
}

unsigned sdl3_video_window_poll(void *owner, video_window_event_t *events,
      unsigned capacity)
{
   unsigned i;
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      sdl3_video_window_t *state = sdl3_video_windows[i];
      if (     state && state->apply && state->common.owner == owner
            && !state->common.closed)
         sdl3_video_window_settle(state);
   }
   return video_windows_drain(owner, sdl3_video_window_at, events,
         capacity);
}

static bool SDLCALL sdl3_video_window_unqueue(void *data, SDL_Event *event)
{
   return    event->type < SDL_EVENT_WINDOW_FIRST
          || event->type > SDL_EVENT_WINDOW_LAST
          || event->window.windowID != *(const SDL_WindowID*)data;
}

void sdl3_video_window_destroy(void *owner, video_window_native_t *window)
{
   SDL_WindowID id;
   sdl3_video_window_t *state = window
      ? (sdl3_video_window_t*)window->state : NULL;

   if (!state || state->common.owner != owner)
      return;
   /* out of the table first: nothing destroying it sends is reported */
   sdl3_video_windows[state->common.request.screen] = NULL;
   sdl3_video_window_count--;
   id = state->id;
   SDL_DestroyWindow(state->win);
   /* nor is what is still queued for it, which the main window's
    * handling would take */
   SDL_FilterEvents(sdl3_video_window_unqueue, &id);
   free(state);
   memset(window, 0, sizeof(*window));
}

void sdl3_video_windows_free(void *owner,
      void (*destroy)(void *owner, video_window_native_t *window))
{
   video_windows_sweep(owner, sdl3_video_window_at, destroy);
   /* The context is going, and its displays with it */
   sdl3_video_monitor_count = 0;
   video_driver_publish_window_displays(NULL, 0);
}
#endif
