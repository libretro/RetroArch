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
   unsigned vp_out_width;
   unsigned vp_out_height;
   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
   unsigned rotation;
   unsigned overlays;

   GLuint tex;
   GLuint menu_tex;
   GLuint texture[GFX_MAX_TEXTURES];

   bool fullscreen;
   bool menu_rgb32;
   bool menu_size_changed;
   bool rgb32;
   bool supports_bgra;
   bool keep_aspect;
   bool should_resize;
   bool menu_texture_enable;
   bool menu_texture_full_screen;
   bool have_sync;
   bool smooth;
   bool menu_smooth;
   bool overlay_enable;
   bool overlay_full_screen;
   bool shared_context_use;
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
