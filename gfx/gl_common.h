/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#ifndef __GL_COMMON_H
#define __GL_COMMON_H

#include "../general.h"
#include "fonts/fonts.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#elif defined(__CELLOS_LV2__)
#include <PSGL/psgl.h>
#include <PSGL/psglu.h>
#include <GLES/glext.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

static inline bool gl_check_error(void)
{
   int error = glGetError();
   switch (error)
   {
      case GL_INVALID_ENUM:
         RARCH_ERR("GL: Invalid enum.\n");
         break;
      case GL_INVALID_VALUE:
         RARCH_ERR("GL: Invalid value. (You're not alone.)\n");
         break;
      case GL_INVALID_OPERATION:
         RARCH_ERR("GL: Invalid operation.\n");
         break;
      case GL_STACK_OVERFLOW:
         RARCH_ERR("GL: Stack overflow. (wtf)\n");
         break;
      case GL_STACK_UNDERFLOW:
         RARCH_ERR("GL: Stack underflow. (:v)\n");
         break;
      case GL_OUT_OF_MEMORY:
         RARCH_ERR("GL: Out of memory. Harhar.\n");
         break;
      case GL_NO_ERROR:
         return true;
      default:
         RARCH_ERR("Non specified error :v\n");
   }

   return false;
}

static inline unsigned get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

struct gl_fbo_rect
{
   unsigned img_width;
   unsigned img_height;
   unsigned max_img_width;
   unsigned max_img_height;
   unsigned width;
   unsigned height;
};

enum gl_scale_type
{
   RARCH_SCALE_ABSOLUTE,
   RARCH_SCALE_INPUT,
   RARCH_SCALE_VIEWPORT
};

struct gl_fbo_scale
{
   enum gl_scale_type type_x;
   enum gl_scale_type type_y;
   float scale_x;
   float scale_y;
   unsigned abs_x;
   unsigned abs_y;
   bool valid;
};

struct gl_tex_info
{
   GLuint tex;
   GLfloat input_size[2];
   GLfloat tex_size[2];
   GLfloat coord[8];
};

#define MAX_SHADERS 16

#if defined(HAVE_XML) || defined(HAVE_CG)
#define TEXTURES 8
#else
#define TEXTURES 1
#endif
#define TEXTURES_MASK (TEXTURES - 1)

typedef struct gl
{
#ifdef RARCH_CONSOLE
   bool block_swap;
#endif
#ifdef HAVE_CG_MENU
   bool menu_render;
   GLuint menu_texture_id;
#endif
   bool vsync;
   GLuint texture[TEXTURES];
   unsigned tex_index; // For use with PREV.
   struct gl_tex_info prev_info[TEXTURES];
   GLuint tex_filter;

   void *empty_buf;

   unsigned frame_count;

#ifdef HAVE_FBO
   // Render-to-texture, multipass shaders
   GLuint fbo[MAX_SHADERS];
   GLuint fbo_texture[MAX_SHADERS];
   struct gl_fbo_rect fbo_rect[MAX_SHADERS];
   struct gl_fbo_scale fbo_scale[MAX_SHADERS];
   bool render_to_tex;
   int fbo_pass;
   bool fbo_inited;
#endif

   bool should_resize;
   bool quitting;
   bool fullscreen;
   bool keep_aspect;
   unsigned rotation;

   unsigned full_x, full_y;

   unsigned win_width;
   unsigned win_height;
   unsigned vp_width, vp_out_width;
   unsigned vp_height, vp_out_height;
   unsigned last_width[TEXTURES];
   unsigned last_height[TEXTURES];
   unsigned tex_w, tex_h;
   GLfloat tex_coords[8];

#ifdef __CELLOS_LV2__
   GLuint pbo;
#endif
   GLenum texture_type; // XBGR1555 or ARGB
   GLenum texture_fmt;
   unsigned base_size; // 2 or 4

#ifdef HAVE_FREETYPE
   font_renderer_t *font;
   GLuint font_tex;
   int font_tex_w, font_tex_h;
   void *font_tex_empty_buf;
   char font_last_msg[256];
   int font_last_width, font_last_height;
   GLfloat font_color[16];
   GLfloat font_color_dark[16];
#endif
} gl_t;

// Windows ... <_<
#if (defined(HAVE_XML) || defined(HAVE_CG)) && defined(_WIN32)
extern PFNGLCLIENTACTIVETEXTUREPROC pglClientActiveTexture;
extern PFNGLACTIVETEXTUREPROC pglActiveTexture;
#else
#define pglClientActiveTexture glClientActiveTexture
#define pglActiveTexture glActiveTexture
#endif

#define RARCH_GL_INTERNAL_FORMAT GL_RGBA
#define RARCH_GL_TEXTURE_TYPE GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_UNSIGNED_SHORT_1_5_5_5_REV

void gl_shader_use(unsigned index);
void gl_set_projection(gl_t *gl, bool allow_rotate);
void gl_set_viewport(gl_t *gl, unsigned width, unsigned height, bool force_full, bool allow_rotate);

#endif
