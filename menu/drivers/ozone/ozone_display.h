/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2018      - Alfredo Monclús
 *  Copyright (C) 2018      - natinusala
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

#include "ozone.h"

#include "../../../gfx/gfx_display.h"

#include "../../menu_driver.h"

void ozone_draw_cursor(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int x_offset,
      unsigned width, unsigned height,
      size_t y, float alpha);

void ozone_draw_icon(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      unsigned icon_width,
      unsigned icon_height,
      uintptr_t texture,
      float x, float y,
      unsigned width, unsigned height,
      float rotation, float scale_factor,
      float *color);

void ozone_restart_cursor_animation(ozone_handle_t *ozone);

void ozone_draw_backdrop(
      void *userdata,
      void *disp_data,
      unsigned video_width,
      unsigned video_height,
      float alpha);

void ozone_draw_osk(
      ozone_handle_t *ozone,
      void *userdata,
      void *disp_userdata,
      unsigned video_width,
      unsigned video_height,
      const char *label, const char *str);

void ozone_draw_messagebox(
      ozone_handle_t *ozone,
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      const char *message);

void ozone_draw_fullscreen_thumbnails(
      ozone_handle_t *ozone,
      void *userdata,
      void *disp_userdata,
      unsigned video_width,
      unsigned video_height
      );
