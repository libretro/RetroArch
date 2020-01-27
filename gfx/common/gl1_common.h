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

#ifndef __GL1_COMMON_H
#define __GL1_COMMON_H

#include <retro_environment.h>
#include <retro_inline.h>
#include <gfx/math/matrix_4x4.h>

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

#include "../../retroarch.h"

#ifdef VITA
#define GL_RGBA8     GL_RGBA
#define GL_RGB8      GL_RGB
#define GL_BGRA_EXT  GL_RGBA // Currently unsupported in vitaGL
#define GL_CLAMP     GL_CLAMP_TO_EDGE
#endif

#define RARCH_GL1_INTERNAL_FORMAT32 GL_RGBA8
#define RARCH_GL1_TEXTURE_TYPE32 GL_BGRA_EXT
#define RARCH_GL1_FORMAT32 GL_UNSIGNED_BYTE

struct string_list;

typedef struct gl1
{
   unsigned video_width;
   unsigned video_height;
   unsigned screen_width;
   unsigned screen_height;
   int version_major;
   int version_minor;
   void *ctx_data;
   const gfx_ctx_driver_t *ctx_driver;
   GLuint tex;
   GLuint menu_tex;
   struct string_list *extensions;
   bool supports_bgra;
   struct video_viewport vp;
   bool keep_aspect;
   unsigned vp_out_width;
   unsigned vp_out_height;
   bool should_resize;
   struct video_coords coords;
   GLuint texture[GFX_MAX_TEXTURES];
   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
   math_matrix_4x4 mvp, mvp_no_rot;
   struct video_tex_info tex_info;
   const float *vertex_ptr;
   const float *white_color_ptr;
   unsigned rotation;
   bool menu_texture_enable;
   bool menu_texture_full_screen;
   bool have_sync;
   bool smooth;
   bool menu_smooth;
   void *readback_buffer_screenshot;
   bool overlay_enable;
   bool overlay_full_screen;
   GLuint *overlay_tex;
   unsigned overlays;
   float *overlay_vertex_coord;
   float *overlay_tex_coord;
   float *overlay_color_coord;
   bool fullscreen;
} gl1_t;

static INLINE void gl1_bind_texture(GLuint id, GLint wrap_mode, GLint mag_filter,
      GLint min_filter)
{
   glBindTexture(GL_TEXTURE_2D, id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
}

#endif
