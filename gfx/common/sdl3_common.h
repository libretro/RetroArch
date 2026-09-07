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

#ifndef SDL3_COMMON_H__
#define SDL3_COMMON_H__

#include <stdint.h>
#include <boolean.h>

#include <SDL3/SDL.h>

#include "../video_defines.h"
#include "../../retroarch.h"

enum sdl3_flags
{
   SDL3_FLAG_QUITTING       = (1 << 0),
   SDL3_FLAG_SHOULD_RESIZE  = (1 << 1),
   SDL3_FLAG_ADAPTIVE_VSYNC = (1 << 2)
};

typedef struct sdl3_tex
{
   SDL_Texture *tex;

   unsigned w;
   unsigned h;
   bool active;
   bool rgb32;
} sdl3_tex_t;

typedef struct _sdl3_video
{
   SDL_Window *window; /* Must be first because it's shared across
                        * sdl3_ctx_* callbacks. */
   double rotation;

   struct video_viewport vp;
   video_info_t video;

   sdl3_tex_t frame; /* ptr alignment */
   sdl3_tex_t menu;  /* ptr alignment */

   SDL_Renderer *renderer;

   uint8_t flags;
} sdl3_video_t;

/* Registers the application name, version and metadata. */
void sdl3_set_app_metadata(void);

/* Sets the window's native display/window handles (X11, Wayland,
 * Win32, Cocoa, KMS) to the video driver state via SDL's window
 * properties API, so subsystems that require the raw handles
 * can get access to them. */
void sdl3_set_handles(SDL_Window *window);

/* Processes the SDL event queue, and cleans up the ones that
 * are handled elsewhere. For example: quit / window / display
 * events, etc. The keyboard/mouse events are left in the queue
 * for the SDL input driver to handle itself. */
void sdl3_pump_window_events(bool *quit, bool *resize);

/* Creates or resizes the window, or toggles fullscreen. */
bool sdl3_window_set_video_mode(SDL_Window **win,
      unsigned width, unsigned height, bool fullscreen,
      SDL_WindowFlags backend_flags);

/* Retrieves the window size in pixels, or the desktop mode when the
 * window doesn't exist yet. */
void sdl3_window_get_video_size(SDL_Window *win,
      unsigned *width, unsigned *height);

/* Get the refresh rate of the display the window is on, in Hz.
 * Returns 0.0f when there is no window or when we can't tell. */
float sdl3_window_get_refresh_rate(SDL_Window *win);

/* Updates the window title to what is expected. */
void sdl3_window_update_title(SDL_Window *win);

/* True when the window has keyboard input focus. */
bool sdl3_window_has_focus(SDL_Window *win);

/* Toggles screensaver inhibition. */
bool sdl3_suppress_screensaver(void *data, bool enable);

/* Initializes the input driver paired with an SDL3 window. */
void sdl3_input_driver(const char *joypad_name,
      input_driver_t **input, void **input_data);

/* gfx_ctx_driver_t input_driver callback wrapping sdl3_input_driver. */
void sdl3_ctx_input_driver(void *data, const char *name,
      input_driver_t **input, void **input_data);

/* Determines whether or not an SDL3 context driver should be
 * initialized. Will return true when the user is using the
 * sdl3 video driver, or when ctx_ident is explicitly configured
 * as the context driver. */
bool sdl3_ctx_enabled(const char *ctx_ident);

void sdl3_ctx_get_video_size(void *data, unsigned *width, unsigned *height);
float sdl3_ctx_get_refresh_rate(void *data);
void sdl3_ctx_update_title(void *data);
bool sdl3_ctx_has_focus(void *data);
void sdl3_ctx_check_window(void *data, bool *quit, bool *resize,
      unsigned *width, unsigned *height);

/* Retrieve the DISPLAY_METRIC_DPI for the window's display scale.
 * This usually ends up being scale * 96 DPI, or false otherwise. */
bool sdl3_ctx_get_metrics(void *data, enum display_metric_types type,
      float *value);

/* Shows or hides the mouse cursor. */
void sdl3_show_mouse(void *data, bool state);

#endif
