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

   settings->uints.window_position_x = (unsigned)x;
   settings->uints.window_position_y = (unsigned)y;
   settings->uints.window_position_width = (unsigned)w;
   settings->uints.window_position_height = (unsigned)h;
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
      if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
         *resize = true;

      if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_MOVED)
         sdl3_window_save_position(SDL_GetWindowFromID(event.window.windowID));
   }

   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_DISPLAY_FIRST, SDL_EVENT_DISPLAY_LAST) > 0)
   {
      if (event.type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED)
         *resize = true;
   }

    /* Clear out the input queue if we're not using the
     * SDL driver. */
   if (input_state_get_ptr()->current_driver != &input_sdl3)
   {
      SDL_FlushEvents(SDL_EVENT_KEY_DOWN,         SDL_EVENT_MOUSE_REMOVED);
      SDL_FlushEvents(SDL_EVENT_FINGER_DOWN,      SDL_EVENT_FINGER_CANCELED);
      SDL_FlushEvents(SDL_EVENT_PEN_PROXIMITY_IN, SDL_EVENT_PEN_AXIS);
   }
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

/* Moves the window to the provided 1-based monitor. If 0
 * is provided, leave as is. */
static void sdl3_window_move_to_monitor(SDL_Window *win,
      unsigned monitor_index)
{
   int count;
   SDL_DisplayID *displays;
   SDL_DisplayID display = 0;

   if (!win || monitor_index == 0)
      return;

   displays = SDL_GetDisplays(&count);
   if (displays && (int)monitor_index <= count)
      display = displays[monitor_index - 1];
   SDL_free(displays);

   /* Out-of-range index (e.g. the configured monitor was unplugged):
    * leave the window where the system placed it. */
   if (display == 0)
      return;

   SDL_SetWindowPosition(win,
         SDL_WINDOWPOS_CENTERED_DISPLAY(display),
         SDL_WINDOWPOS_CENTERED_DISPLAY(display));
   SDL_SyncWindow(win);
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
      flags |= SDL_WINDOW_FULLSCREEN;

   if (!settings->bools.video_window_show_decorations)
      flags |= SDL_WINDOW_BORDERLESS;

   if (!(win = SDL_CreateWindow("RetroArch", width, height, flags)))
      return NULL;

   sdl3_window_set_icon(win);

   /* Set either the window position, or the active monitor. This is ignored
    * in Wayland where windows are not manually positioned. */
   if (settings->bools.video_window_save_positions
         && !fullscreen
         && settings->uints.window_position_width
         && settings->uints.window_position_height
         && !string_is_equal(SDL_GetCurrentVideoDriver(), "wayland"))
   {
      SDL_SetWindowPosition(win,
            (int)settings->uints.window_position_x,
            (int)settings->uints.window_position_y);
      SDL_SyncWindow(win);
   }
   else
      sdl3_window_move_to_monitor(win, settings->uints.video_monitor_index);

   /* SDL_EVENT_TEXT_INPUT is emitted for windows that opted in for
    * it. The SDL3 input driver handles those events for menu
    * text entry and core keyboard callbacks. */
   SDL_StartTextInput(win);

   return win;
}

/* Applies the fullscreen state. */
static void sdl3_window_apply_fullscreen(SDL_Window *win,
      unsigned width, unsigned height, bool fullscreen)
{
   SDL_DisplayMode mode;
   settings_t *settings = config_get_ptr();
   bool exclusive = fullscreen
         && !settings->bools.video_windowed_fullscreen
         && SDL_GetClosestFullscreenDisplayMode(
               SDL_GetDisplayForWindow(win),
               width, height,
               settings->floats.video_refresh_rate, false, &mode);

   SDL_SetWindowFullscreenMode(win, exclusive ? &mode : NULL);
   SDL_SetWindowFullscreen(win, fullscreen);

   if (fullscreen)
      SDL_HideCursor();
   else
   {
      /* Leaving fullscreen is asynchronous, and size requests
       * are dropped while still fullscreen, so we wait until
       * it has completed. */
      SDL_SyncWindow(win);
      SDL_SetWindowSize(win, width, height);
      SDL_ShowCursor();
   }
}

bool sdl3_window_set_video_mode(SDL_Window **win,
      unsigned width, unsigned height, bool fullscreen,
      SDL_WindowFlags backend_flags)
{
   if (*win)
      SDL_SetWindowBordered(*win, config_get_ptr()->bools.video_window_show_decorations);
   else if (!(*win = sdl3_window_create(width, height, fullscreen, backend_flags)))
      return false;

   sdl3_window_apply_fullscreen(*win, width, height, fullscreen);

   sdl3_set_handles(*win);

   return true;
}

void sdl3_window_get_video_size(SDL_Window *win,
      unsigned *width, unsigned *height)
{
   const SDL_DisplayMode *mode;

   if (win)
   {
      int w, h;
      SDL_GetWindowSizeInPixels(win, &w, &h);
      *width = w;
      *height = h;
      return;
   }

   /* The window doesn't exist yet, so report the desktop size. */
   mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
   if (mode)
   {
      *width = mode->w;
      *height = mode->h;
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

/* Returns the configured input driver if it works alongside an SDL3
 * window, or NULL if the SDL3 input driver should be used instead.
 *
 * The udev, linuxraw, raw and dinput input drivers read input devices
 * directly, so they will function inside an SDL3 window. Drivers tied
 * to a windowing system like x, wayland, or cocoa however, should end
 * up using the SDL3 input driver. */
static input_driver_t *sdl3_passthrough_input(const char *ident)
{
#ifdef HAVE_UDEV
   if (string_is_equal(ident, "udev"))
      return &input_udev;
#endif
#if defined(__linux__) && !defined(ANDROID)
   if (string_is_equal(ident, "linuxraw"))
      return &input_linuxraw;
#endif
#if defined(_WIN32) && !defined(_XBOX) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(ident, "raw"))
      return &input_winraw;
#endif
#endif
#ifdef HAVE_DINPUT
   if (string_is_equal(ident, "dinput"))
      return &input_dinput;
#endif
   return NULL;
}

void sdl3_input_driver(const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   input_driver_t *passthrough = sdl3_passthrough_input(config_get_ptr()->arrays.input_driver);

   if (passthrough)
   {
      if ((*input_data = input_driver_init_wrap(passthrough, joypad_name)))
      {
         *input = passthrough;
         return;
      }
      /* The configured input driver failed, so fallback to SDL3. */
   }

   /* Use the SDL3 input driver, taking advantage of the window's event queue. */
   *input_data = input_driver_init_wrap(&input_sdl3, joypad_name);
   *input = *input_data ? &input_sdl3 : NULL;
}

void sdl3_ctx_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   sdl3_input_driver(name, input, input_data);
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

void sdl3_ctx_get_video_size(void *data, unsigned *width, unsigned *height)
{
   if (data)
      sdl3_window_get_video_size(sdl3_ctx_window(data), width, height);
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
      unsigned *width, unsigned *height)
{
   SDL_Window *win = sdl3_ctx_window(data);

   sdl3_pump_window_events(quit, resize);

   if (*resize && win)
      sdl3_window_get_video_size(win, width, height);
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
