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

#include <retro_atomic.h>
#include "../video_defines.h"
#include "../video_views.h"
#include "../../retroarch.h"

enum sdl3_flags
{
   SDL3_FLAG_QUITTING         = (1 << 0),
   SDL3_FLAG_SHOULD_RESIZE    = (1 << 1),
   SDL3_FLAG_ADAPTIVE_VSYNC   = (1 << 2),
   /* The overlay textures are the overlay pack's (load_textures),
    * not this driver's to destroy. */
   SDL3_FLAG_OVERLAY_BORROWED = (1 << 3),
   /* The renderer takes these 16-bit formats as they are */
   SDL3_FLAG_NATIVE_RGB565    = (1 << 4),
   SDL3_FLAG_NATIVE_RGBA4444  = (1 << 5)
};

typedef struct sdl3_tex
{
   SDL_Texture *tex;

   unsigned w;
   unsigned h;
   /* The 16-bit source a 32-bit texture is widened from on upload,
    * SDL_PIXELFORMAT_UNKNOWN when uploads are copied as they are */
   SDL_PixelFormat widen;
   bool active;
   bool rgb32;
   bool failed; /* creating it at w x h failed */
} sdl3_tex_t;

#ifdef HAVE_OVERLAY
/* On-screen overlay for SDL3. */
struct sdl3_overlay
{
   SDL_Texture *tex;
   SDL_FRect tex_coords; /* Normalized 0..1 for the source. */
   SDL_FRect vert_coords; /* Normalized 0..1 within the base area. */
   float alpha_mod;
   bool fullscreen;
};
#endif

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

   /* The largest swap interval the renderer took at init */
   unsigned swap_interval_cap;

   /* Resolution steps asked for on the main thread, taken on the
    * video thread */
   retro_atomic_int_t mode_step;

   /* What SDL_RenderGeometry is handed, grown to the largest batch:
    * the display draws, and the interlaced placement's rows */
   SDL_Vertex *display_verts;
   int *display_indices;
   size_t display_verts_cap;
   size_t display_indices_cap;

#ifdef HAVE_OVERLAY
   struct sdl3_overlay *overlays;
   unsigned overlays_size;
   bool overlays_enabled;
#endif

   uint8_t flags;

   struct
   {
      video_views_chains_t  chains;
      video_views_target_t  canvas_target;
      video_views_target_t  ui_target;
      video_views_ui_dims_t saved;
      /* What the last frame drew, kept for the viewport readback to
       * draw again: its map, layout, chains and the window's size */
      video_views_map_t     map;
      video_views_layout_t  layout;
      void                 *drawn[RETRO_VIDEO_VIEWS_MAX];
      unsigned              dims;
      /* Each view of the frame, written straight into its own texture */
      SDL_Texture          *tex[RETRO_VIDEO_VIEWS_MAX];
      unsigned              tex_dims[RETRO_VIDEO_VIEWS_MAX];
      SDL_Texture          *canvas;
      SDL_Texture          *ui;
      SDL_BlendMode         premultiplied;
      unsigned              frame_dims;
      /* The game viewport while the UI layer is drawn */
      struct video_viewport vp;
      /* For get_flags on the main thread: whether views may be drawn,
       * and whether the last frame with a map drew it whole */
      retro_atomic_int_t    allowed;
      retro_atomic_int_t    fallback;
      bool                  tried;
      /* A view could not be written at frame_dims: draw whole */
      bool                  write_failed;
      /* The premultiplied blend the UI layer is placed with */
      bool                  layer;
      bool                  active;
      bool                  blend;
      bool                  direct_seen;
   } views;
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

/* Pumps the SDL event queue for the input drivers, unless the video
 * thread owns the window and pumps it already. A pump also updates
 * joysticks and sensors; false when it was left to the video thread,
 * and the caller updates them itself. */
bool sdl3_pump_input_events(void);

/* Creates or resizes the window, or toggles fullscreen. */
bool sdl3_window_set_video_mode(SDL_Window **win,
      unsigned dims, bool fullscreen,
      SDL_WindowFlags backend_flags);

/* Retrieves the window size in pixels, or the desktop mode when the
 * window doesn't exist yet. */
void sdl3_window_get_video_size(SDL_Window *win,
      unsigned *dims);

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
void sdl3_input_driver(const char *joypad_name);

/* gfx_ctx_driver_t input_driver callback wrapping sdl3_input_driver. */
void sdl3_ctx_input_driver(void *data, const char *name);

/* Determines whether or not an SDL3 context driver should be
 * initialized. Will return true when the user is using the
 * sdl3 video driver, or when ctx_ident is explicitly configured
 * as the context driver. */
bool sdl3_ctx_enabled(const char *ctx_ident);

/* Whether the settings make the next video driver's window an SDL3
 * one, as sdl3_ctx_enabled() decides it. */
bool sdl3_next_window_is_sdl3(void);

void sdl3_ctx_get_video_size(void *data, unsigned *dims);
float sdl3_ctx_get_refresh_rate(void *data);
void sdl3_ctx_update_title(void *data);
bool sdl3_ctx_has_focus(void *data);
void sdl3_ctx_check_window(void *data, bool *quit, bool *resize,
      unsigned *dims);

/* Retrieve the DISPLAY_METRIC_DPI for the window's display scale.
 * This usually ends up being scale * 96 DPI, or false otherwise. */
bool sdl3_ctx_get_metrics(void *data, enum display_metric_types type,
      float *value);

/* Shows or hides the mouse cursor. */
void sdl3_show_mouse(void *data, bool state);

/* Returns the active SDL_Window, whether it belongs to the sdl3
 * video driver or one of the gl/vk sdl3 context drivers. NULL when
 * SDL3 isn't driving the window. Under threaded video it is made on
 * the video thread; only sdl3_pump_input_events pumps its queue. */
SDL_Window *sdl3_get_window(void);

/* True when SDL_StartTextInput() on this device pops up a system
 * screen keyboard, rather than just enabling text events in the
 * background. False whenever a physical keyboard is attached. */
bool sdl3_uses_screen_keyboard(void);

#endif
