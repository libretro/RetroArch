/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef __MENU_DISPLAY_H__
#define __MENU_DISPLAY_H__

#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>

#include <gfx/math/matrix_4x4.h>

#ifdef __cplusplus
extern "C" {
#endif

enum menu_display_ctl_state
{
   MENU_DISPLAY_CTL_SET_VIEWPORT = 0,
   MENU_DISPLAY_CTL_UNSET_VIEWPORT,
   MENU_DISPLAY_CTL_GET_FRAMEBUFFER_DIRTY_FLAG,
   MENU_DISPLAY_CTL_SET_FRAMEBUFFER_DIRTY_FLAG,
   MENU_DISPLAY_CTL_UNSET_FRAMEBUFFER_DIRTY_FLAG,
   MENU_DISPLAY_CTL_GET_DPI,
   MENU_DISPLAY_CTL_UPDATE_PENDING,
   MENU_DISPLAY_CTL_WIDTH,
   MENU_DISPLAY_CTL_HEIGHT,
   MENU_DISPLAY_CTL_HEADER_HEIGHT,
   MENU_DISPLAY_CTL_SET_HEADER_HEIGHT,
   MENU_DISPLAY_CTL_SET_WIDTH,
   MENU_DISPLAY_CTL_SET_HEIGHT,
   MENU_DISPLAY_CTL_FB_DATA,
   MENU_DISPLAY_CTL_SET_FB_DATA,
   MENU_DISPLAY_CTL_FB_PITCH,
   MENU_DISPLAY_CTL_SET_FB_PITCH,
   MENU_DISPLAY_CTL_LIBRETRO,
   MENU_DISPLAY_CTL_LIBRETRO_RUNNING,
   MENU_DISPLAY_CTL_FONT_DATA_INIT,
   MENU_DISPLAY_CTL_SET_FONT_DATA_INIT,
   MENU_DISPLAY_CTL_FONT_SIZE,
   MENU_DISPLAY_CTL_SET_FONT_SIZE,
   MENU_DISPLAY_CTL_MSG_FORCE,
   MENU_DISPLAY_CTL_SET_MSG_FORCE,
   MENU_DISPLAY_CTL_FONT_BUF,
   MENU_DISPLAY_CTL_SET_FONT_BUF,
   MENU_DISPLAY_CTL_FONT_FB,
   MENU_DISPLAY_CTL_SET_FONT_FB
};

enum menu_display_prim_type
{
   MENU_DISPLAY_PRIM_NONE = 0,
   MENU_DISPLAY_PRIM_TRIANGLESTRIP,
   MENU_DISPLAY_PRIM_TRIANGLES
};

void menu_display_free(void);

bool menu_display_init(void);

bool menu_display_font_init_first(const void **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float font_size);

bool menu_display_font_bind_block(void *data, const void *font_data, void *userdata);

bool menu_display_font_flush_block(void *data, const void *font_data);

bool menu_display_init_main_font(void *data,
      const char *font_path, float font_size);

void menu_display_free_main_font(void);

bool menu_display_ctl(enum menu_display_ctl_state state, void *data);

void menu_display_timedate(char *s, size_t len, unsigned time_mode);

void menu_display_msg_queue_push(const char *msg, unsigned prio, unsigned duration,
      bool flush);

#ifdef HAVE_OPENGL
#include "../gfx/drivers/gl_common.h"

void menu_display_draw_frame(
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      const void *shader,
      struct gfx_coords *coords,
      math_matrix_4x4 *mat, 
      GLuint texture,
      size_t vertex_count,
      enum menu_display_prim_type prim_type
      );

void menu_display_blend_begin(void *data);

void menu_display_blend_end(void *data);

void menu_display_frame_background(
      menu_handle_t *menu,
      settings_t *settings,
      gl_t *gl,
      unsigned width, unsigned height,
      GLuint texture,
      float handle_alpha,
      bool force_transparency,
      GRfloat *color,
      GRfloat *color2,
      const GRfloat *tex_coord,
      size_t vertex_count,
      enum menu_display_prim_type prim_type
      );

void menu_display_restore_clear_color(void *data);

void menu_display_clear_color(void *data, float r, float g, float b, float a);
#endif

const char *menu_video_get_ident(void);

#ifdef __cplusplus
}
#endif

#endif
