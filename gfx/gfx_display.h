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
#include <formats/image.h>
#include <gfx/math/matrix_4x4.h>

#include "../retroarch.h"
#include "../gfx/font_driver.h"

RETRO_BEGIN_DECLS

enum gfx_display_flags
{
   GFX_DISP_FLAG_HAS_WINDOWED     = (1 << 0),
   GFX_DISP_FLAG_MSG_FORCE        = (1 << 1),
   GFX_DISP_FLAG_FB_DIRTY         = (1 << 2)
};

#define GFX_SHADOW_ALPHA 0.50f

/* Number of pixels corner-to-corner on a 1080p
 * display:
 * > sqrt((1920 * 1920) + (1080 * 1080))
 * Note: This is a double, so no suffix */
#define DIAGONAL_PIXELS_1080P 2202.90717008229831581901

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

#define gfx_display_set_alpha(color, alpha_value) (color[3] = color[7] = color[11] = color[15] = (alpha_value))

/* Returns true if an animation is still active or
 * when the display framebuffer still is dirty and
 * therefore it still needs to be rendered onscreen.
 *
 * This macro can be used for optimization purposes
 * so that we don't have to render the display graphics per-frame
 * unless a change has happened.
 * */
#define GFX_DISPLAY_GET_UPDATE_PENDING(p_anim, p_disp) (ANIM_IS_ACTIVE(p_anim) || (p_disp->flags & GFX_DISP_FLAG_FB_DIRTY))

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
   GFX_VIDEO_DRIVER_DIRECT3D9_CG,
   GFX_VIDEO_DRIVER_DIRECT3D9_HLSL,
   GFX_VIDEO_DRIVER_DIRECT3D10,
   GFX_VIDEO_DRIVER_DIRECT3D11,
   GFX_VIDEO_DRIVER_DIRECT3D12,
   GFX_VIDEO_DRIVER_VITA2D,
   GFX_VIDEO_DRIVER_CTR,
   GFX_VIDEO_DRIVER_WIIU,
   GFX_VIDEO_DRIVER_GDI,
   GFX_VIDEO_DRIVER_SWITCH,
   GFX_VIDEO_DRIVER_RSX
};

typedef struct gfx_display_ctx_draw gfx_display_ctx_draw_t;

typedef struct gfx_display gfx_display_t;

typedef struct gfx_display_ctx_driver
{
   /* Draw graphics to the screen. */
   void (*draw)(gfx_display_ctx_draw_t *draw,
         void *data, unsigned video_width, unsigned video_height);
   /* Draw one of the menu pipeline shaders. */
   void (*draw_pipeline)(gfx_display_ctx_draw_t *draw,
         gfx_display_t *p_disp,
         void *data, unsigned video_width, unsigned video_height);
   /* Start blending operation. */
   void (*blend_begin)(void *data);
   /* Finish blending operation. */
   void (*blend_end)(void *data);
   /* Get the default Model-View-Projection matrix */
   void *(*get_default_mvp)(void *data);
   /* Get the default vertices matrix */
   const float *(*get_default_vertices)(void);
   /* Get the default texture coordinates matrix */
   const float *(*get_default_tex_coords)(void);
   enum font_driver_render_api  font_type;
   enum gfx_display_driver_type type;
   const char *ident;
   bool handles_transform;
   /* Enables and disables scissoring */
   void (*scissor_begin)(void *data, unsigned video_width,
         unsigned video_height,
         int x, int y, unsigned width, unsigned height);
   void (*scissor_end)(void *data, unsigned video_width,
         unsigned video_height);
} gfx_display_ctx_driver_t;

struct gfx_display_ctx_draw
{
   float *color;
   const float *vertex;
   const float *tex_coord;
   const void *backend_data;
   struct video_coords *coords;
   void *matrix_data;
   uintptr_t texture;
   size_t vertex_count;
   size_t backend_data_size;
   unsigned width;
   unsigned height;
   unsigned pipeline_id;
   float x;
   float y;
   float rotation;
   float scale_factor;
   enum gfx_display_prim_type prim_type;
   bool pipeline_active;
};

typedef struct gfx_display_ctx_coord_draw
{
   const float *ptr;
} gfx_display_ctx_coord_draw_t;

typedef struct gfx_display_ctx_datetime
{
   char *s;
   size_t len;
   unsigned time_mode;
   unsigned date_separator;
} gfx_display_ctx_datetime_t;

typedef struct gfx_display_ctx_powerstate
{
   char *s;
   size_t len;
   unsigned percent;
   bool battery_enabled;
   bool charging;
} gfx_display_ctx_powerstate_t;

struct gfx_display
{
   gfx_display_ctx_driver_t *dispctx;
   video_coord_array_t dispca; /* ptr alignment */

   /* Width, height and pitch of the display framebuffer */
   size_t   framebuf_pitch;
   unsigned framebuf_width;
   unsigned framebuf_height;

   /* Height of the display header */
   unsigned header_height;

   enum menu_driver_id_type menu_driver_id;

   uint8_t flags;
};

void gfx_display_free(void);

void gfx_display_init(void);

void gfx_display_draw_cursor(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      bool cursor_visible,
      float *color, float cursor_size, uintptr_t texture,
      float x, float y, unsigned width, unsigned height);

void gfx_display_draw_text(
      const font_data_t *font, const char *text,
      float x, float y, int width, int height,
      uint32_t color, enum text_alignment text_align,
      float scale_factor, bool shadows_enable, float shadow_offset,
      bool draw_outside);

void gfx_display_scissor_begin(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned width, unsigned height);

void gfx_display_font_free(font_data_t *font);

void gfx_display_set_width(unsigned width);
void gfx_display_get_fb_size(unsigned *fb_width, unsigned *fb_height,
      size_t *fb_pitch);
void gfx_display_set_height(unsigned height);
void gfx_display_set_framebuffer_pitch(size_t pitch);

bool gfx_display_init_first_driver(gfx_display_t *p_disp,
      bool video_is_threaded);

gfx_display_t *disp_get_ptr(void);

void gfx_display_draw_keyboard(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      uintptr_t hover_texture,
      const font_data_t *font,
      char *grid[], unsigned id,
      unsigned text_color);

void gfx_display_draw_bg(
      gfx_display_t *p_disp,
      gfx_display_ctx_draw_t *draw,
      void *userdata,
      bool add_opacity, float opacity_override);

void gfx_display_draw_quad(
      gfx_display_t *p_disp,
      void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height,
      float *color,
      uintptr_t *texture);

void gfx_display_draw_texture_slice(
      gfx_display_t *p_disp,
      void *userdata,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned w, unsigned h,
      unsigned new_w, unsigned new_h,
      unsigned width, unsigned height,
      float *color, unsigned offset, float scale_factor, uintptr_t texture,
      math_matrix_4x4 *mymat);

void gfx_display_rotate_z(gfx_display_t *p_disp,
      math_matrix_4x4 *matrix, float cosine, float sine, void *data);

font_data_t *gfx_display_font_file(gfx_display_t *p_disp,
      char* fontpath, float font_size, bool is_threaded);

bool gfx_display_reset_textures_list(
      const char *texture_path,
      const char *iconpath,
      uintptr_t *item,
      enum texture_filter_type filter_type,
      unsigned *width,
      unsigned *height);

bool gfx_display_reset_textures_list_buffer(
        uintptr_t *item,
        enum texture_filter_type filter_type,
        void* buffer,
        unsigned buffer_len,
        enum image_type_enum image_type,
        unsigned *width,
        unsigned *height);

/* Returns the OSK key at a given position */
int gfx_display_osk_ptr_at_pos(void *data, int x, int y,
      unsigned width, unsigned height);

float gfx_display_get_adjusted_scale(
      gfx_display_t *p_disp,
      float base_scale, float scale_factor, unsigned width);

float gfx_display_get_dpi_scale_internal(unsigned width, unsigned height);

float gfx_display_get_dpi_scale(
      gfx_display_t *p_disp,
      void *settings_data,
      unsigned width, unsigned height,
      bool fullscreen,
      bool is_widget);

void gfx_display_deinit_white_texture(void);

void gfx_display_init_white_texture(void);

bool gfx_display_init_first_driver(gfx_display_t *p_disp,
      bool video_is_threaded);

extern gfx_display_ctx_driver_t gfx_display_ctx_gl;
extern gfx_display_ctx_driver_t gfx_display_ctx_gl3;
extern gfx_display_ctx_driver_t gfx_display_ctx_gl1;
extern gfx_display_ctx_driver_t gfx_display_ctx_vulkan;
extern gfx_display_ctx_driver_t gfx_display_ctx_metal;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d8;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d9_cg;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d9_hlsl;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d10;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d11;
extern gfx_display_ctx_driver_t gfx_display_ctx_d3d12;
extern gfx_display_ctx_driver_t gfx_display_ctx_vita2d;
extern gfx_display_ctx_driver_t gfx_display_ctx_ctr;
extern gfx_display_ctx_driver_t gfx_display_ctx_wiiu;
extern gfx_display_ctx_driver_t gfx_display_ctx_gdi;
extern gfx_display_ctx_driver_t gfx_display_ctx_switch;
extern gfx_display_ctx_driver_t gfx_display_ctx_rsx;

RETRO_END_DECLS

#endif
