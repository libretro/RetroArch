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

#ifndef __VIDEO_DEFINES__H
#define __VIDEO_DEFINES__H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum
{
   TEXTURES = 8,
   TEXTURESMASK = TEXTURES - 1
};

enum texture_filter_type
{
   TEXTURE_FILTER_LINEAR = 0,
   TEXTURE_FILTER_NEAREST,
   TEXTURE_FILTER_MIPMAP_LINEAR,
   TEXTURE_FILTER_MIPMAP_NEAREST
};

enum aspect_ratio
{
   ASPECT_RATIO_4_3 = 0,
   ASPECT_RATIO_16_9,
   ASPECT_RATIO_16_10,
   ASPECT_RATIO_16_15,
   ASPECT_RATIO_21_9,
   ASPECT_RATIO_1_1,
   ASPECT_RATIO_2_1,
   ASPECT_RATIO_3_2,
   ASPECT_RATIO_3_4,
   ASPECT_RATIO_4_1,
   ASPECT_RATIO_4_4,
   ASPECT_RATIO_5_4,
   ASPECT_RATIO_6_5,
   ASPECT_RATIO_7_9,
   ASPECT_RATIO_8_3,
   ASPECT_RATIO_8_7,
   ASPECT_RATIO_19_12,
   ASPECT_RATIO_19_14,
   ASPECT_RATIO_30_17,
   ASPECT_RATIO_32_9,
   ASPECT_RATIO_CONFIG,
   ASPECT_RATIO_SQUARE,
   ASPECT_RATIO_CORE,
   ASPECT_RATIO_CUSTOM,
   ASPECT_RATIO_FULL,

   ASPECT_RATIO_END
};

enum rotation
{
   ORIENTATION_NORMAL = 0,
   ORIENTATION_VERTICAL,
   ORIENTATION_FLIPPED,
   ORIENTATION_FLIPPED_ROTATED,
   ORIENTATION_END
};

enum video_rotation_type
{
   VIDEO_ROTATION_NORMAL = 0,
   VIDEO_ROTATION_90_DEG,
   VIDEO_ROTATION_180_DEG,
   VIDEO_ROTATION_270_DEG
};

enum autoswitch_refresh_rate
{
   AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN = 0,
   AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   AUTOSWITCH_REFRESH_RATE_OFF,
   AUTOSWITCH_REFRESH_RATE_LAST
};

enum rarch_display_type
{
   /* Non-bindable types like consoles, KMS, VideoCore, etc. */
   RARCH_DISPLAY_NONE = 0,
   /* video_display => Display*, video_window => Window */
   RARCH_DISPLAY_X11,
   /* video_display => N/A, video_window => HWND */
   RARCH_DISPLAY_WIN32,
   RARCH_DISPLAY_WAYLAND,
   RARCH_DISPLAY_OSX,
   RARCH_DISPLAY_KMS
};

enum font_driver_render_api
{
   FONT_DRIVER_RENDER_DONT_CARE,
   FONT_DRIVER_RENDER_OPENGL_API,
   FONT_DRIVER_RENDER_OPENGL_CORE_API,
   FONT_DRIVER_RENDER_OPENGL1_API,
   FONT_DRIVER_RENDER_D3D8_API,
   FONT_DRIVER_RENDER_D3D9_API,
   FONT_DRIVER_RENDER_D3D10_API,
   FONT_DRIVER_RENDER_D3D11_API,
   FONT_DRIVER_RENDER_D3D12_API,
   FONT_DRIVER_RENDER_PS2,
   FONT_DRIVER_RENDER_VITA2D,
   FONT_DRIVER_RENDER_CTR,
   FONT_DRIVER_RENDER_WIIU,
   FONT_DRIVER_RENDER_VULKAN_API,
   FONT_DRIVER_RENDER_METAL_API,
   FONT_DRIVER_RENDER_CACA,
   FONT_DRIVER_RENDER_SIXEL,
   FONT_DRIVER_RENDER_NETWORK_VIDEO,
   FONT_DRIVER_RENDER_GDI,
   FONT_DRIVER_RENDER_VGA,
   FONT_DRIVER_RENDER_SWITCH,
   FONT_DRIVER_RENDER_RSX
};

enum text_alignment
{
   TEXT_ALIGN_LEFT = 0,
   TEXT_ALIGN_RIGHT,
   TEXT_ALIGN_CENTER
};

#ifndef COLOR_ABGR
#define COLOR_ABGR(r, g, b, a) (((unsigned)(a) << 24) | ((b) << 16) | ((g) << 8) | ((r) << 0))
#endif

#define LAST_ASPECT_RATIO ASPECT_RATIO_FULL

/* ABGR color format defines */

#define WHITE		  0xffffffffu
#define RED         0xff0000ffu
#define GREEN		  0xff00ff00u
#define BLUE        0xffff0000u
#define YELLOW      0xff00ffffu
#define PURPLE      0xffff00ffu
#define CYAN        0xffffff00u
#define ORANGE      0xff0063ffu
#define SILVER      0xff8c848cu
#define LIGHTBLUE   0xFFFFE0E0U
#define LIGHTORANGE 0xFFE0EEFFu

#define FONT_COLOR_RGBA(r, g, b, a) (((unsigned)(r) << 24) | ((g) << 16) | ((b) << 8) | ((a) << 0))
#define FONT_COLOR_GET_RED(col)   (((col) >> 24) & 0xff)
#define FONT_COLOR_GET_GREEN(col) (((col) >> 16) & 0xff)
#define FONT_COLOR_GET_BLUE(col)  (((col) >>  8) & 0xff)
#define FONT_COLOR_GET_ALPHA(col) (((col) >>  0) & 0xff)
#define FONT_COLOR_ARGB_TO_RGBA(col) ( (((col) >> 24) & 0xff) | (((unsigned)(col) << 8) & 0xffffff00) )

typedef struct video_viewport
{
   int x;
   int y;
   unsigned width;
   unsigned height;
   unsigned full_width;
   unsigned full_height;
} video_viewport_t;

typedef struct gfx_ctx_flags
{
   uint32_t flags;
} gfx_ctx_flags_t;

struct Size2D
{
   unsigned width, height;
};

enum gfx_ctx_api
{
   GFX_CTX_NONE = 0,
   GFX_CTX_OPENGL_API,
   GFX_CTX_OPENGL_ES_API,
   GFX_CTX_DIRECT3D8_API,
   GFX_CTX_DIRECT3D9_API,
   GFX_CTX_DIRECT3D10_API,
   GFX_CTX_DIRECT3D11_API,
   GFX_CTX_DIRECT3D12_API,
   GFX_CTX_OPENVG_API,
   GFX_CTX_VULKAN_API,
   GFX_CTX_METAL_API,
   GFX_CTX_RSX_API
};

enum display_metric_types
{
   DISPLAY_METRIC_NONE = 0,
   DISPLAY_METRIC_MM_WIDTH,
   DISPLAY_METRIC_MM_HEIGHT,
   DISPLAY_METRIC_DPI,
   DISPLAY_METRIC_PIXEL_WIDTH,
   DISPLAY_METRIC_PIXEL_HEIGHT
};

enum display_flags
{
   GFX_CTX_FLAGS_NONE            = 0,
   GFX_CTX_FLAGS_GL_CORE_CONTEXT,
   GFX_CTX_FLAGS_MULTISAMPLING,
   GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES,
   GFX_CTX_FLAGS_CUSTOMIZABLE_FRAME_LATENCY,
   GFX_CTX_FLAGS_HARD_SYNC,
   GFX_CTX_FLAGS_BLACK_FRAME_INSERTION,
   GFX_CTX_FLAGS_MENU_FRAME_FILTERING,
   GFX_CTX_FLAGS_ADAPTIVE_VSYNC,
   GFX_CTX_FLAGS_SHADERS_GLSL,
   GFX_CTX_FLAGS_SHADERS_CG,
   GFX_CTX_FLAGS_SHADERS_HLSL,
   GFX_CTX_FLAGS_SHADERS_SLANG,
   GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED,
   GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED,
   GFX_CTX_FLAGS_CRT_SWITCHRES,
   GFX_CTX_FLAGS_SUBFRAME_SHADERS
};

enum shader_uniform_type
{
   UNIFORM_1F = 0,
   UNIFORM_2F,
   UNIFORM_3F,
   UNIFORM_4F,
   UNIFORM_1FV,
   UNIFORM_2FV,
   UNIFORM_3FV,
   UNIFORM_4FV,
   UNIFORM_1I
};

enum shader_program_type
{
   SHADER_PROGRAM_VERTEX = 0,
   SHADER_PROGRAM_FRAGMENT,
   SHADER_PROGRAM_COMBINED
};

/* All coordinates and offsets are top-left oriented.
 *
 * This is a texture-atlas approach which allows text to
 * be drawn in a single draw call.
 *
 * It is up to the code using this interface to actually
 * generate proper vertex buffers and upload the atlas texture to GPU. */

struct font_glyph
{
   unsigned width;
   unsigned height;

   /* Texel coordinate offset for top-left pixel of this glyph. */
   unsigned atlas_offset_x;
   unsigned atlas_offset_y;

   /* When drawing this glyph, apply an offset to
    * current X/Y draw coordinate. */
   int draw_offset_x;
   int draw_offset_y;

   /* Advance X/Y draw coordinates after drawing this glyph. */
   int advance_x;
   int advance_y;
};

struct font_atlas
{
   uint8_t *buffer; /* Alpha channel. */
   unsigned width;
   unsigned height;
   bool dirty;
};

struct font_params
{
   /* Drop shadow offset.
    * If both are 0, no drop shadow will be rendered. */
   int drop_x, drop_y;

   /* ABGR. Use the macros. */
   uint32_t color;

   float x;
   float y;
   float scale;
   /* Drop shadow color multiplier. */
   float drop_mod;
   /* Drop shadow alpha */
   float drop_alpha;

   enum text_alignment text_align;

   bool full_screen;
};

struct font_line_metrics
{
   float height;
   float ascender;
   float descender;
};

struct video_fbo_rect
{
   unsigned img_width;
   unsigned img_height;
   unsigned max_img_width;
   unsigned max_img_height;
   unsigned width;
   unsigned height;
};

struct video_ortho
{
   float left;
   float right;
   float bottom;
   float top;
   float znear;
   float zfar;
};

struct video_tex_info
{
   unsigned int tex;
   float input_size[2];
   float tex_size[2];
   float coord[8];
};

typedef struct video_coords
{
   const float *vertex;
   const float *color;
   const float *tex_coord;
   const float *lut_tex_coord;
   const unsigned *index;
   unsigned vertices;
   unsigned indexes;
} video_coords_t;

typedef struct video_mut_coords
{
   float *vertex;
   float *color;
   float *tex_coord;
   float *lut_tex_coord;
   unsigned *index;
   unsigned vertices;
   unsigned indexes;
} video_mut_coords_t;

typedef struct video_coord_array
{
   video_mut_coords_t coords; /* ptr alignment */
   unsigned allocated;
} video_coord_array_t;

typedef struct video_font_raster_block
{
   video_coord_array_t carr; /* ptr alignment */
   bool fullscreen;
} video_font_raster_block_t;


RETRO_END_DECLS

#endif
