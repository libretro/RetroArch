/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
 *  copyright (c) 2016-2019 - Brad Parker
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

#ifndef __GL1_DEFINES_H
#define __GL1_DEFINES_H

#include <retro_environment.h>
#include <retro_inline.h>
#include <gfx/math/matrix_4x4.h>
#include <lists/string_list.h>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef VITA
#include <vitaGL.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#endif

#include "../video_driver.h"

#ifdef VITA
#define GL_RGBA8                    GL_RGBA
#define GL_RGB8                     GL_RGB
#define GL_BGRA_EXT                 GL_RGBA /* Currently unsupported in vitaGL */
#define GL_CLAMP                    GL_CLAMP_TO_EDGE
#endif

#define RARCH_GL1_INTERNAL_FORMAT32 GL_RGBA8
#define RARCH_GL1_TEXTURE_TYPE32    GL_BGRA_EXT
#define RARCH_GL1_FORMAT32          GL_UNSIGNED_BYTE

enum gl1_flags
{
   GL1_FLAG_FULLSCREEN              = (1 << 0),
   GL1_FLAG_MENU_SIZE_CHANGED       = (1 << 1),
   GL1_FLAG_RGB32                   = (1 << 2),
   GL1_FLAG_SUPPORTS_BGRA           = (1 << 3),
   GL1_FLAG_KEEP_ASPECT             = (1 << 4),
   GL1_FLAG_SHOULD_RESIZE           = (1 << 5),
   GL1_FLAG_MENU_TEXTURE_ENABLE     = (1 << 6),
   GL1_FLAG_MENU_TEXTURE_FULLSCREEN = (1 << 7),
   GL1_FLAG_SMOOTH                  = (1 << 8),
   GL1_FLAG_MENU_SMOOTH             = (1 << 9),
   GL1_FLAG_OVERLAY_ENABLE          = (1 << 10),
   GL1_FLAG_OVERLAY_FULLSCREEN      = (1 << 11),
   GL1_FLAG_FRAME_DUPE_LOCK         = (1 << 12)
};

typedef struct gl1
{
   struct video_viewport vp;
   struct video_coords coords;
   math_matrix_4x4 mvp, mvp_no_rot;

   void *ctx_data;
   const gfx_ctx_driver_t *ctx_driver;
   struct string_list *extensions;
   struct video_tex_info tex_info;
   void *readback_buffer_screenshot;
   GLuint *overlay_tex;
   float *overlay_vertex_coord;
   float *overlay_tex_coord;
   float *overlay_color_coord;
   const float *vertex_ptr;
   const float *white_color_ptr;
   unsigned char *menu_frame;
   unsigned char *video_buf;
   unsigned char *menu_video_buf;

   int version_major;
   int version_minor;
   unsigned video_width;
   unsigned video_height;
   unsigned video_pitch;
   unsigned screen_width;
   unsigned screen_height;
   unsigned menu_width;
   unsigned menu_height;
   unsigned menu_pitch;
   unsigned video_bits;
   unsigned menu_bits;
   unsigned out_vp_width;
   unsigned out_vp_height;
   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
   unsigned rotation;
   unsigned overlays;

   GLuint tex;
   GLuint menu_tex;
   GLuint texture[GFX_MAX_TEXTURES];

   uint16_t flags;
} gl1_t;

#endif
