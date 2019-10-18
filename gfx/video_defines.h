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

enum rarch_display_type
{
   /* Non-bindable types like consoles, KMS, VideoCore, etc. */
   RARCH_DISPLAY_NONE = 0,
   /* video_display => Display*, video_window => Window */
   RARCH_DISPLAY_X11,
   /* video_display => N/A, video_window => HWND */
   RARCH_DISPLAY_WIN32,
   RARCH_DISPLAY_WAYLAND,
   RARCH_DISPLAY_OSX
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
   FONT_DRIVER_RENDER_FPGA,
   FONT_DRIVER_RENDER_SWITCH
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

#define LAST_ASPECT_RATIO ASPECT_RATIO_CUSTOM

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

RETRO_END_DECLS

#endif
