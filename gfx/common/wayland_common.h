/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#pragma once

#include "../../input/common/wayland_common.h"
#include "../video_windows.h"

#define WAYLAND_APP_ID "com.libretro.RetroArch"

#define WL_VIDEO_WINDOW_OUTPUTS 4

typedef struct shm_buffer
{
   struct wl_buffer *wl_buffer;
   void *data;
   size_t data_size;
} shm_buffer_t;

void gfx_ctx_wl_get_video_size_common(void *data, unsigned *dims);

void gfx_ctx_wl_destroy_resources_common(gfx_ctx_wayland_data_t *wl);
#ifdef WEBOS
void gfx_ctx_wl_destroy_resources_webos(gfx_ctx_wayland_data_t *wl);
#endif

/* Frees a context, or on a video reinit (@may_keep) keeps its window
 * for the next init_common to adopt. */
void gfx_ctx_wl_free_common(gfx_ctx_wayland_data_t *wl, bool may_keep);

/* Destroys a kept window no context adopted. */
void gfx_ctx_wl_release_kept(void);

/* The kept window, adopted by the context being initialised; NULL
 * when none is kept. */
gfx_ctx_wayland_data_t *gfx_ctx_wl_take_kept(
      driver_configure_handler_t driver_configure_handler);

void gfx_ctx_wl_update_title_common(void *data);


bool gfx_ctx_wl_init_common(
      driver_configure_handler_t driver_configure_handler,
      gfx_ctx_wayland_data_t **wl);

bool gfx_ctx_wl_set_video_mode_common_size(gfx_ctx_wayland_data_t *wl,
      unsigned width, unsigned height, bool fullscreen);

bool gfx_ctx_wl_set_video_mode_common_fullscreen(gfx_ctx_wayland_data_t *wl,
      bool fullscreen);

bool gfx_ctx_wl_request_fullscreen(gfx_ctx_wayland_data_t *wl,
      bool fullscreen);

bool gfx_ctx_wl_suppress_screensaver(void *data, bool state);

bool gfx_ctx_wl_set_video_mode_common(gfx_ctx_wayland_data_t *wl,
      unsigned width, unsigned height,
      bool fullscreen);


bool gfx_ctx_wl_has_focus(void *data);

void gfx_ctx_wl_check_window_common(gfx_ctx_wayland_data_t *wl,
      void (*get_video_size)(void*, unsigned*), bool *quit,
      bool *resize, unsigned *dims);

#ifdef HAVE_VIDEO_WINDOWS
/* A secondary screen's toplevel on the main window's connection,
 * window->state from create to destroy. Its events are handled in the
 * context's check_window, so all of it belongs to the thread that runs
 * the context (the RETROARCH_WAYLAND_INPUT_QUEUE=0 opt-out runs them on
 * the frontend's too, as it does the main window's). */
typedef struct wl_video_window
{
   video_window_common_t common;
   /* The context it was made on, which a kept window stays on across
    * a reinit: Wayland's is kept with it */
   void *ctx;
   struct wl_surface *surface;
   struct xdg_surface *xdg_surface;
   struct xdg_toplevel *xdg_toplevel;
   struct zxdg_toplevel_decoration_v1 *deco;
#ifdef HAVE_LIBDECOR_H
   struct libdecor_frame *libdecor_frame;
#endif
   struct wp_viewport *viewport;
   struct wp_fractional_scale_v1 *fractional_scale;
   /* The outputs the surface is on */
   struct wl_output *outputs[WL_VIDEO_WINDOW_OUTPUTS];
   /* The context's: what it draws with, and the native window (a
    * wl_egl_window) it is made on, at win_dims */
   void *drawable;
   void *win;
   /* The surface's size, and the size to go back to when the window
    * floats again, each packed; window->dims is the buffer's */
   unsigned dims;
   unsigned floating_dims;
   unsigned win_dims;
   unsigned scale;
   unsigned preferred_buffer_scale; /* wl_surface v6; 0 until sent */
   unsigned fractional_scale_num;
   /* xdg_toplevel.configure, until xdg_surface.configure makes it
    * current */
   unsigned cfg_dims;
   bool cfg_fullscreen;
   bool cfg_suspended;
   bool cfg_floating;
   bool fullscreen;
   bool floating;
   /* floating_dims was asked for while the window did not float */
   bool floating_asked;
   bool suspended;
   /* Shown by the compositor: its first wl_surface.enter */
   bool mapped;
   /* The context has set its drawable's swap interval */
   bool interval_set;
} wl_video_window_t;

/* The window comes unconfigured: the compositor's first configure is
 * handled in a later check_window, which makes window->configured
 * true. Nothing may be attached to the surface before that. */
bool wl_video_window_create(void *owner,
      const video_window_request_t *request,
      video_window_native_t *window);
void wl_video_window_update(void *owner, video_window_native_t *window,
      const video_window_request_t *request);
unsigned wl_video_window_poll(void *owner, video_window_event_t *events,
      unsigned capacity);
void wl_video_window_destroy(void *owner, video_window_native_t *window);
void wl_video_window_keep(void *owner, video_window_native_t *window);
bool wl_video_window_take(void *owner, unsigned screen,
      video_window_native_t *window, unsigned *display);
/* Kept windows on context 'ctx', NULL for any, go: before its
 * connection does, or once the next driver is up */
void wl_video_windows_release_kept(void *ctx);
/* Hands each window 'owner' still has to 'destroy'. */
void wl_video_windows_free(void *owner,
      void (*destroy)(void *owner, video_window_native_t *window));
#endif
