/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __GFX_DISPLAY_H__
#define __GFX_DISPLAY_H__

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <string/stdstring.h>
#include <gfx/math/matrix_4x4.h>

#include "../retroarch.h"
#include "../file_path_special.h"
#include "../gfx/font_driver.h"

RETRO_BEGIN_DECLS

#define COLOR_TEXT_ALPHA(color, alpha) (color & 0xFFFFFF00) | alpha

#define HEX_R(hex) ((hex >> 16) & 0xFF) * (1.0f / 255.0f)
#define HEX_G(hex) ((hex >> 8 ) & 0xFF) * (1.0f / 255.0f)
#define HEX_B(hex) ((hex >> 0 ) & 0xFF) * (1.0f / 255.0f)

#define COLOR_HEX_TO_FLOAT(hex, alpha) { \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha, \
   HEX_R(hex), HEX_G(hex), HEX_B(hex), alpha  \
}

/* TODO/FIXME - global, not thread-safe */
extern float osk_dark[16];

enum menu_driver_id_type
{
   MENU_DRIVER_ID_UNKNOWN = 0,
   MENU_DRIVER_ID_RGUI,
   MENU_DRIVER_ID_OZONE,
   MENU_DRIVER_ID_GLUI,
   MENU_DRIVER_ID_XMB,
   MENU_DRIVER_ID_XUI,
   MENU_DRIVER_ID_STRIPES
};


enum gfx_display_prim_type
{
   GFX_DISPLAY_PRIM_NONE = 0,
   GFX_DISPLAY_PRIM_TRIANGLESTRIP,
   GFX_DISPLAY_PRIM_TRIANGLES
};

enum gfx_display_driver_type
{
   GFX_VIDEO_DRIVER_GENERIC = 0,
   GFX_VIDEO_DRIVER_OPENGL,
   GFX_VIDEO_DRIVER_OPENGL1,
   GFX_VIDEO_DRIVER_OPENGL_CORE,
   GFX_VIDEO_DRIVER_VULKAN,
   GFX_VIDEO_DRIVER_METAL,
   GFX_VIDEO_DRIVER_DIRECT3D8,
   GFX_VIDEO_DRIVER_DIRECT3D9,
   GFX_VIDEO_DRIVER_DIRECT3D10,
   GFX_VIDEO_DRIVER_DIRECT3D11,
   GFX_VIDEO_DRIVER_DIRECT3D12,
   GFX_VIDEO_DRIVER_VITA2D,
   GFX_VIDEO_DRIVER_CTR,
   GFX_VIDEO_DRIVER_WIIU,
   GFX_VIDEO_DRIVER_GDI,
   GFX_VIDEO_DRIVER_SWITCH
};

typedef struct gfx_display_ctx_clearcolor
{
   float r;
   float g;
   float b;
   float a;
} gfx_display_ctx_clearcolor_t;

typedef struct gfx_display_frame_info
{
   bool shadows_enable;
} gfx_display_frame_info_t;

typedef struct gfx_display_ctx_draw gfx_display_ctx_draw_t;


typedef struct gfx_display_ctx_driver
{
   /* Draw graphics to the screen. */
   void (*draw)(gfx_display_ctx_draw_t *draw, video_frame_info_t *video_info);
   /* Draw one of the menu pipeline shaders. */
   void (*draw_pipeline)(gfx_display_ctx_draw_t *draw,
         video_frame_info_t *video_info);
   void (*viewport)(gfx_display_ctx_draw_t *draw,
         video_frame_info_t *video_info);
   /* Start blending operation. */
   void (*blend_begin)(video_frame_info_t *video_info);
   /* Finish blending operation. */
   void (*blend_end)(video_frame_info_t *video_info);
   /* Set the clear color back to its default values. */
   void (*restore_clear_color)(void);
   /* Set the color to be used when clearing the screen */
   void (*clear_color)(gfx_display_ctx_clearcolor_t *clearcolor,
         video_frame_info_t *video_info);
   /* Get the default Model-View-Projection matrix */
   void *(*get_default_mvp)(video_frame_info_t *video_info);
   /* Get the default vertices matrix */
   const float *(*get_default_vertices)(void);
   /* Get the default texture coordinates matrix */
   const float *(*get_default_tex_coords)(void);
   /* Initialize the first compatible font driver for this menu driver. */
   bool (*font_init_first)(
         void **font_handle, void *video_data,
         const char *font_path, float font_size,
         bool is_threaded);
   enum gfx_display_driver_type type;
   const char *ident;
   bool handles_transform;
   /* Enables and disables scissoring */
   void (*scissor_begin)(video_frame_info_t *video_info, int x, int y, unsigned width, unsigned height);
   void (*scissor_end)(video_frame_info_t *video_info);
} gfx_display_ctx_driver_t;

struct gfx_display_ctx_draw
{
   float x;
   float y;
   float *color;
   const float *vertex;
   const float *tex_coord;
   unsigned width;
   unsigned height;
   uintptr_t texture;
   size_t vertex_count;
   struct video_coords *coords;
   void *matrix_data;
   enum gfx_display_prim_type prim_type;
   struct
   {
      unsigned id;
      const void *backend_data;
      size_t backend_data_size;
      bool active;
   } pipeline;
   float rotation;
   float scale_factor;
};

typedef struct gfx_display_ctx_rotate_draw
{
   bool scale_enable;
   float rotation;
   float scale_x;
   float scale_y;
   float scale_z;
   math_matrix_4x4 *matrix;
} gfx_display_ctx_rotate_draw_t;

typedef struct gfx_display_ctx_coord_draw
{
   const float *ptr;
} gfx_display_ctx_coord_draw_t;

typedef struct gfx_display_ctx_datetime
{
   char *s;
   size_t len;
   unsigned time_mode;
} gfx_display_ctx_datetime_t;

typedef struct gfx_display_ctx_powerstate
{
   char *s;
   size_t len;
   unsigned percent;
   bool battery_enabled;
   bool charging;
} gfx_display_ctx_powerstate_t;

#define gfx_display_set_alpha(color, alpha_value) (color[3] = color[7] = color[11] = color[15] = (alpha_value))

void gfx_display_free(void);

void gfx_display_init(void);

void gfx_display_blend_begin(video_frame_info_t *video_info);

void gfx_display_blend_end(video_frame_info_t *video_info);

void gfx_display_push_quad(
      unsigned width, unsigned height,
      const float *colors, int x1, int y1,
      int x2, int y2);

void gfx_display_snow(
      int16_t pointer_x,
      int16_t pointer_y,
      int width, int height);

void gfx_display_draw_cursor(
      video_frame_info_t *video_info,
      float *color, float cursor_size, uintptr_t texture,
      float x, float y, unsigned width, unsigned height);

void gfx_display_draw_text(
      const font_data_t *font, const char *text,
      float x, float y, int width, int height,
      uint32_t color, enum text_alignment text_align,
      float scale_factor, bool shadows_enable, float shadow_offset,
      bool draw_outside);

font_data_t *gfx_display_font(
      enum application_special_type type,
      float font_size,
      bool video_is_threaded);


void gfx_display_scissor_begin(video_frame_info_t *video_info, int x, int y, unsigned width, unsigned height);
void gfx_display_scissor_end(video_frame_info_t *video_info);

void gfx_display_font_free(font_data_t *font);

void gfx_display_coords_array_reset(void);
video_coord_array_t *gfx_display_get_coords_array(void);

void gfx_display_set_width(unsigned width);
void gfx_display_get_fb_size(unsigned *fb_width, unsigned *fb_height,
      size_t *fb_pitch);
void gfx_display_set_height(unsigned height);
void gfx_display_set_header_height(unsigned height);
unsigned gfx_display_get_header_height(void);
size_t gfx_display_get_framebuffer_pitch(void);
void gfx_display_set_framebuffer_pitch(size_t pitch);

bool gfx_display_get_msg_force(void);
void gfx_display_set_msg_force(bool state);
bool gfx_display_get_update_pending(void);
void gfx_display_set_viewport(unsigned width, unsigned height);
void gfx_display_unset_viewport(unsigned width, unsigned height);
bool gfx_display_get_framebuffer_dirty_flag(void);
void gfx_display_set_framebuffer_dirty_flag(void);
void gfx_display_unset_framebuffer_dirty_flag(void);
bool gfx_display_init_first_driver(bool video_is_threaded);
bool gfx_display_restore_clear_color(void);
void gfx_display_clear_color(gfx_display_ctx_clearcolor_t *color,
      video_frame_info_t *video_info);
void gfx_display_draw(gfx_display_ctx_draw_t *draw,
      video_frame_info_t *video_info);
void gfx_display_draw_blend(gfx_display_ctx_draw_t *draw,
      video_frame_info_t *video_info);
void gfx_display_draw_keyboard(
      uintptr_t hover_texture,
      const font_data_t *font,
      video_frame_info_t *video_info,
      char *grid[], unsigned id,
      unsigned text_color);

void gfx_display_draw_pipeline(gfx_display_ctx_draw_t *draw,
      video_frame_info_t *video_info);
void gfx_display_draw_bg(
      gfx_display_ctx_draw_t *draw,
      video_frame_info_t *video_info,
      bool add_opacity, float opacity_override);
void gfx_display_draw_gradient(
      gfx_display_ctx_draw_t *draw,
      video_frame_info_t *video_info);
void gfx_display_draw_quad(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color);
void gfx_display_draw_polygon(
      video_frame_info_t *video_info,
      int x1, int y1,
      int x2, int y2,
      int x3, int y3,
      int x4, int y4,
      unsigned width, unsigned height,
      float *color);
void gfx_display_draw_texture(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color, uintptr_t texture);
void gfx_display_draw_texture_slice(
      video_frame_info_t *video_info,
      int x, int y, unsigned w, unsigned h,
      unsigned new_w, unsigned new_h, unsigned width, unsigned height,
      float *color, unsigned offset, float scale_factor, uintptr_t texture);

void gfx_display_rotate_z(gfx_display_ctx_rotate_draw_t *draw,
      video_frame_info_t *video_info);

font_data_t *gfx_display_font_file(char* fontpath, float font_size, bool is_threaded);

bool gfx_display_reset_textures_list(
      const char *texture_path, const char *iconpath,
      uintptr_t *item, enum texture_filter_type filter_type,
      unsigned *width, unsigned *height);

bool gfx_display_reset_textures_list_buffer(
        uintptr_t *item, enum texture_filter_type filter_type,
        void* buffer, unsigned buffer_len, enum image_type_enum image_type,
        unsigned *width, unsigned *height);

/* Returns the OSK key at a given position */
int gfx_display_osk_ptr_at_pos(void *data, int x, int y,
      unsigned width, unsigned height);

enum menu_driver_id_type gfx_display_get_driver_id(void);

void gfx_display_set_driver_id(enum menu_driver_id_type type);

float gfx_display_get_dpi_scale(unsigned width, unsigned height);

float gfx_display_get_widget_dpi_scale(unsigned width, unsigned height);

float gfx_display_get_widget_pixel_scale(unsigned width, unsigned height);

void gfx_display_allocate_white_texture(void);

bool gfx_display_driver_exists(const char *s);

bool gfx_display_init_first_driver(bool video_is_threaded);

extern uintptr_t gfx_display_white_texture;

extern gfx_display_ctx_driver_t gfx_display_ctx_gl;
extern gfx_display_ctx_driver_t gfx_display_ctx_gl_core;
extern gfx_display_ctx_driver_t gfx_display_ctx_gl1;
extern gfx_display_ctx_driver_t gfx_display_ctx_vulkan;
extern gfx_display_ctx_driver_t gfx_display_ctx_metal;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d8;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d9;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d10;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d11;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d12;
extern gfx_display_ctx_driver_t gfx_display_ctx_vita2d;
extern gfx_display_ctx_driver_t gfx_display_ctx_ctr;
extern gfx_display_ctx_driver_t gfx_display_ctx_wiiu;
extern gfx_display_ctx_driver_t gfx_display_ctx_gdi;
extern gfx_display_ctx_driver_t gfx_display_ctx_switch;

RETRO_END_DECLS

#endif
