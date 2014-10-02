/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include "math/matrix.h"
#include "gfx_context.h"
#include "scaler/scaler.h"
#include "fonts/gl_font.h"
#include "shader/shader_common.h"
#include "shader/shader_parse.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <string.h>

#ifdef HAVE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#include "glsym/glsym.h"

#define context_bind_hw_render(gl, enable)               if (gl->shared_context_use && gl->ctx_driver->bind_hw_render) gl->ctx_driver->bind_hw_render(gl, enable)

#ifdef HAVE_EGL
#define context_init_egl_image_buffer_func(gl, video)    gl->ctx_driver->init_egl_image_buffer(gl, video)
#define context_write_egl_image_func(gl, frame, width, height, pitch, base_size, tex_index, img) \
   gl->ctx_driver->write_egl_image(gl, frame, width, height, pitch, base_size, tex_index,img)
#endif

#if (!defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3))
#define HAVE_GL_ASYNC_READBACK
#endif

#if defined(HAVE_PSGL)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#elif defined(OSX_PPC)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#else
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0
#endif

#if defined(HAVE_OPENGLES2)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#elif defined(OSX_PPC)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_EXT
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_EXT
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_EXT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_EXT
#elif defined(HAVE_PSGL) && !defined(HAVE_GCMGL)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_OES
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_SCE
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_OES
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_OES
#else
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#endif

#ifdef OSX_PPC
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_EXT
#elif defined(HAVE_PSGL) && !defined(HAVE_GCMGL)
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_OES
#else
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE
#endif

#if defined(HAVE_PSGL) && !defined(HAVE_GCMGL)
#define glGenerateMipmap glGenerateMipmapOES
#endif

#ifdef HAVE_FBO

#if defined(__APPLE__) || defined(HAVE_PSGL)
#define GL_RGBA32F GL_RGBA32F_ARB
#endif

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
         RARCH_ERR("GL: Invalid value.\n");
         break;
      case GL_INVALID_OPERATION:
         RARCH_ERR("GL: Invalid operation.\n");
         break;
      case GL_OUT_OF_MEMORY:
         RARCH_ERR("GL: Out of memory.\n");
         break;
      case GL_NO_ERROR:
         return true;
   }

   RARCH_ERR("Non specified GL error.\n");
   return false;
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

struct gl_ortho
{
   GLfloat left;
   GLfloat right;
   GLfloat bottom;
   GLfloat top;
   GLfloat znear;
   GLfloat zfar;
};

struct gl_tex_info
{
   GLuint tex;
   GLfloat input_size[2];
   GLfloat tex_size[2];
   GLfloat coord[8];
};

struct gl_coords
{
   const GLfloat *vertex;
   const GLfloat *color;
   const GLfloat *tex_coord;
   const GLfloat *lut_tex_coord;
   unsigned vertices;
};

#define MAX_SHADERS 16
#define MAX_TEXTURES 8

typedef struct gl
{
   const gfx_ctx_driver_t *ctx_driver;
   const shader_backend_t *shader;

   bool vsync;
   GLuint texture[MAX_TEXTURES];
   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
   struct gl_tex_info tex_info;
   struct gl_tex_info prev_info[MAX_TEXTURES];
   GLuint tex_mag_filter;
   GLuint tex_min_filter;
   bool tex_mipmap;

   void *empty_buf;

   void *conv_buffer;
   struct scaler_ctx scaler;

   unsigned frame_count;

#ifdef HAVE_FBO
   /* Render-to-texture, multipass shaders. */
   GLuint fbo[MAX_SHADERS];
   GLuint fbo_texture[MAX_SHADERS];
   struct gl_fbo_rect fbo_rect[MAX_SHADERS];
   struct gfx_fbo_scale fbo_scale[MAX_SHADERS];
   int fbo_pass;
   bool fbo_inited;

   GLuint hw_render_fbo[MAX_TEXTURES];
   GLuint hw_render_depth[MAX_TEXTURES];
   bool hw_render_fbo_init;
   bool hw_render_depth_init;
   bool has_fp_fbo;
   bool has_srgb_fbo;
   bool has_srgb_fbo_gles3;
#endif
   bool hw_render_use;
   bool shared_context_use;

   bool should_resize;
   bool quitting;
   bool fullscreen;
   bool keep_aspect;
   unsigned rotation;

   unsigned full_x, full_y;

   unsigned win_width;
   unsigned win_height;
   struct rarch_viewport vp;
   unsigned vp_out_width;
   unsigned vp_out_height;
   unsigned last_width[MAX_TEXTURES];
   unsigned last_height[MAX_TEXTURES];
   unsigned tex_w, tex_h;
   math_matrix mvp, mvp_no_rot;

   struct gl_coords coords;
   const GLfloat *vertex_ptr;
   const GLfloat *white_color_ptr;

   GLuint pbo;

   GLenum internal_fmt;
   GLenum texture_type; /* RGB565 or ARGB */
   GLenum texture_fmt;
   GLenum wrap_mode;
   unsigned base_size; /* 2 or 4 */
#ifdef HAVE_OPENGLES
   bool support_unpack_row_length;
#else
   bool have_es2_compat;
#endif

   /* Fonts */
   const gl_font_renderer_t *font_driver;
   void *font_handle;

   bool egl_images;
   video_info_t video_info;

#ifdef HAVE_OVERLAY
   unsigned overlays;
   bool overlay_enable;
   bool overlay_full_screen;
   GLuint *overlay_tex;
   GLfloat *overlay_vertex_coord;
   GLfloat *overlay_tex_coord;
   GLfloat *overlay_color_coord;
#endif

#ifdef HAVE_GL_ASYNC_READBACK
   /* PBOs used for asynchronous viewport readbacks. */
   GLuint pbo_readback[4];
   bool pbo_readback_valid[4];
   bool pbo_readback_enable;
   unsigned pbo_readback_index;
   struct scaler_ctx pbo_readback_scaler;
#endif
   void *readback_buffer_screenshot;

#if defined(HAVE_MENU)
   GLuint menu_texture;
   bool menu_texture_enable;
   bool menu_texture_full_screen;
   GLfloat menu_texture_alpha;
#endif

#ifdef HAVE_GL_SYNC
#define MAX_FENCES 4
   bool have_sync;
   GLsync fences[MAX_FENCES];
   unsigned fence_count;
#endif

   bool core_context;
   GLuint vao;
} gl_t;

#if defined(HAVE_PSGL)
#define RARCH_GL_INTERNAL_FORMAT32 GL_ARGB_SCE
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB5 /* TODO: Verify if this is really 565 or just 555. */
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_RGB5
#elif defined(HAVE_OPENGLES)
/* Imgtec/SGX headers have this missing. */
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifdef IOS
/* Stupid Apple. */
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA
#else
#define RARCH_GL_INTERNAL_FORMAT32 GL_BGRA_EXT
#endif
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA_EXT
#define RARCH_GL_TEXTURE_TYPE16 GL_RGB
#define RARCH_GL_FORMAT32 GL_UNSIGNED_BYTE
#define RARCH_GL_FORMAT16 GL_UNSIGNED_SHORT_5_6_5
#else
/* On desktop, we always use 32-bit. */
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA8
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGBA8
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_UNSIGNED_INT_8_8_8_8_REV

/* GL_RGB565 internal format isn't in desktop GL 
 * until 4.1 core (ARB_ES2_compatibility).
 * Check for this. */
#ifndef GL_RGB565
#define GL_RGB565 0x8D62
#endif
#define RARCH_GL_INTERNAL_FORMAT16_565 GL_RGB565
#define RARCH_GL_TEXTURE_TYPE16_565 GL_RGB
#define RARCH_GL_FORMAT16_565 GL_UNSIGNED_SHORT_5_6_5
#endif

/* Platform specific workarounds/hacks. */
#if defined(__CELLOS_LV2__)
#define NO_GL_READ_PIXELS

/* Performance hacks. */
#ifdef HAVE_GCMGL

extern GLvoid* glMapBufferTextureReferenceRA( GLenum target, GLenum access );

extern GLboolean glUnmapBufferTextureReferenceRA( GLenum target );

extern void glBufferSubDataTextureReferenceRA( GLenum target,
      GLintptr offset, GLsizeiptr size, const GLvoid *data );
#define glMapBuffer(target, access) glMapBufferTextureReferenceRA(target, access)
#define glUnmapBuffer(target) glUnmapBufferTextureReferenceRA(target)
#define glBufferSubData(target, offset, size, data) glBufferSubDataTextureReferenceRA(target, offset, size, data)
#endif
#endif

#if defined(HAVE_OPENGL_MODERN) || defined(HAVE_OPENGLES2) || defined(HAVE_PSGL)
#define NO_GL_FF_VERTEX
#endif

#if defined(HAVE_OPENGL_MODERN) || defined(HAVE_OPENGLES2) || defined(HAVE_PSGL)
#define NO_GL_FF_MATRIX
#endif

#if defined(HAVE_OPENGLES2) /* TODO: Figure out exactly what. */
#define NO_GL_CLAMP_TO_BORDER
#endif

#if defined(HAVE_OPENGLES)
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH  0x0CF2
#endif

#ifndef GL_SRGB_ALPHA_EXT
#define GL_SRGB_ALPHA_EXT 0x8C42
#endif
#endif

void gl_set_projection(gl_t *gl, struct gl_ortho *ortho, bool allow_rotate);

void gl_set_viewport(gl_t *gl, unsigned width, unsigned height,
      bool force_full, bool allow_rotate);

void gl_shader_set_coords(gl_t *gl,
      const struct gl_coords *coords, const math_matrix *mat);

#endif

