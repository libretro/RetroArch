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

#ifdef HAVE_LIBDECOR_H
#include <libdecor.h>
#endif

#include "../../input/common/wayland_common.h"

typedef struct toplevel_listener {
#ifdef HAVE_LIBDECOR_H
   struct libdecor_frame_interface libdecor_frame_interface;
#endif
   struct xdg_toplevel_listener xdg_toplevel_listener;
} toplevel_listener_t;

typedef struct shm_buffer {
   struct wl_buffer *wl_buffer;
   void *data;
   size_t data_size;
} shm_buffer_t;

void xdg_toplevel_handle_configure_common(gfx_ctx_wayland_data_t *wl, void *toplevel,
      int32_t width, int32_t height, struct wl_array *states);

void xdg_toplevel_handle_close(void *data,
      struct xdg_toplevel *xdg_toplevel);

#ifdef HAVE_LIBDECOR_H
void libdecor_frame_handle_configure_common(struct libdecor_frame *frame,
      struct libdecor_configuration *configuration, gfx_ctx_wayland_data_t *wl);

void libdecor_frame_handle_close(struct libdecor_frame *frame,
      void *data);

void libdecor_frame_handle_commit(struct libdecor_frame *frame,
      void *data);
#endif

void gfx_ctx_wl_get_video_size_common(gfx_ctx_wayland_data_t *wl,
      unsigned *width, unsigned *height);

void gfx_ctx_wl_destroy_resources_common(gfx_ctx_wayland_data_t *wl);

void gfx_ctx_wl_update_title_common(gfx_ctx_wayland_data_t *wl);

bool gfx_ctx_wl_get_metrics_common(gfx_ctx_wayland_data_t *wl,
      enum display_metric_types type, float *value);

bool gfx_ctx_wl_init_common(void *video_driver,
      const toplevel_listener_t *toplevel_listener,
      gfx_ctx_wayland_data_t **wl);

bool gfx_ctx_wl_set_video_mode_common_size(gfx_ctx_wayland_data_t *wl,
      unsigned width, unsigned height);

bool gfx_ctx_wl_set_video_mode_common_fullscreen(gfx_ctx_wayland_data_t *wl,
      bool fullscreen);

bool gfx_ctx_wl_suppress_screensaver(void *data, bool state);

bool gfx_ctx_wl_set_video_mode_common(gfx_ctx_wayland_data_t *wl,
      unsigned width, unsigned height,
      bool fullscreen);

float gfx_ctx_wl_get_refresh_rate(void *data);

bool gfx_ctx_wl_has_focus(void *data);

void gfx_ctx_wl_check_window_common(gfx_ctx_wayland_data_t *wl,
      void (*get_video_size)(void*, unsigned*, unsigned*), bool *quit,
      bool *resize, unsigned *width, unsigned *height);

int create_shm_file(off_t size);

shm_buffer_t *create_shm_buffer(gfx_ctx_wayland_data_t *wl,
   int width, int height, uint32_t format);

void shm_buffer_paint_checkerboard(shm_buffer_t *buffer,
      int width, int height, int scale,
      size_t chk, uint32_t bg, uint32_t fg);

bool draw_splash_screen(gfx_ctx_wayland_data_t *wl);

#ifdef HAVE_LIBDECOR_H
extern const struct libdecor_interface libdecor_interface;
#endif

