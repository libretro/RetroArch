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
   struct video_coords *coords;
   void *matrix_data;
   uintptr_t texture;
   enum menu_display_prim_type prim_type;
   float *color;
   const float *vertex;
   const float *tex_coord;
   size_t vertex_count;
   struct
   {
      unsigned id;
      const void *backend_data;
   } pipeline;
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
   void (*draw_pipeline)(void *data);
   void (*viewport)(void *data);
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

void menu_display_blend_begin(void);
void menu_display_blend_end(void);

void menu_display_font_main_deinit(void);
bool menu_display_font_main_init(menu_display_ctx_font_t *font);
void menu_display_font_bind_block(void *block);
bool menu_display_font_flush_block(void);

void menu_display_framebuffer_deinit(void);

void menu_display_deinit(void);
bool menu_display_init(void);

void menu_display_coords_array_reset(void);
video_coord_array_t *menu_display_get_coords_array(void);
void *menu_display_get_font_buffer(void);
void menu_display_set_font_buffer(void *buffer);
const uint8_t *menu_display_get_font_framebuffer(void);
void menu_display_set_font_framebuffer(const uint8_t *buffer);
bool menu_display_libretro_running(void);
bool menu_display_libretro(void);

void menu_display_set_width(unsigned width);
unsigned menu_display_get_width(void);
void menu_display_set_height(unsigned height);
unsigned menu_display_get_height(void);
void menu_display_set_header_height(unsigned height);
unsigned menu_display_get_header_height(void);
unsigned menu_display_get_font_size(void);
void menu_display_set_font_size(unsigned size);
size_t menu_display_get_framebuffer_pitch(void);
void menu_display_set_framebuffer_pitch(size_t pitch);

bool menu_display_get_msg_force(void);
void menu_display_set_msg_force(bool state);
bool menu_display_get_font_data_init(void);
void menu_display_set_font_data_init(bool state);
bool menu_display_get_update_pending(void);
void menu_display_set_viewport(void);
void menu_display_unset_viewport(void);
bool menu_display_get_framebuffer_dirty_flag(void);
void menu_display_set_framebuffer_dirty_flag(void);
void menu_display_unset_framebuffer_dirty_flag(void);
float menu_display_get_dpi(void);
bool menu_display_init_first_driver(void);
bool menu_display_restore_clear_color(void);
void menu_display_clear_color(menu_display_ctx_clearcolor_t *color);
void menu_display_draw(menu_display_ctx_draw_t *draw);

void menu_display_draw_pipeline(menu_display_ctx_draw_t *draw);
void menu_display_draw_bg(menu_display_ctx_draw_t *draw);
void menu_display_draw_gradient(menu_display_ctx_draw_t *draw);
void menu_display_rotate_z(menu_display_ctx_rotate_draw_t *draw);
bool menu_display_get_tex_coords(menu_display_ctx_coord_draw_t *draw);

void menu_display_timedate(menu_display_ctx_datetime_t *datetime);

void menu_display_handle_wallpaper_upload(void *task_data,
      void *user_data, const char *err);

void menu_display_push_quad( 
      unsigned width, unsigned height,
      const float *colors, int x1, int y1,
      int x2, int y2);

void menu_display_snow(int width, int height);

void menu_display_allocate_white_texture(void);

void menu_display_draw_cursor(
      float *color, float cursor_size, uintptr_t texture,
      float x, float y, unsigned width, unsigned height);

void menu_display_draw_text(const char *msg, int width, int height, 
      struct font_params *params);

void menu_display_set_alpha(float *color, float alpha_value);

extern uintptr_t menu_display_white_texture;

extern menu_display_ctx_driver_t menu_display_ctx_gl;
extern menu_display_ctx_driver_t menu_display_ctx_vulkan;
extern menu_display_ctx_driver_t menu_display_ctx_d3d;
extern menu_display_ctx_driver_t menu_display_ctx_null;

#ifdef __cplusplus
}
#endif

#endif
