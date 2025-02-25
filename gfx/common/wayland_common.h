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

#define WAYLAND_APP_ID "com.libretro.RetroArch"

void gfx_ctx_wl_get_video_size_common(void *data, unsigned *width,
      unsigned *height);

void gfx_ctx_wl_destroy_resources_common(gfx_ctx_wayland_data_t *wl);

void gfx_ctx_wl_update_title_common(void *data);

bool gfx_ctx_wl_get_metrics_common(void *data,
      enum display_metric_types type, float *value);

bool gfx_ctx_wl_init_common(
      const driver_configure_handler_t driver_configure_handler,
      gfx_ctx_wayland_data_t **wl);

bool gfx_ctx_wl_set_video_mode_common_size(gfx_ctx_wayland_data_t *wl,
      unsigned width, unsigned height, bool fullscreen);

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

