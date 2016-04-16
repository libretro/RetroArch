/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../gfx/video_context_driver.h"
#include "../gfx/video_coord_array.h"

#ifdef __cplusplus
extern "C" {
#endif

enum menu_display_ctl_state
{
   MENU_DISPLAY_CTL_NONE = 0,
   MENU_DISPLAY_CTL_SET_VIEWPORT,
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
   MENU_DISPLAY_CTL_FB_PITCH,
   MENU_DISPLAY_CTL_SET_FB_PITCH,
   MENU_DISPLAY_CTL_LIBRETRO,
   MENU_DISPLAY_CTL_LIBRETRO_RUNNING,
   MENU_DISPLAY_CTL_SET_STUB_DRAW_FRAME,
   MENU_DISPLAY_CTL_UNSET_STUB_DRAW_FRAME,
   MENU_DISPLAY_CTL_FRAMEBUF_DEINIT,
   MENU_DISPLAY_CTL_DEINIT,
   MENU_DISPLAY_CTL_INIT,
   MENU_DISPLAY_CTL_INIT_FIRST_DRIVER,
   MENU_DISPLAY_CTL_FONT_DATA_INIT,
   MENU_DISPLAY_CTL_SET_FONT_DATA_INIT,
   MENU_DISPLAY_CTL_FONT_SIZE,
   MENU_DISPLAY_CTL_SET_FONT_SIZE,
   MENU_DISPLAY_CTL_MSG_FORCE,
   MENU_DISPLAY_CTL_SET_MSG_FORCE,
   MENU_DISPLAY_CTL_FONT_BUF,
   MENU_DISPLAY_CTL_FONT_FLUSH_BLOCK,
   MENU_DISPLAY_CTL_SET_FONT_BUF,
   MENU_DISPLAY_CTL_FONT_FB,
   MENU_DISPLAY_CTL_SET_FONT_FB,
   MENU_DISPLAY_CTL_FONT_MAIN_DEINIT,
   MENU_DISPLAY_CTL_FONT_MAIN_INIT,
   MENU_DISPLAY_CTL_FONT_BIND_BLOCK,
   MENU_DISPLAY_CTL_BLEND_BEGIN,
   MENU_DISPLAY_CTL_BLEND_END,
   MENU_DISPLAY_CTL_RESTORE_CLEAR_COLOR,
   MENU_DISPLAY_CTL_CLEAR_COLOR,
   MENU_DISPLAY_CTL_DRAW,
   MENU_DISPLAY_CTL_DRAW_BG,
   MENU_DISPLAY_CTL_DRAW_GRADIENT,
   MENU_DISPLAY_CTL_ROTATE_Z,
   MENU_DISPLAY_CTL_TEX_COORDS_GET,
   MENU_DISPLAY_CTL_TIMEDATE
};

enum menu_display_prim_type
{
   MENU_DISPLAY_PRIM_NONE = 0,
   MENU_DISPLAY_PRIM_TRIANGLESTRIP,
   MENU_DISPLAY_PRIM_TRIANGLES
};

enum menu_display_driver_type
{
   MENU_VIDEO_DRIVER_GENERIC = 0,
   MENU_VIDEO_DRIVER_OPENGL,
   MENU_VIDEO_DRIVER_VULKAN,
   MENU_VIDEO_DRIVER_DIRECT3D
};

typedef struct menu_display_ctx_clearcolor
{
   float r;
   float g;
   float b;
   float a;
} menu_display_ctx_clearcolor_t;

typedef struct menu_display_ctx_draw
{
   float x;
   float y;
   unsigned width;
   unsigned height;
   struct gfx_coords *coords;
   void *matrix_data;
   uintptr_t texture;
   enum menu_display_prim_type prim_type;
   float handle_alpha;
   bool force_transparency;
   float *color;
   const float *vertex;
   const float *tex_coord;
   size_t vertex_count;
} menu_display_ctx_draw_t;

typedef struct menu_display_ctx_rotate_draw
{
   math_matrix_4x4 *matrix;
   float rotation;
   float scale_x;
   float scale_y;
   float scale_z;
   bool scale_enable;
} menu_display_ctx_rotate_draw_t;

typedef struct menu_display_ctx_coord_draw
{
   const float *ptr;
} menu_display_ctx_coord_draw_t;

typedef struct menu_display_ctx_datetime
{
   char *s;
   size_t len;
   unsigned time_mode;
} menu_display_ctx_datetime_t;

typedef struct menu_display_ctx_driver
{
   void (*draw)(void *data);
   void (*blend_begin)(void);
   void (*blend_end)(void);
   void (*restore_clear_color)(void);
   void (*clear_color)(menu_display_ctx_clearcolor_t *clearcolor);
   void *(*get_default_mvp)(void);
   const float *(*get_default_vertices)(void);
   const float *(*get_default_tex_coords)(void);
   bool (*font_init_first)(
         void **font_handle, void *video_data,
         const char *font_path, float font_size);
   enum menu_display_driver_type type;
   const char *ident;
} menu_display_ctx_driver_t;

typedef struct menu_display_ctx_font
{
   const char *path;
   float size;
} menu_display_ctx_font_t;

typedef uintptr_t menu_texture_item;

bool menu_display_ctl(enum menu_display_ctl_state state, void *data);

void menu_display_handle_wallpaper_upload(void *task_data,
      void *user_data, const char *err);

uintptr_t menu_display_white_texture;
void menu_display_allocate_white_texture();

extern menu_display_ctx_driver_t menu_display_ctx_gl;
extern menu_display_ctx_driver_t menu_display_ctx_vulkan;
extern menu_display_ctx_driver_t menu_display_ctx_d3d;
extern menu_display_ctx_driver_t menu_display_ctx_null;

#ifdef __cplusplus
}
#endif

#endif
