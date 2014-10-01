/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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

#ifdef _MSC_VER
#pragma comment(lib, "opengl32")
#endif

#include "../driver.h"
#include "../performance.h"
#include "scaler/scaler.h"
#include "image/image.h"
#include "../file.h"

#include <stdint.h>
#include "../libretro.h"
#include <stdio.h>
#include <string.h>
#include "../general.h"
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gl_common.h"
#include "gfx_common.h"
#include "gfx_context.h"
#include "../compat/strl.h"

#ifdef HAVE_CG
#include "shader_cg.h"
#endif

#ifdef HAVE_GLSL
#include "shader_glsl.h"
#endif

#include "shader_common.h"

/* Used for the last pass when rendering to the back buffer. */
static const GLfloat vertexes_flipped[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

/* Used when rendering to an FBO.
 * Texture coords have to be aligned 
 * with vertex coordinates. */
static const GLfloat vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat tex_coords[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat white_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};

static inline bool gl_query_extension(gl_t *gl, const char *ext)
{
   bool ret = false;

   if (gl->core_context)
   {
#ifdef GL_NUM_EXTENSIONS
      GLint i, exts;
      exts = 0;
      glGetIntegerv(GL_NUM_EXTENSIONS, &exts);
      for (i = 0; i < exts; i++)
      {
         const char *str = (const char*)glGetStringi(GL_EXTENSIONS, i);
         if (str && strstr(str, ext))
         {
            ret = true;
            break;
         }
      }
#endif
   }
   else
   {
      const char *str = (const char*)glGetString(GL_EXTENSIONS);
      ret = str && strstr(str, ext);
   }

   RARCH_LOG("Querying GL extension: %s => %s\n",
         ext, ret ? "exists" : "doesn't exist");
   return ret;
}

#ifdef HAVE_OVERLAY
static void gl_render_overlay(void *data);
static void gl_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y, float w, float h);
static void gl_overlay_tex_geom(void *data,
      unsigned image,
      float x, float y, float w, float h);
#endif

static inline void set_texture_coords(GLfloat *coords,
      GLfloat xamt, GLfloat yamt)
{
   coords[2] = xamt;
   coords[6] = xamt;
   coords[5] = yamt;
   coords[7] = yamt;
}

#if defined(HAVE_EGL) && defined(HAVE_OPENGLES2)
static bool check_eglimage_proc(void)
{
   return glEGLImageTargetTexture2DOES != NULL;
}
#endif

#ifdef HAVE_GL_SYNC
static bool check_sync_proc(gl_t *gl)
{
   if (!gl_query_extension(gl, "ARB_sync"))
      return false;

   return glFenceSync && glDeleteSync && glClientWaitSync;
}
#endif

#ifndef HAVE_OPENGLES
static bool init_vao(gl_t *gl)
{
   if (!gl->core_context && !gl_query_extension(gl, "ARB_vertex_array_object"))
      return false;

   if (!(glGenVertexArrays && glBindVertexArray && glDeleteVertexArrays))
      return false;

   glGenVertexArrays(1, &gl->vao);
   return true;
}
#endif

#ifdef HAVE_FBO
#if defined(HAVE_PSGL)
#define glGenFramebuffers glGenFramebuffersOES
#define glBindFramebuffer glBindFramebufferOES
#define glFramebufferTexture2D glFramebufferTexture2DOES
#define glCheckFramebufferStatus glCheckFramebufferStatusOES
#define glDeleteFramebuffers glDeleteFramebuffersOES
#define glGenRenderbuffers glGenRenderbuffersOES
#define glBindRenderbuffer glBindRenderbufferOES
#define glFramebufferRenderbuffer glFramebufferRenderbufferOES
#define glRenderbufferStorage glRenderbufferStorageOES
#define glDeleteRenderbuffers glDeleteRenderbuffersOES
#define check_fbo_proc(gl) (true)
#elif !defined(HAVE_OPENGLES2)
static bool check_fbo_proc(gl_t *gl)
{
   if (!gl->core_context && !gl_query_extension(gl, "ARB_framebuffer_object"))
      return false;

   return glGenFramebuffers && glBindFramebuffer && glFramebufferTexture2D && 
      glCheckFramebufferStatus && glDeleteFramebuffers &&
      glGenRenderbuffers && glBindRenderbuffer &&
      glFramebufferRenderbuffer && glRenderbufferStorage &&
      glDeleteRenderbuffers;
}
#else
#define check_fbo_proc(gl) (true)
#endif
#endif

/* Shaders */

static bool gl_shader_init(gl_t *gl)
{
   bool ret = false;
   const gl_shader_backend_t *backend = NULL;

   const char *shader_path = (g_settings.video.shader_enable && *g_settings.video.shader_path) ?
      g_settings.video.shader_path : NULL;

   enum rarch_shader_type type;

   if (!gl)
   {
      RARCH_ERR("Invalid GL instance passed.\n");
      return false;
   }

   type = gfx_shader_parse_type(shader_path,
      gl->core_context ? RARCH_SHADER_GLSL : DEFAULT_SHADER_TYPE);

   if (type == RARCH_SHADER_NONE)
   {
      RARCH_LOG("[GL]: Not loading any shader.\n");
      return true;
   }

   switch (type)
   {
#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         RARCH_LOG("[GL]: Using Cg shader backend.\n");
         backend = &gl_cg_backend;
         break;
#endif

#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         RARCH_LOG("[GL]: Using GLSL shader backend.\n");
         backend = &gl_glsl_backend;
         break;
#endif

      default:
         break;
   }

   if (!backend)
   {
      RARCH_ERR("[GL]: Didn't find valid shader backend. Continuing without shaders.\n");
      return true;
   }

#ifdef HAVE_GLSL
   if (gl->core_context && RARCH_SHADER_CG)
   {
      RARCH_ERR("[GL]: Cg cannot be used with core GL context. Falling back to GLSL.\n");
      backend = &gl_glsl_backend;
      shader_path = NULL;
   }
#endif

   gl->shader = backend;

   if (gl->shader && gl->shader->init)
   {
      ret = gl->shader->init(gl, shader_path);

      if (!ret)
      {
         RARCH_ERR("[GL]: Failed to init shader, falling back to stock.\n");
         ret = gl->shader->init(gl, NULL);
      }
   }

   return ret;
}

static void gl_shader_deinit(gl_t *gl)
{
   if (gl->shader)
      gl->shader->deinit();
   gl->shader = NULL;
}

#ifndef NO_GL_FF_VERTEX
static void gl_disable_client_arrays(gl_t *gl)
{
   if (!gl || gl->core_context)
      return;

   glClientActiveTexture(GL_TEXTURE1);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glClientActiveTexture(GL_TEXTURE0);
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}
#endif

void gl_shader_set_coords(gl_t *gl, const struct gl_coords *coords, const math_matrix *mat)
{
   bool ret_coords = (gl->shader) ? gl->shader->set_coords(coords) : false;
   bool ret_mvp    = (gl->shader) ? gl->shader->set_mvp(gl, mat)   : false;

   (void)ret_coords;
   (void)ret_mvp;

#ifndef NO_GL_FF_VERTEX
   if (!ret_coords)
   {
      /* Fall back to FF-style if needed and possible. */
      glClientActiveTexture(GL_TEXTURE1);
      glTexCoordPointer(2, GL_FLOAT, 0, coords->lut_tex_coord);
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);

      glClientActiveTexture(GL_TEXTURE0);
      glVertexPointer(2, GL_FLOAT, 0, coords->vertex);
      glEnableClientState(GL_VERTEX_ARRAY);

      glColorPointer(4, GL_FLOAT, 0, coords->color);
      glEnableClientState(GL_COLOR_ARRAY);

      glTexCoordPointer(2, GL_FLOAT, 0, coords->tex_coord);
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   }
#endif

#ifndef NO_GL_FF_MATRIX
   if (!ret_mvp)
   {
      /* Fall back to FF-style if needed and possible. */
      glMatrixMode(GL_PROJECTION);
      glLoadMatrixf(mat->data);
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
   }
#endif
}

#define gl_shader_num(gl) ((gl->shader) ? gl->shader->num_shaders() : 0)
#define gl_shader_filter_type(gl, index, smooth) ((gl->shader) ? gl->shader->filter_type(index, smooth) : false)
#define gl_shader_wrap_type(gl, index) ((gl->shader) ? gl->shader->wrap_type(index) : RARCH_WRAP_BORDER)
#define gl_shader_mipmap_input(gl, index) ((gl->shader) ? gl->shader->mipmap_input(index) : false)

#ifdef IOS
/* There is no default frame buffer on iOS. */
void apple_bind_game_view_fbo(void);
#define gl_bind_backbuffer() apple_bind_game_view_fbo()
#else
#define gl_bind_backbuffer() glBindFramebuffer(RARCH_GL_FRAMEBUFFER, 0)
#endif

static inline GLenum min_filter_to_mag(GLenum type)
{
   switch (type)
   {
      case GL_LINEAR_MIPMAP_LINEAR:
         return GL_LINEAR;
      case GL_NEAREST_MIPMAP_NEAREST:
         return GL_NEAREST;
      default:
         return type;
   }
}

#ifdef HAVE_FBO
static void gl_shader_scale(gl_t *gl, unsigned index, struct gfx_fbo_scale *scale)
{
   scale->valid = false;
   if (gl->shader)
      gl->shader->shader_scale(index, scale);
}

/* Compute FBO geometry.
 * When width/height changes or window sizes change, 
 * we have to recalculate geometry of our FBO. */

static void gl_compute_fbo_geometry(gl_t *gl, unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
{
   int i;
   bool size_modified = false;
   GLint max_size = 0;
   unsigned last_width = width;
   unsigned last_height = height;
   unsigned last_max_width = gl->tex_w;
   unsigned last_max_height = gl->tex_h;

   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);

   /* Calculate viewports for FBOs. */
   for (i = 0; i < gl->fbo_pass; i++)
   {
      switch (gl->fbo_scale[i].type_x)
      {
         case RARCH_SCALE_INPUT:
            gl->fbo_rect[i].img_width = last_width * gl->fbo_scale[i].scale_x;
            gl->fbo_rect[i].max_img_width = last_max_width * gl->fbo_scale[i].scale_x;
            break;

         case RARCH_SCALE_ABSOLUTE:
            gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].abs_x;
            break;

         case RARCH_SCALE_VIEWPORT:
            gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].scale_x * vp_width;
            break;
      }

      switch (gl->fbo_scale[i].type_y)
      {
         case RARCH_SCALE_INPUT:
            gl->fbo_rect[i].img_height = last_height * gl->fbo_scale[i].scale_y;
            gl->fbo_rect[i].max_img_height = last_max_height * gl->fbo_scale[i].scale_y;
            break;

         case RARCH_SCALE_ABSOLUTE:
            gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].abs_y;
            break;

         case RARCH_SCALE_VIEWPORT:
            gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].scale_y * vp_height;
            break;
      }

      if (gl->fbo_rect[i].img_width > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].img_width = max_size;
      }

      if (gl->fbo_rect[i].img_height > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].img_height = max_size;
      }

      if (gl->fbo_rect[i].max_img_width > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].max_img_width = max_size;
      }

      if (gl->fbo_rect[i].max_img_height > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].max_img_height = max_size;
      }

      if (size_modified)
         RARCH_WARN("FBO textures exceeded maximum size of GPU (%ux%u). Resizing to fit.\n", max_size, max_size);

      last_width = gl->fbo_rect[i].img_width;
      last_height = gl->fbo_rect[i].img_height;
      last_max_width = gl->fbo_rect[i].max_img_width;
      last_max_height = gl->fbo_rect[i].max_img_height;
   }
}


static void gl_create_fbo_textures(gl_t *gl)
{
   int i;
   if (!gl)
      return;

   glGenTextures(gl->fbo_pass, gl->fbo_texture);

   GLuint base_filt     = g_settings.video.smooth ? GL_LINEAR : GL_NEAREST;
   GLuint base_mip_filt = g_settings.video.smooth ? 
      GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;

   for (i = 0; i < gl->fbo_pass; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);

      bool mipmapped = gl_shader_mipmap_input(gl, i + 2);

      GLenum min_filter = mipmapped ? base_mip_filt : base_filt;
      bool smooth = false;
      if (gl_shader_filter_type(gl, i + 2, &smooth))
         min_filter = mipmapped ? (smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) : (smooth ? GL_LINEAR : GL_NEAREST);

      GLenum mag_filter = min_filter_to_mag(min_filter);

      enum gfx_wrap_type wrap = gl_shader_wrap_type(gl, i + 2);
      GLenum wrap_enum = gl_wrap_type_to_enum(wrap);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_enum);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_enum);

      bool fp_fbo = gl->fbo_scale[i].fp_fbo;
      bool srgb_fbo = gl->fbo_scale[i].srgb_fbo;

      if (fp_fbo)
      {
         if (!gl->has_fp_fbo)
            RARCH_ERR("[GL]: Floating-point FBO was requested, but is not supported. Falling back to UNORM. Result may band/clip/etc.!\n");
      }
      else if (srgb_fbo)
      {
         if (!gl->has_srgb_fbo)
            RARCH_ERR("[GL]: sRGB FBO was requested, but it is not supported. Falling back to UNORM. Result may have banding!\n");
      }

      if (g_settings.video.force_srgb_disable)
         srgb_fbo = false;

   #ifndef HAVE_OPENGLES2
      if (fp_fbo && gl->has_fp_fbo)
      {
         RARCH_LOG("[GL]: FBO pass #%d is floating-point.\n", i);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
               gl->fbo_rect[i].width, gl->fbo_rect[i].height,
               0, GL_RGBA, GL_FLOAT, NULL);
      }
      else
   #endif
      {
      #ifndef HAVE_OPENGLES
         if (srgb_fbo && gl->has_srgb_fbo)
         {
            RARCH_LOG("[GL]: FBO pass #%d is sRGB.\n", i);
         #ifdef HAVE_OPENGLES2
            /* EXT defines are same as core GLES3 defines, 
             * but GLES3 variant requires different arguments. */
            glTexImage2D(GL_TEXTURE_2D,
                  0, GL_SRGB_ALPHA_EXT,
                  gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
                  gl->has_srgb_fbo_gles3 ? GL_RGBA : GL_SRGB_ALPHA_EXT,
                  GL_UNSIGNED_BYTE, NULL);
         #else
            glTexImage2D(GL_TEXTURE_2D,
                  0, GL_SRGB8_ALPHA8,
                  gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, NULL);
         #endif
         }
         else
      #endif
         {
         #ifdef HAVE_OPENGLES2
            glTexImage2D(GL_TEXTURE_2D,
                  0, GL_RGBA,
                  gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, NULL);
         #else
            /* Avoid potential performance 
             * reductions on particular platforms. */
            glTexImage2D(GL_TEXTURE_2D,
                  0, RARCH_GL_INTERNAL_FORMAT32,
                  gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
                  RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, NULL);
         #endif
         }
      }
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

static bool gl_create_fbo_targets(gl_t *gl)
{
   int i;
   if (!gl)
      return false;

   glBindTexture(GL_TEXTURE_2D, 0);
   glGenFramebuffers(gl->fbo_pass, gl->fbo);
   for (i = 0; i < gl->fbo_pass; i++)
   {
      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo[i]);
      glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

      GLenum status = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);
      if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
         goto error;
   }

   return true;

error:
   glDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   RARCH_ERR("Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
   return false;
}

void gl_deinit_fbo(gl_t *gl)
{
   if (!gl->fbo_inited)
      return;

   glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
   glDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   memset(gl->fbo_texture, 0, sizeof(gl->fbo_texture));
   memset(gl->fbo, 0, sizeof(gl->fbo));
   gl->fbo_inited = false;
   gl->fbo_pass = 0;
}

/* Set up render to texture. */

void gl_init_fbo(gl_t *gl, unsigned width, unsigned height)
{
   int i;

   if (!gl || gl_shader_num(gl) == 0)
      return;

   struct gfx_fbo_scale scale, scale_last;
   gl_shader_scale(gl, 1, &scale);
   gl_shader_scale(gl, gl_shader_num(gl), &scale_last);

   /* we always want FBO to be at least initialized on startup for consoles */
   if (gl_shader_num(gl) == 1 && !scale.valid)
      return;

   if (!check_fbo_proc(gl))
   {
      RARCH_ERR("Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
      return;
   }

   gl->fbo_pass = gl_shader_num(gl) - 1;
   if (scale_last.valid)
      gl->fbo_pass++;

   if (!scale.valid)
   {
      scale.scale_x = 1.0f;
      scale.scale_y = 1.0f; 
      scale.type_x  = scale.type_y = RARCH_SCALE_INPUT;
      scale.valid   = true;
   }

   gl->fbo_scale[0] = scale;

   for (i = 1; i < gl->fbo_pass; i++)
   {
      gl_shader_scale(gl, i + 1, &gl->fbo_scale[i]);

      if (!gl->fbo_scale[i].valid)
      {
         gl->fbo_scale[i].scale_x = gl->fbo_scale[i].scale_y = 1.0f;
         gl->fbo_scale[i].type_x  = gl->fbo_scale[i].type_y  = 
            RARCH_SCALE_INPUT;
         gl->fbo_scale[i].valid   = true;
      }
   }

   gl_compute_fbo_geometry(gl, width, height, gl->win_width, gl->win_height);

   for (i = 0; i < gl->fbo_pass; i++)
   {
      gl->fbo_rect[i].width  = next_pow2(gl->fbo_rect[i].img_width);
      gl->fbo_rect[i].height = next_pow2(gl->fbo_rect[i].img_height);
      RARCH_LOG("Creating FBO %d @ %ux%u\n", i,
            gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   gl_create_fbo_textures(gl);
   if (!gl_create_fbo_targets(gl))
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      RARCH_ERR("Failed to create FBO targets. Will continue without FBO.\n");
      return;
   }

   gl->fbo_inited = true;
}

#ifndef HAVE_GCMGL
static void gl_deinit_hw_render(gl_t *gl)
{
   if (!gl)
      return;

   context_bind_hw_render(gl, true);

   if (gl->hw_render_fbo_init)
      glDeleteFramebuffers(gl->textures, gl->hw_render_fbo);
   if (gl->hw_render_depth_init)
      glDeleteRenderbuffers(gl->textures, gl->hw_render_depth);
   gl->hw_render_fbo_init = false;

   context_bind_hw_render(gl, false);
}

static bool gl_init_hw_render(gl_t *gl, unsigned width, unsigned height)
{
   unsigned i;
   bool depth = false, stencil = false;
   GLint max_fbo_size = 0, max_renderbuffer_size = 0;

   /* We can only share texture objects through contexts.
    * FBOs are "abstract" objects and are not shared. */
   context_bind_hw_render(gl, true);

   RARCH_LOG("[GL]: Initializing HW render (%u x %u).\n", width, height);
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_fbo_size);
   glGetIntegerv(RARCH_GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size);
   RARCH_LOG("[GL]: Max texture size: %d px, renderbuffer size: %u px.\n",
         max_fbo_size, max_renderbuffer_size);

   if (!check_fbo_proc(gl))
      return false;

   glBindTexture(GL_TEXTURE_2D, 0);
   glGenFramebuffers(gl->textures, gl->hw_render_fbo);

   depth = g_extern.system.hw_render_callback.depth;
   stencil = g_extern.system.hw_render_callback.stencil;

#ifdef HAVE_OPENGLES2
   if (stencil && !gl_query_extension(gl, "OES_packed_depth_stencil"))
      return false;
#endif

   if (depth)
   {
      glGenRenderbuffers(gl->textures, gl->hw_render_depth);
      gl->hw_render_depth_init = true;
   }

   for (i = 0; i < gl->textures; i++)
   {
      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->hw_render_fbo[i]);
      glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->texture[i], 0);

      if (depth)
      {
         glBindRenderbuffer(RARCH_GL_RENDERBUFFER, gl->hw_render_depth[i]);
         glRenderbufferStorage(RARCH_GL_RENDERBUFFER,
               stencil ? RARCH_GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT16,
               width, height);
         glBindRenderbuffer(RARCH_GL_RENDERBUFFER, 0);

         if (stencil)
         {
#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES1) || defined(OSX_PPC)
            /* GLES2 is a bit weird, as always.
             * There's no GL_DEPTH_STENCIL_ATTACHMENT like in desktop GL. */
            glFramebufferRenderbuffer(RARCH_GL_FRAMEBUFFER,
                  RARCH_GL_DEPTH_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER, gl->hw_render_depth[i]);
            glFramebufferRenderbuffer(RARCH_GL_FRAMEBUFFER,
                  RARCH_GL_STENCIL_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER, gl->hw_render_depth[i]);
#else
            /* We use ARB FBO extensions, no need to check. */
            glFramebufferRenderbuffer(RARCH_GL_FRAMEBUFFER,
                  GL_DEPTH_STENCIL_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER, gl->hw_render_depth[i]);
#endif
         }
         else
         {
            glFramebufferRenderbuffer(RARCH_GL_FRAMEBUFFER,
                  RARCH_GL_DEPTH_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER, gl->hw_render_depth[i]);
         }
      }

      GLenum status = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);
      if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
      {
         RARCH_ERR("[GL]: Failed to create HW render FBO #%u, error: 0x%u.\n",
               i, (unsigned)status);
         return false;
      }
   }

   gl_bind_backbuffer();
   gl->hw_render_fbo_init = true;

   context_bind_hw_render(gl, false);
   return true;
}
#endif
#endif

void gl_set_projection(gl_t *gl, struct gl_ortho *ortho, bool allow_rotate)
{
   math_matrix rot;

   /* Calculate projection. */
   matrix_ortho(&gl->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      gl->mvp = gl->mvp_no_rot;
      return;
   }

   matrix_rotate_z(&rot, M_PI * gl->rotation / 180.0f);
   matrix_multiply(&gl->mvp, &rot, &gl->mvp_no_rot);
}

void gl_set_viewport(gl_t *gl, unsigned width,
      unsigned height, bool force_full, bool allow_rotate)
{
   int x = 0, y = 0;
   float device_aspect = (float)width / height;
   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (gl->ctx_driver->translate_aspect)
      device_aspect = context_translate_aspect_func(gl, width, height);

   if (g_settings.video.scale_integer && !force_full)
   {
      gfx_scale_integer(&gl->vp, width, height,
            g_extern.system.aspect_ratio, gl->keep_aspect);
      width  = gl->vp.width;
      height = gl->vp.height;
   }
   else if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = g_extern.system.aspect_ratio;
      float delta;

#if defined(HAVE_MENU)
      if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct rarch_viewport *custom =
            (const struct rarch_viewport*)
            &g_extern.console.screen.viewports.custom_vp;

         /* GL has bottom-left origin viewport. */
         x      = custom->x;
         y      = gl->win_height - custom->y - custom->height;
         width  = custom->width;
         height = custom->height;
      }
      else
#endif
      {
         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect 
             * ratio are sufficiently equal (floating point stuff), 
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            x     = (int)roundf(width * (0.5f - delta));
            width = (unsigned)roundf(2.0f * width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            y      = (int)roundf(height * (0.5f - delta));
            height = (unsigned)roundf(2.0f * height * delta);
         }
      }

      gl->vp.x      = x;
      gl->vp.y      = y;
      gl->vp.width  = width;
      gl->vp.height = height;
   }
   else
   {
      gl->vp.x = gl->vp.y = 0;
      gl->vp.width = width;
      gl->vp.height = height;
   }

#if defined(RARCH_MOBILE)
   /* In portrait mode, we want viewport to gravitate to top of screen. */
   if (device_aspect < 1.0f)
      gl->vp.y *= 2;
#endif

   glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   gl_set_projection(gl, &ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      gl->vp_out_width  = width;
      gl->vp_out_height = height;
   }

#if 0
   RARCH_LOG("Setting viewport @ %ux%u\n", width, height);
#endif
}

static void gl_set_rotation(void *data, unsigned rotation)
{
   gl_t *gl = (gl_t*)data;
   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (!gl)
      return;

   gl->rotation = 90 * rotation;
   gl_set_projection(gl, &ortho, true);
}

#ifdef HAVE_FBO
static inline void gl_start_frame_fbo(gl_t *gl)
{
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo[0]);

   gl_set_viewport(gl, gl->fbo_rect[0].img_width,
         gl->fbo_rect[0].img_height, true, false);

   /* Need to preserve the "flipped" state when in FBO 
    * as well to have consistent texture coordinates.
    *
    * We will "flip" it in place on last pass. */
   gl->coords.vertex = vertexes;

#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (gl->has_srgb_fbo)
      glEnable(GL_FRAMEBUFFER_SRGB);
#endif
}

/* On resize, we might have to recreate our FBOs 
 * due to "Viewport" scale, and set a new viewport. */

static void gl_check_fbo_dimensions(gl_t *gl)
{
   int i;

   /* Check if we have to recreate our FBO textures. */
   for (i = 0; i < gl->fbo_pass; i++)
   {
      if (gl->fbo_rect[i].max_img_width > gl->fbo_rect[i].width ||
            gl->fbo_rect[i].max_img_height > gl->fbo_rect[i].height)
      {
         /* Check proactively since we might suddently 
          * get sizes of tex_w width or tex_h height. */
         unsigned img_width = gl->fbo_rect[i].max_img_width;
         unsigned img_height = gl->fbo_rect[i].max_img_height;
         unsigned max = img_width > img_height ? img_width : img_height;
         unsigned pow2_size = next_pow2(max);

         gl->fbo_rect[i].width = gl->fbo_rect[i].height = pow2_size;

         glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo[i]);
         glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);

         glTexImage2D(GL_TEXTURE_2D,
               0, RARCH_GL_INTERNAL_FORMAT32,
               gl->fbo_rect[i].width,
               gl->fbo_rect[i].height,
               0, RARCH_GL_TEXTURE_TYPE32,
               RARCH_GL_FORMAT32, NULL);

         glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER,
               RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
               gl->fbo_texture[i], 0);

         GLenum status = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);
         if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
            RARCH_WARN("Failed to reinit FBO texture.\n");

         RARCH_LOG("Recreating FBO texture #%d: %ux%u\n",
               i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
      }
   }
}

static void gl_frame_fbo(gl_t *gl, const struct gl_tex_info *tex_info)
{
   const struct gl_fbo_rect *prev_rect;
   const struct gl_fbo_rect *rect;
   struct gl_tex_info *fbo_info;
   struct gl_tex_info fbo_tex_info[MAX_SHADERS];
   int i;
   GLfloat xamt, yamt;
   unsigned fbo_tex_info_cnt = 0;
   GLfloat fbo_tex_coords[8] = {0.0f};

   /* Render the rest of our passes. */
   gl->coords.tex_coord = fbo_tex_coords;

   /* Calculate viewports, texture coordinates etc,
    * and render all passes from FBOs, to another FBO. */
   for (i = 1; i < gl->fbo_pass; i++)
   {
      prev_rect = &gl->fbo_rect[i - 1];
      rect      = &gl->fbo_rect[i];
      fbo_info  = &fbo_tex_info[i - 1];

      xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
      yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

      set_texture_coords(fbo_tex_coords, xamt, yamt);

      fbo_info->tex = gl->fbo_texture[i - 1];
      fbo_info->input_size[0] = prev_rect->img_width;
      fbo_info->input_size[1] = prev_rect->img_height;
      fbo_info->tex_size[0] = prev_rect->width;
      fbo_info->tex_size[1] = prev_rect->height;
      memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
      fbo_tex_info_cnt++;

      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo[i]);

      if (gl->shader)
         gl->shader->use(gl, i + 1);
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

#ifndef HAVE_GCMGL
      if (gl_shader_mipmap_input(gl, i + 1))
         glGenerateMipmap(GL_TEXTURE_2D);
#endif

      glClear(GL_COLOR_BUFFER_BIT);

      /* Render to FBO with certain size. */
      gl_set_viewport(gl, rect->img_width, rect->img_height, true, false);
      if (gl->shader)
         gl->shader->set_params(gl, prev_rect->img_width, prev_rect->img_height, 
            prev_rect->width, prev_rect->height, 
            gl->vp.width, gl->vp.height, g_extern.frame_count, 
            tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

      gl->coords.vertices = 4;
      gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }

#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (gl->has_srgb_fbo)
      glDisable(GL_FRAMEBUFFER_SRGB);
#endif

   /* Render our last FBO texture directly to screen. */
   prev_rect = &gl->fbo_rect[gl->fbo_pass - 1];
   xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
   yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

   set_texture_coords(fbo_tex_coords, xamt, yamt);

   /* Push final FBO to list. */
   fbo_info = &fbo_tex_info[gl->fbo_pass - 1];
   fbo_info->tex = gl->fbo_texture[gl->fbo_pass - 1];
   fbo_info->input_size[0] = prev_rect->img_width;
   fbo_info->input_size[1] = prev_rect->img_height;
   fbo_info->tex_size[0] = prev_rect->width;
   fbo_info->tex_size[1] = prev_rect->height;
   memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
   fbo_tex_info_cnt++;

   /* Render our FBO texture to back buffer. */
   gl_bind_backbuffer();
   if (gl->shader)
      gl->shader->use(gl, gl->fbo_pass + 1);

   glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

#ifndef HAVE_GCMGL
   if (gl_shader_mipmap_input(gl, gl->fbo_pass + 1))
      glGenerateMipmap(GL_TEXTURE_2D);
#endif

   glClear(GL_COLOR_BUFFER_BIT);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);

   if (gl->shader)
      gl->shader->set_params(gl, prev_rect->img_width, prev_rect->img_height, 
         prev_rect->width, prev_rect->height, 
         gl->vp.width, gl->vp.height, g_extern.frame_count, 
         tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

   gl->coords.vertex = gl->vertex_ptr;

   gl->coords.vertices = 4;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_info.coord;
}
#endif

static void gl_update_input_size(gl_t *gl, unsigned width,
      unsigned height, unsigned pitch, bool clear)
{
   bool set_coords = false;


   if ((width != gl->last_width[gl->tex_index] || 
            height != gl->last_height[gl->tex_index]) && gl->empty_buf)
   {
      /* Resolution change. Need to clear out texture. */

      gl->last_width[gl->tex_index] = width;
      gl->last_height[gl->tex_index] = height;

      if (clear)
      {
         glPixelStorei(GL_UNPACK_ALIGNMENT,
               get_alignment(width * sizeof(uint32_t)));
#if defined(HAVE_PSGL)
         glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
               gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size,
               gl->tex_w * gl->tex_h * gl->base_size,
               gl->empty_buf);
#else
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, gl->tex_w, gl->tex_h, gl->texture_type,
               gl->texture_fmt, gl->empty_buf);
#endif
      }

      set_coords = true;
   }
   else if ((width != 
         gl->last_width[(gl->tex_index + gl->textures - 1) % gl->textures]) ||
         (height != 
         gl->last_height[(gl->tex_index + gl->textures - 1) % gl->textures]))
   {
      /* We might have used different texture coordinates 
       * last frame. Edge case if resolution changes very rapidly. */
      set_coords = true;
   }

   if (set_coords)
   {
      GLfloat xamt = (GLfloat)width  / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;
      set_texture_coords(gl->tex_info.coord, xamt, yamt);
   }
}

/* It is *much* faster (order of magnitude on my setup)
 * to use a custom SIMD-optimized conversion routine 
 * than letting GL do it. */
#if !defined(HAVE_PSGL) && !defined(HAVE_OPENGLES2)
static inline void gl_convert_frame_rgb16_32(gl_t *gl, void *output,
      const void *input, int width, int height, int in_pitch)
{
   if (width != gl->scaler.in_width || height != gl->scaler.in_height)
   {
      gl->scaler.in_width    = width;
      gl->scaler.in_height   = height;
      gl->scaler.out_width   = width;
      gl->scaler.out_height  = height;
      gl->scaler.in_fmt      = SCALER_FMT_RGB565;
      gl->scaler.out_fmt     = SCALER_FMT_ARGB8888;
      gl->scaler.scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(&gl->scaler);
   }

   gl->scaler.in_stride  = in_pitch;
   gl->scaler.out_stride = width * sizeof(uint32_t);
   scaler_ctx_scale(&gl->scaler, output, input);
}
#endif

#ifdef HAVE_OPENGLES2
static inline void gl_convert_frame_argb8888_abgr8888(gl_t *gl,
      void *output, const void *input,
      int width, int height, int in_pitch)
{
   if (width != gl->scaler.in_width || height != gl->scaler.in_height)
   {
      gl->scaler.in_width    = width;
      gl->scaler.in_height   = height;
      gl->scaler.out_width   = width;
      gl->scaler.out_height  = height;
      gl->scaler.in_fmt      = SCALER_FMT_ARGB8888;
      gl->scaler.out_fmt     = SCALER_FMT_ABGR8888;
      gl->scaler.scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(&gl->scaler);
   }

   gl->scaler.in_stride  = in_pitch;
   gl->scaler.out_stride = width * sizeof(uint32_t);
   scaler_ctx_scale(&gl->scaler, output, input);
}
#endif

static void gl_init_textures_data(gl_t *gl)
{
   unsigned i;
   for (i = 0; i < gl->textures; i++)
   {
      gl->last_width[i]  = gl->tex_w;
      gl->last_height[i] = gl->tex_h;
   }

   for (i = 0; i < gl->textures; i++)
   {
      gl->prev_info[i].tex           = gl->texture[0];
      gl->prev_info[i].input_size[0] = gl->tex_w;
      gl->prev_info[i].tex_size[0]   = gl->tex_w;
      gl->prev_info[i].input_size[1] = gl->tex_h;
      gl->prev_info[i].tex_size[1]   = gl->tex_h;
      memcpy(gl->prev_info[i].coord, tex_coords, sizeof(tex_coords)); 
   }
}

static void gl_init_textures(gl_t *gl, const video_info_t *video)
{
   unsigned i;
#if defined(HAVE_EGL) && defined(HAVE_OPENGLES2)
   // Use regular textures if we use HW render.
   gl->egl_images = !gl->hw_render_use && check_eglimage_proc() &&
      gl->ctx_driver->init_egl_image_buffer
      && context_init_egl_image_buffer_func(gl, video);
#else
   (void)video;
#endif

#ifdef HAVE_PSGL
   if (!gl->pbo)
      glGenBuffers(1, &gl->pbo);

   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
         gl->tex_w * gl->tex_h * gl->base_size * gl->textures, NULL, GL_STREAM_DRAW);
#endif

   GLenum internal_fmt = gl->internal_fmt;
#ifndef HAVE_PSGL
   GLenum texture_type = gl->texture_type;
   GLenum texture_fmt  = gl->texture_fmt;
#endif

#ifdef HAVE_OPENGLES2
   /* GLES is picky about which format we use here.
    * Without extensions, we can *only* render to 16-bit FBOs. */

   if (gl->hw_render_use && gl->base_size == sizeof(uint32_t))
   {
      bool support_argb = gl_query_extension(gl, "OES_rgb8_rgba8")
         || gl_query_extension(gl, "ARM_argb8");

      if (support_argb)
      {
         internal_fmt = GL_RGBA;
         texture_type = GL_RGBA;
         texture_fmt  = GL_UNSIGNED_BYTE;
      }
      else
      {
         RARCH_WARN("[GL]: 32-bit FBO not supported. Falling back to 16-bit.\n");
         internal_fmt = GL_RGB;
         texture_type = GL_RGB;
         texture_fmt  = GL_UNSIGNED_SHORT_5_6_5;
      }
   }
#endif

   glGenTextures(gl->textures, gl->texture);

   for (i = 0; i < gl->textures; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->wrap_mode);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->wrap_mode);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_mag_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_min_filter);

#ifdef HAVE_PSGL
      glTextureReferenceSCE(GL_TEXTURE_2D, 1,
            gl->tex_w, gl->tex_h, 0, 
            internal_fmt,
            gl->tex_w * gl->base_size,
            gl->tex_w * gl->tex_h * i * gl->base_size);
#else
      if (!gl->egl_images)
      {
         glTexImage2D(GL_TEXTURE_2D,
               0, internal_fmt, gl->tex_w, gl->tex_h, 0, texture_type,
               texture_fmt, gl->empty_buf ? gl->empty_buf : NULL);
      }
#endif
   }
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static inline void gl_copy_frame(gl_t *gl, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   RARCH_PERFORMANCE_INIT(copy_frame);
   RARCH_PERFORMANCE_START(copy_frame);
#if defined(HAVE_OPENGLES2)
#if defined(HAVE_EGL)
   if (gl->egl_images)
   {
      EGLImageKHR img = 0;
      bool new_egl = context_write_egl_image_func(gl,
            frame, width, height, pitch, (gl->base_size == 4),
            gl->tex_index, &img);

      if (img == EGL_NO_IMAGE_KHR)
      {
         RARCH_ERR("[GL]: Failed to create EGL image.\n");
         return;
      }

      if (new_egl)
         glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);
   }
   else
#endif
   {
      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(width * gl->base_size));

      /* Fallback for GLES devices without GL_BGRA_EXT. */
      if (gl->base_size == 4 && driver.gfx_use_rgba)
      {
         gl_convert_frame_argb8888_abgr8888(gl, gl->conv_buffer,
               frame, width, height, pitch);
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, gl->conv_buffer);
      }
      else if (gl->support_unpack_row_length)
      {
         glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, frame);

         glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      }
      else
      {
         /* No GL_UNPACK_ROW_LENGTH. */

         const GLvoid *data_buf = frame;
         unsigned pitch_width = pitch / gl->base_size;

         if (width != pitch_width)
         {
            /* Slow path - conv_buffer is preallocated 
             * just in case we hit this path. */

            unsigned h;
            const unsigned line_bytes = width * gl->base_size;
            uint8_t *dst = (uint8_t*)gl->conv_buffer;
            const uint8_t *src = (const uint8_t*)frame;

            for (h = 0; h < height; h++, src += pitch, dst += line_bytes)
               memcpy(dst, src, line_bytes);

            data_buf = gl->conv_buffer;
         }

         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, data_buf);         
      }
   }
#elif defined(HAVE_PSGL)
   unsigned h;
   size_t buffer_addr        = gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size;
   size_t buffer_stride      = gl->tex_w * gl->base_size;
   const uint8_t *frame_copy = frame;
   size_t frame_copy_size    = width * gl->base_size;

   uint8_t *buffer = (uint8_t*)glMapBuffer(
         GL_TEXTURE_REFERENCE_BUFFER_SCE, GL_READ_WRITE) + buffer_addr;
   for (h = 0; h < height; h++, buffer += buffer_stride, frame_copy += pitch)
      memcpy(buffer, frame_copy, frame_copy_size);

   glUnmapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE);
#else
   const GLvoid *data_buf = frame;
   glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(pitch));

   if (gl->base_size == 2 && !gl->have_es2_compat)
   {
      /* Convert to 32-bit textures on desktop GL. */
      gl_convert_frame_rgb16_32(gl, gl->conv_buffer,
            frame, width, height, pitch);
      data_buf = gl->conv_buffer;
   }
   else
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);

   glTexSubImage2D(GL_TEXTURE_2D,
         0, 0, 0, width, height, gl->texture_type,
         gl->texture_fmt, data_buf);

   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
   RARCH_PERFORMANCE_STOP(copy_frame);
}

static inline void gl_set_prev_texture(gl_t *gl,
      const struct gl_tex_info *tex_info)
{
   memmove(gl->prev_info + 1, gl->prev_info,
         sizeof(*tex_info) * (gl->textures - 1));
   memcpy(&gl->prev_info[0], tex_info,
         sizeof(*tex_info));
}

static inline void gl_set_shader_viewport(gl_t *gl, unsigned shader)
{
   if (gl->shader)
      gl->shader->use(gl, shader);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
}

#if defined(HAVE_GL_ASYNC_READBACK) && defined(HAVE_MENU)
static void gl_pbo_async_readback(gl_t *gl)
{
   glBindBuffer(GL_PIXEL_PACK_BUFFER,
         gl->pbo_readback[gl->pbo_readback_index++]);
   gl->pbo_readback_index &= 3;

   /* 4 frames back, we can readback. */
   gl->pbo_readback_valid[gl->pbo_readback_index] = true;

   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glPixelStorei(GL_PACK_ALIGNMENT,
         get_alignment(gl->vp.width * sizeof(uint32_t)));

   /* Read asynchronously into PBO buffer. */
   RARCH_PERFORMANCE_INIT(async_readback);
   RARCH_PERFORMANCE_START(async_readback);
   glReadBuffer(GL_BACK);
#ifdef HAVE_OPENGLES3
   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#else
   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
#endif
   RARCH_PERFORMANCE_STOP(async_readback);

   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}
#endif

#if defined(HAVE_MENU)
static inline void gl_draw_texture(gl_t *gl)
{
   if (!gl->menu_texture)
      return;

   const GLfloat color[] = {
      1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
      1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
      1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
      1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
   };

   gl->coords.vertex = vertexes_flipped;
   gl->coords.tex_coord = tex_coords;
   gl->coords.color = color;
   glBindTexture(GL_TEXTURE_2D, gl->menu_texture);

   if (gl->shader)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
   gl->coords.vertices = 4;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);

   glEnable(GL_BLEND);

   if (gl->menu_texture_full_screen)
   {
      glViewport(0, 0, gl->win_width, gl->win_height);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   }
   else
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glDisable(GL_BLEND);

   gl->coords.vertex = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_info.coord;
   gl->coords.color = gl->white_color_ptr;
}
#endif

static bool gl_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   RARCH_PERFORMANCE_INIT(frame_run);
   RARCH_PERFORMANCE_START(frame_run);

   gl_t *gl = (gl_t*)data;

   if (!gl)
      return true;

   context_bind_hw_render(gl, false);

#ifndef HAVE_OPENGLES
   if (gl->core_context)
      glBindVertexArray(gl->vao);
#endif

   if (gl->shader)
      gl->shader->use(gl, 1);

#ifdef IOS
   /* Apparently the viewport is lost each frame, thanks Apple. */
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
#endif

#ifdef HAVE_FBO
   /* Render to texture in first pass. */
   if (gl->fbo_inited)
   {
      gl_compute_fbo_geometry(gl, width, height,
            gl->vp_out_width, gl->vp_out_height);
      gl_start_frame_fbo(gl);
   }
#endif

   if (gl->should_resize)
   {
      gl->should_resize = false;
      context_set_resize_func(gl, gl->win_width, gl->win_height);

#ifdef HAVE_FBO
      if (gl->fbo_inited)
      {
         gl_check_fbo_dimensions(gl);

         /* Go back to what we're supposed to do, 
          * render to FBO #0. */
         gl_start_frame_fbo(gl);
      }
      else
#endif
         gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
   }

   gl->tex_index = frame ?
      ((gl->tex_index + 1) % gl->textures) : (gl->tex_index);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   /* Can be NULL for frame dupe / NULL render. */
   if (frame) 
   {
#ifdef HAVE_FBO
      if (!gl->hw_render_fbo_init)
#endif
      {
         gl_update_input_size(gl, width, height, pitch, true);
         gl_copy_frame(gl, frame, width, height, pitch);
      }

#ifndef HAVE_GCMGL
      /* No point regenerating mipmaps 
       * if there are no new frames. */
      if (gl->tex_mipmap)
         glGenerateMipmap(GL_TEXTURE_2D);
#endif
   }

   /* Have to reset rendering state which libretro core 
    * could easily have overridden. */
#ifdef HAVE_FBO
   if (gl->hw_render_fbo_init)
   {
      gl_update_input_size(gl, width, height, pitch, false);
      if (!gl->fbo_inited)
      {
         gl_bind_backbuffer();
         gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
      }

#ifndef HAVE_OPENGLES
      if (!gl->core_context)
         glEnable(GL_TEXTURE_2D);
#endif
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_STENCIL_TEST);
      glDisable(GL_CULL_FACE);
      glDisable(GL_DITHER);
      glDisable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendEquation(GL_FUNC_ADD);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   }
#endif

   gl->tex_info.tex           = gl->texture[gl->tex_index];
   gl->tex_info.input_size[0] = width;
   gl->tex_info.input_size[1] = height;
   gl->tex_info.tex_size[0]   = gl->tex_w;
   gl->tex_info.tex_size[1]   = gl->tex_h;

   glClear(GL_COLOR_BUFFER_BIT);

   if (gl->shader && gl->shader->set_params)
      gl->shader->set_params(gl, width, height,
         gl->tex_w, gl->tex_h,
         gl->vp.width, gl->vp.height,
         g_extern.frame_count, 
         &gl->tex_info, gl->prev_info, NULL, 0);

   gl->coords.vertices = 4;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#ifdef HAVE_FBO
   if (gl->fbo_inited)
      gl_frame_fbo(gl, &gl->tex_info);
#endif

   gl_set_prev_texture(gl, &gl->tex_info);

#if defined(HAVE_MENU)
   if (g_extern.is_menu
         && driver.menu_ctx && driver.menu_ctx->frame)
      driver.menu_ctx->frame();

   if (gl->menu_texture_enable)
      gl_draw_texture(gl);
#endif

   if (msg && gl->font_driver && gl->font_handle)
      gl->font_driver->render_msg(gl->font_handle, msg, NULL);

#ifdef HAVE_OVERLAY
   if (gl->overlay_enable)
      gl_render_overlay(gl);
#endif

   context_update_window_title_func(gl);

   RARCH_PERFORMANCE_STOP(frame_run);

#ifdef HAVE_FBO
   /* Reset state which could easily mess up libretro core. */
   if (gl->hw_render_fbo_init)
   {
      if (gl->shader)
         gl->shader->use(gl, 0);
      glBindTexture(GL_TEXTURE_2D, 0);
#ifndef NO_GL_FF_VERTEX
      gl_disable_client_arrays(gl);
#endif
   }
#endif

#ifndef NO_GL_READ_PIXELS
   /* Screenshots. */
   if (gl->readback_buffer_screenshot)
   {
      glPixelStorei(GL_PACK_ALIGNMENT, 4);
#ifndef HAVE_OPENGLES
      glPixelStorei(GL_PACK_ROW_LENGTH, 0);
      glReadBuffer(GL_BACK);
#endif
      glReadPixels(gl->vp.x, gl->vp.y,
            gl->vp.width, gl->vp.height,
            GL_RGBA, GL_UNSIGNED_BYTE, gl->readback_buffer_screenshot);
   }
#ifdef HAVE_GL_ASYNC_READBACK
#ifdef HAVE_MENU
   /* Don't readback if we're in menu mode. */
   else if (gl->pbo_readback_enable && !gl->menu_texture_enable)
      gl_pbo_async_readback(gl);
#endif
#endif
#endif
   /* Disable BFI during fast forward, slow-motion,
    * and pause to prevent flicker. */
   if (g_settings.video.black_frame_insertion &&
         !driver.nonblock_state && !g_extern.is_slowmotion
         && !g_extern.is_paused)
   {
      context_swap_buffers_func(gl);
      glClear(GL_COLOR_BUFFER_BIT);
   }

   context_swap_buffers_func(gl);
   g_extern.frame_count++;

#ifdef HAVE_GL_SYNC
   if (g_settings.video.hard_sync && gl->have_sync)
   {
      RARCH_PERFORMANCE_INIT(gl_fence);
      RARCH_PERFORMANCE_START(gl_fence);
      glClear(GL_COLOR_BUFFER_BIT);
      gl->fences[gl->fence_count++] = 
         glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

      while (gl->fence_count > g_settings.video.hard_sync_frames)
      {
         glClientWaitSync(gl->fences[0],
               GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
         glDeleteSync(gl->fences[0]);

         gl->fence_count--;
         memmove(gl->fences, gl->fences + 1,
               gl->fence_count * sizeof(GLsync));
      }

      RARCH_PERFORMANCE_STOP(gl_fence);
   }
#endif

#ifndef HAVE_OPENGLES
   if (gl->core_context)
      glBindVertexArray(0);
#endif

   context_bind_hw_render(gl, true);

   return true;
}

#ifdef HAVE_OVERLAY
static void gl_free_overlay(gl_t *gl)
{
   if (!gl)
      return;

   glDeleteTextures(gl->overlays, gl->overlay_tex);

   free(gl->overlay_tex);
   free(gl->overlay_vertex_coord);
   free(gl->overlay_tex_coord);
   free(gl->overlay_color_coord);
   gl->overlay_tex = NULL;
   gl->overlay_vertex_coord = NULL;
   gl->overlay_tex_coord = NULL;
   gl->overlay_color_coord = NULL;
   gl->overlays = 0;
}
#endif

static void gl_free(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (!gl)
      return;

   context_bind_hw_render(gl, false);

#ifdef HAVE_GL_SYNC
   if (gl->have_sync)
   {
      unsigned i;
      for (i = 0; i < gl->fence_count; i++)
      {
         glClientWaitSync(gl->fences[i],
               GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
         glDeleteSync(gl->fences[i]);
      }
      gl->fence_count = 0;
   }
#endif

   if (gl->font_driver && gl->font_handle)
      gl->font_driver->free(gl->font_handle);
   gl_shader_deinit(gl);

#ifndef NO_GL_FF_VERTEX
   gl_disable_client_arrays(gl);
#endif

   glDeleteTextures(gl->textures, gl->texture);

#if defined(HAVE_MENU)
   if (gl->menu_texture)
      glDeleteTextures(1, &gl->menu_texture);
#endif

#ifdef HAVE_OVERLAY
   gl_free_overlay(gl);
#endif

#if defined(HAVE_PSGL)
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);
#endif

   scaler_ctx_gen_reset(&gl->scaler);

#ifdef HAVE_GL_ASYNC_READBACK
   if (gl->pbo_readback_enable)
   {
      glDeleteBuffers(4, gl->pbo_readback);
      scaler_ctx_gen_reset(&gl->pbo_readback_scaler);
   }
#endif

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
#ifndef HAVE_GCMGL
   gl_deinit_hw_render(gl);
#endif
#endif

#ifndef HAVE_OPENGLES
   if (gl->core_context)
   {
      glBindVertexArray(0);
      glDeleteVertexArrays(1, &gl->vao);
   }
#endif

   context_destroy_func(gl);

   free(gl->empty_buf);
   free(gl->conv_buffer);
   free(gl);
}

static void gl_set_nonblock_state(void *data, bool state)
{
   gl_t *gl = (gl_t*)data;

   if (!gl)
      return;

   RARCH_LOG("GL VSync => %s\n", state ? "off" : "on");

   context_bind_hw_render(gl, false);
   context_swap_interval_func(gl,
         state ? 0 : g_settings.video.swap_interval);
   context_bind_hw_render(gl, true);
}

static bool resolve_extensions(gl_t *gl)
{
#ifndef HAVE_OPENGLES
   gl->core_context = 
      (g_extern.system.hw_render_callback.context_type 
       == RETRO_HW_CONTEXT_OPENGL_CORE);
   if (gl->core_context)
      RARCH_LOG("[GL]: Using Core GL context.\n");
   if (gl->core_context &&
         !init_vao(gl))
   {
      RARCH_ERR("[GL]: Failed to init VAOs.\n");
      return false;
   }

   /* GL_RGB565 internal format support.
    * Even though ES2 support is claimed, the format 
    * is not supported on older ATI catalyst drivers.
    *
    * The speed gain from using GL_RGB565 is worth 
    * adding some workarounds for.
    */
   const char *vendor = (const char*)glGetString(GL_VENDOR);
   const char *renderer = (const char*)glGetString(GL_RENDERER);
   if (vendor && renderer && (strstr(vendor, "ATI") || strstr(renderer, "ATI")))
      RARCH_LOG("[GL]: ATI card detected, skipping check for GL_RGB565 support.\n");
   else
      gl->have_es2_compat = gl_query_extension(gl, "ARB_ES2_compatibility");
#endif

#ifdef HAVE_GL_SYNC
   gl->have_sync = check_sync_proc(gl);
   if (gl->have_sync && g_settings.video.hard_sync)
      RARCH_LOG("[GL]: Using ARB_sync to reduce latency.\n");
#endif

   driver.gfx_use_rgba = false;
#ifdef HAVE_OPENGLES2
   /* There are both APPLE and EXT variants. */
   if (gl_query_extension(gl, "BGRA8888"))
      RARCH_LOG("[GL]: BGRA8888 extension found for GLES.\n");
   else
   {
      driver.gfx_use_rgba = true;
      RARCH_WARN("[GL]: GLES implementation does not have BGRA8888 extension.\n"
                 "32-bit path will require conversion.\n");
   }

   bool gles3 = false;
   const char *version = (const char*)glGetString(GL_VERSION);
   unsigned gles_major = 0, gles_minor = 0;
   /* This format is mandated by GLES. */
   if (version && sscanf(version, "OpenGL ES %u.%u",
            &gles_major, &gles_minor) == 2 && gles_major >= 3)
   {
      RARCH_LOG("[GL]: GLES3 or newer detected. Auto-enabling some extensions.\n");
      gles3 = true;
   }

   /* GLES3 has unpack_subimage and sRGB in core. */

   gl->support_unpack_row_length = gles3;
   if (!gles3 && gl_query_extension(gl, "GL_EXT_unpack_subimage"))
   {
      RARCH_LOG("[GL]: Extension GL_EXT_unpack_subimage, can copy textures faster using UNPACK_ROW_LENGTH.\n");
      gl->support_unpack_row_length = true;
   }

   /* No extensions for float FBO currently. */
   gl->has_srgb_fbo = gles3 || gl_query_extension(gl, "EXT_sRGB");
   gl->has_srgb_fbo_gles3 = gles3;
#else
#ifdef HAVE_FBO
   /* Float FBO is core in 3.2. */
#ifdef HAVE_GCMGL
   gl->has_fp_fbo = false; /* FIXME - rewrite GL implementation */
#else
   gl->has_fp_fbo = gl->core_context || gl_query_extension(gl, "ARB_texture_float");
#endif
   gl->has_srgb_fbo = gl->core_context || 
      (gl_query_extension(gl, "EXT_texture_sRGB")
       && gl_query_extension(gl, "ARB_framebuffer_sRGB"));
#endif
#endif

   if (g_settings.video.force_srgb_disable)
      gl->has_srgb_fbo = false;

#ifdef GL_DEBUG
   /* Useful for debugging, but kinda obnoxious otherwise. */
   RARCH_LOG("[GL]: Supported extensions:\n");
   if (gl->core_context)
   {
#ifdef GL_NUM_EXTENSIONS
      GLint exts = 0;
      glGetIntegerv(GL_NUM_EXTENSIONS, &exts);
      for (GLint i = 0; i < exts; i++)
      {
         const char *ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
         if (ext)
            RARCH_LOG("\t%s\n", ext);
      }
#endif
   }
   else
   {
      const char *ext = (const char*)glGetString(GL_EXTENSIONS);
      if (ext)
      {
         struct string_list *list = string_split(ext, " ");
         for (size_t i = 0; i < list->size; i++)
            RARCH_LOG("\t%s\n", list->elems[i].data);
         string_list_free(list);
      }
   }
#endif

   return true;
}

static inline void gl_set_texture_fmts(gl_t *gl, bool rgb32)
{
   gl->internal_fmt = rgb32 ? RARCH_GL_INTERNAL_FORMAT32 : RARCH_GL_INTERNAL_FORMAT16;
   gl->texture_type = rgb32 ? RARCH_GL_TEXTURE_TYPE32 : RARCH_GL_TEXTURE_TYPE16;
   gl->texture_fmt  = rgb32 ? RARCH_GL_FORMAT32 : RARCH_GL_FORMAT16;
   gl->base_size    = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   if (driver.gfx_use_rgba && rgb32)
   {
      gl->internal_fmt = GL_RGBA;
      gl->texture_type = GL_RGBA;
   }

#ifndef HAVE_OPENGLES
   if (!rgb32 && gl->have_es2_compat)
   {
      RARCH_LOG("[GL]: Using GL_RGB565 for texture uploads.\n");
      gl->internal_fmt = RARCH_GL_INTERNAL_FORMAT16_565;
      gl->texture_type = RARCH_GL_TEXTURE_TYPE16_565;
      gl->texture_fmt = RARCH_GL_FORMAT16_565;
   }
#endif
}

#ifdef HAVE_GL_ASYNC_READBACK
static void gl_init_pbo_readback(gl_t *gl)
{
   unsigned i;
   /* Only bother with this if we're doing GPU recording.
    * Check g_extern.recording_enable and not 
    * driver.recording_data, because recording is 
    * not initialized yet.
    */
   gl->pbo_readback_enable = g_settings.video.gpu_record 
      && g_extern.recording_enable;
   if (!gl->pbo_readback_enable)
      return;

   RARCH_LOG("[GL]: Async PBO readback enabled.\n");

   glGenBuffers(4, gl->pbo_readback);
   for (i = 0; i < 4; i++)
   {
      glBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[i]);
      glBufferData(GL_PIXEL_PACK_BUFFER, gl->vp.width * 
            gl->vp.height * sizeof(uint32_t),
            NULL, GL_STREAM_READ);
   }
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

#ifndef HAVE_OPENGLES3
   struct scaler_ctx *scaler = &gl->pbo_readback_scaler;
   scaler->in_width    = gl->vp.width;
   scaler->in_height   = gl->vp.height;
   scaler->out_width   = gl->vp.width;
   scaler->out_height  = gl->vp.height;
   scaler->in_stride   = gl->vp.width * sizeof(uint32_t);
   scaler->out_stride  = gl->vp.width * 3;
   scaler->in_fmt      = SCALER_FMT_ARGB8888;
   scaler->out_fmt     = SCALER_FMT_BGR24;
   scaler->scaler_type = SCALER_TYPE_POINT;

   if (!scaler_ctx_gen_filter(scaler))
   {
      gl->pbo_readback_enable = false;
      RARCH_ERR("Failed to init pixel conversion for PBO.\n");
      glDeleteBuffers(4, gl->pbo_readback);
   }
#endif
}
#endif

static const gfx_ctx_driver_t *gl_get_context(gl_t *gl)
{
   const struct retro_hw_render_callback *cb = 
      (const struct retro_hw_render_callback*)
      &g_extern.system.hw_render_callback;
   unsigned major = cb->version_major;
   unsigned minor = cb->version_minor;
#ifdef HAVE_OPENGLES
   enum gfx_ctx_api api = GFX_CTX_OPENGL_ES_API;
   const char *api_name = "OpenGL ES 2.0";
#ifdef HAVE_OPENGLES3
   if (cb->context_type == RETRO_HW_CONTEXT_OPENGLES3)
   {
      major = 3;
      minor = 0;
      api_name = "OpenGL ES 3.0";
   }
   else if (cb->context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION)
      api_name = "OpenGL ES 3.1+";
#endif
#else
   enum gfx_ctx_api api = GFX_CTX_OPENGL_API;
   const char *api_name = "OpenGL";
#endif

   gl->shared_context_use = g_settings.video.shared_context
      && cb->context_type != RETRO_HW_CONTEXT_NONE;

   if (*g_settings.video.gl_context)
   {
      const gfx_ctx_driver_t *ctx = gfx_ctx_find_driver(
            g_settings.video.gl_context);

      if (ctx)
      {
         if (!ctx->bind_api(gl, api, major, minor))
         {
            RARCH_ERR("Failed to bind API %s to context %s.\n", api_name, g_settings.video.gl_context);
            return NULL;
         }

         /* Enables or disables offscreen HW context. */
         if (ctx->bind_hw_render)
            ctx->bind_hw_render(gl, gl->shared_context_use);

         if (!ctx->init(gl))
         {
            RARCH_ERR("Failed to init GL context: %s.\n", ctx->ident);
            return NULL;
         }
      }
      else
      {
         RARCH_ERR("Didn't find GL context: %s.\n", g_settings.video.gl_context);
         return NULL;
      }

      return ctx;
   }

   return gfx_ctx_init_first(gl, api, major, minor, gl->shared_context_use);
}

#ifdef GL_DEBUG
#ifdef HAVE_OPENGLES2
#define DEBUG_CALLBACK_TYPE GL_APIENTRY

#define GL_DEBUG_SOURCE_API GL_DEBUG_SOURCE_API_KHR
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR
#define GL_DEBUG_SOURCE_SHADER_COMPILER GL_DEBUG_SOURCE_SHADER_COMPILER_KHR
#define GL_DEBUG_SOURCE_THIRD_PARTY GL_DEBUG_SOURCE_THIRD_PARTY_KHR
#define GL_DEBUG_SOURCE_APPLICATION GL_DEBUG_SOURCE_APPLICATION_KHR
#define GL_DEBUG_SOURCE_OTHER GL_DEBUG_SOURCE_OTHER_KHR
#define GL_DEBUG_TYPE_ERROR GL_DEBUG_TYPE_ERROR_KHR
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR
#define GL_DEBUG_TYPE_PORTABILITY GL_DEBUG_TYPE_PORTABILITY_KHR
#define GL_DEBUG_TYPE_PERFORMANCE GL_DEBUG_TYPE_PERFORMANCE_KHR
#define GL_DEBUG_TYPE_MARKER GL_DEBUG_TYPE_MARKER_KHR
#define GL_DEBUG_TYPE_PUSH_GROUP GL_DEBUG_TYPE_PUSH_GROUP_KHR
#define GL_DEBUG_TYPE_POP_GROUP GL_DEBUG_TYPE_POP_GROUP_KHR
#define GL_DEBUG_TYPE_OTHER GL_DEBUG_TYPE_OTHER_KHR
#define GL_DEBUG_SEVERITY_HIGH GL_DEBUG_SEVERITY_HIGH_KHR
#define GL_DEBUG_SEVERITY_MEDIUM GL_DEBUG_SEVERITY_MEDIUM_KHR
#define GL_DEBUG_SEVERITY_LOW GL_DEBUG_SEVERITY_LOW_KHR
#else
#define DEBUG_CALLBACK_TYPE APIENTRY
#endif
static void DEBUG_CALLBACK_TYPE gl_debug_cb(GLenum source, GLenum type,
      GLuint id, GLenum severity, GLsizei length,
      const GLchar *message, void *userParam)
{
   const char *src, *typestr;
   gl_t *gl = (gl_t*)userParam; /* Useful for debugger. */

   (void)gl;
   (void)id;
   (void)length;

   switch (source)
   {
      case GL_DEBUG_SOURCE_API:
         src = "API";
         break;
      case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
         src = "Window system";
         break;
      case GL_DEBUG_SOURCE_SHADER_COMPILER:
         src = "Shader compiler";
         break;
      case GL_DEBUG_SOURCE_THIRD_PARTY:
         src = "3rd party";
         break;
      case GL_DEBUG_SOURCE_APPLICATION:
         src = "Application";
         break;
      case GL_DEBUG_SOURCE_OTHER:
         src = "Other";
         break;
      default:
         src = "Unknown";
         break;
   }

   switch (type)
   {
      case GL_DEBUG_TYPE_ERROR:
         typestr = "Error";
         break;
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
         typestr = "Deprecated behavior";
         break;
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
         typestr = "Undefined behavior";
         break;
      case GL_DEBUG_TYPE_PORTABILITY:
         typestr = "Portability";
         break;
      case GL_DEBUG_TYPE_PERFORMANCE:
         typestr = "Performance";
         break;
      case GL_DEBUG_TYPE_MARKER:
         typestr = "Marker";
         break;
      case GL_DEBUG_TYPE_PUSH_GROUP:
         typestr = "Push group";
         break;
      case GL_DEBUG_TYPE_POP_GROUP:
        typestr = "Pop group";
        break;
      case GL_DEBUG_TYPE_OTHER:
        typestr = "Other";
        break;
      default:
        typestr = "Unknown";
        break;
   }

   switch (severity)
   {
      case GL_DEBUG_SEVERITY_HIGH:
         RARCH_ERR("[GL debug (High, %s, %s)]: %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_MEDIUM:
         RARCH_WARN("[GL debug (Medium, %s, %s)]: %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_LOW:
         RARCH_LOG("[GL debug (Low, %s, %s)]: %s\n", src, typestr, message);
         break;
   }
}

static void gl_begin_debug(gl_t *gl)
{
   if (gl_query_extension(gl, "KHR_debug"))
   {
#ifdef HAVE_OPENGLES2
      glDebugMessageCallbackKHR(gl_debug_cb, gl);
      glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
#else
      glDebugMessageCallback(gl_debug_cb, gl);
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
   }
#ifndef HAVE_OPENGLES2
   else if (gl_query_extension(gl, "ARB_debug_output"))
   {
      glDebugMessageCallbackARB(gl_debug_cb, gl);
      glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
   }
#endif
   else
      RARCH_ERR("Neither GL_KHR_debug nor GL_ARB_debug_output are implemented. Cannot start GL debugging.\n");
}
#endif

static void *gl_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   unsigned win_width, win_height;
#ifdef _WIN32
   gfx_set_dwm();
#endif

   gl_t *gl = (gl_t*)calloc(1, sizeof(gl_t));
   if (!gl)
      return NULL;

   gl->ctx_driver = gl_get_context(gl);
   if (!gl->ctx_driver)
   {
      free(gl);
      return NULL;
   }

   gl->video_info = *video;

   RARCH_LOG("Found GL context: %s\n", gl->ctx_driver->ident);

   context_get_video_size_func(gl, &gl->full_x, &gl->full_y);
   RARCH_LOG("Detecting screen resolution %ux%u.\n", gl->full_x, gl->full_y);

   context_swap_interval_func(gl, video->vsync ? g_settings.video.swap_interval : 0);

   win_width  = video->width;
   win_height = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = gl->full_x;
      win_height = gl->full_y;
   }

   if (!context_set_video_mode_func(gl, win_width, win_height, video->fullscreen))
   {
      free(gl);
      return NULL;
   }

   /* Clear out potential error flags in case we use cached context. */
   glGetError(); 

   const char *vendor = (const char*)glGetString(GL_VENDOR);
   const char *renderer = (const char*)glGetString(GL_RENDERER);
   RARCH_LOG("[GL]: Vendor: %s, Renderer: %s.\n", vendor, renderer);

   const char *version = (const char*)glGetString(GL_VERSION);
   RARCH_LOG("[GL]: Version: %s.\n", version);

#ifndef RARCH_CONSOLE
   rglgen_resolve_symbols(gl->ctx_driver->get_proc_address);
#endif

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   if (!resolve_extensions(gl))
   {
      context_destroy_func(gl);
      free(gl);
      return NULL;
   }

#ifdef GL_DEBUG
   gl_begin_debug(gl);
#endif

   gl->vsync      = video->vsync;
   gl->fullscreen = video->fullscreen;
   
   /* Get real known video size, which might have been altered by context. */
   context_get_video_size_func(gl, &gl->win_width, &gl->win_height);
   RARCH_LOG("GL: Using resolution %ux%u\n", gl->win_width, gl->win_height);

   if (gl->full_x || gl->full_y)
   {
      /* We got bogus from gfx_ctx_get_video_size. Replace. */
      gl->full_x = gl->win_width;
      gl->full_y = gl->win_height;
   }

   struct retro_hw_render_callback *hw_render = &g_extern.system.hw_render_callback;
   gl->vertex_ptr = hw_render->bottom_left_origin ? vertexes : vertexes_flipped;

   /* Better pipelining with GPU due to synchronous glSubTexImage.
    * Multiple async PBOs would be an alternative,
    * but still need multiple textures with PREV.
    */
   gl->textures = 4;
#ifdef HAVE_FBO
   gl->hw_render_use = hw_render->context_type != RETRO_HW_CONTEXT_NONE;
   if (gl->hw_render_use)
   {
      /* All on GPU, no need to excessively
       * create textures. */
      gl->textures = 1;
#ifdef GL_DEBUG
      context_bind_hw_render(gl, true);
      gl_begin_debug(gl);
      context_bind_hw_render(gl, false);
#endif
   }
#endif
   gl->white_color_ptr = white_color;

#ifdef HAVE_GLSL
   gl_glsl_set_get_proc_address(gl->ctx_driver->get_proc_address);
   gl_glsl_set_context_type(gl->core_context,
         hw_render->version_major, hw_render->version_minor);
#endif

   if (!gl_shader_init(gl))
   {
      RARCH_ERR("[GL]: Shader init failed.\n");
      context_destroy_func(gl);
      free(gl);
      return NULL;
   }

   if (gl->shader)
   {
      unsigned minimum = gl->shader->get_prev_textures();
      gl->textures = max(minimum + 1, gl->textures);
   }

   RARCH_LOG("GL: Using %u textures.\n", gl->textures);
   RARCH_LOG("GL: Loaded %u program(s).\n", gl_shader_num(gl));

   gl->tex_w = RARCH_SCALE_BASE * video->input_scale;
   gl->tex_h = RARCH_SCALE_BASE * video->input_scale;

   gl->keep_aspect = video->force_aspect;

   /* Apparently need to set viewport for passes 
    * when we aren't using FBOs. */
   gl_set_shader_viewport(gl, 0);
   gl_set_shader_viewport(gl, 1);

   bool force_smooth = false;
   gl->tex_mipmap = gl_shader_mipmap_input(gl, 1);

   if (gl_shader_filter_type(gl, 1, &force_smooth))
      gl->tex_min_filter = gl->tex_mipmap ? (force_smooth ? 
            GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) 
         : (force_smooth ? GL_LINEAR : GL_NEAREST);
   else
      gl->tex_min_filter = gl->tex_mipmap ? 
         (video->smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) 
         : (video->smooth ? GL_LINEAR : GL_NEAREST);
   
   gl->tex_mag_filter = min_filter_to_mag(gl->tex_min_filter);
   gl->wrap_mode = gl_wrap_type_to_enum(gl_shader_wrap_type(gl, 1));

   gl_set_texture_fmts(gl, video->rgb32);

#ifndef HAVE_OPENGLES
   if (!gl->core_context)
      glEnable(GL_TEXTURE_2D);
#endif

   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DITHER);

   memcpy(gl->tex_info.coord, tex_coords, sizeof(gl->tex_info.coord));
   gl->coords.vertex         = gl->vertex_ptr;
   gl->coords.tex_coord      = gl->tex_info.coord;
   gl->coords.color          = gl->white_color_ptr;
   gl->coords.lut_tex_coord  = tex_coords;
   gl->coords.vertices       = 4;

   /* Empty buffer that we use to clear out 
    * the texture with on res change. */
   gl->empty_buf = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

#if !defined(HAVE_PSGL)
   gl->conv_buffer = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);
   if (!gl->conv_buffer)
   {
      context_destroy_func(gl);
      free(gl);
      return NULL;
   }
#endif

   gl_init_textures(gl, video);
   gl_init_textures_data(gl);

#ifdef HAVE_FBO
   gl_init_fbo(gl, gl->tex_w, gl->tex_h);

#ifndef HAVE_GCMGL
   if (gl->hw_render_use && 
         !gl_init_hw_render(gl, gl->tex_w, gl->tex_h))
   {
      context_destroy_func(gl);
      free(gl);
      return NULL;
   }
#endif
#endif

   if (input && input_data)
      context_input_driver_func(gl, input, input_data);
   
#ifndef RARCH_CONSOLE
   if (g_settings.video.font_enable)
#endif
   {
      if (!gl_font_init_first(&gl->font_driver, &gl->font_handle,
            gl, *g_settings.video.font_path 
            ? g_settings.video.font_path : NULL, g_settings.video.font_size))
         RARCH_ERR("[GL]: Failed to init font renderer.\n");
   }

#ifdef HAVE_GL_ASYNC_READBACK
   gl_init_pbo_readback(gl);
#endif

   if (!gl_check_error())
   {
      context_destroy_func(gl);
      free(gl);
      return NULL;
   }

   context_bind_hw_render(gl, true);
   return gl;
}

static bool gl_alive(void *data)
{
   gl_t *gl = (gl_t*)data;
   bool quit = false, resize = false;
    
   if (!gl)
      return false;

   context_check_window_func(gl, &quit,
         &resize, &gl->win_width, &gl->win_height,
         g_extern.frame_count);

   if (quit)
      gl->quitting = true;
   else if (resize)
      gl->should_resize = true;

   return !gl->quitting;
}

static bool gl_focus(void *data)
{
   gl_t *gl = (gl_t*)data;
   return gl && context_has_focus_func(gl);
}

static void gl_update_tex_filter_frame(gl_t *gl)
{
   unsigned i;
   bool smooth = false;

   if (!gl)
      return;

   context_bind_hw_render(gl, false);
   if (!gl_shader_filter_type(gl, 1, &smooth))
      smooth = g_settings.video.smooth;
   GLenum wrap_mode = gl_wrap_type_to_enum(gl_shader_wrap_type(gl, 1));

   gl->tex_mipmap = gl_shader_mipmap_input(gl, 1);

   gl->video_info.smooth = smooth;
   GLuint new_filt = gl->tex_mipmap ? (smooth ? 
         GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) 
      : (smooth ? GL_LINEAR : GL_NEAREST);
   if (new_filt == gl->tex_min_filter && wrap_mode == gl->wrap_mode)
      return;

   gl->tex_min_filter = new_filt;
   gl->tex_mag_filter = min_filter_to_mag(gl->tex_min_filter);

   gl->wrap_mode = wrap_mode;
   for (i = 0; i < gl->textures; i++)
   {
      if (gl->texture[i])
      {
         glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->wrap_mode);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->wrap_mode);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_mag_filter);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_min_filter);
      }
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   context_bind_hw_render(gl, true);
}

static bool gl_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_GLSL) || defined(HAVE_CG)
   gl_t *gl = (gl_t*)data;

   if (!gl)
      return false;

   context_bind_hw_render(gl, false);

   if (type == RARCH_SHADER_NONE)
      return false;

   gl_shader_deinit(gl);

   switch (type)
   {
#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         gl->shader = &gl_glsl_backend;
         break;
#endif

#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         gl->shader = &gl_cg_backend;
         break;
#endif

      default:
         gl->shader = NULL;
         break;
   }

   if (!gl->shader)
   {
      RARCH_ERR("[GL]: Cannot find shader core for path: %s.\n", path);
      context_bind_hw_render(gl, true);
      return false;
   }

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
#endif

   if (!gl->shader->init(gl, path))
   {
      RARCH_WARN("[GL]: Failed to set multipass shader. Falling back to stock.\n");
      bool ret = gl->shader->init(gl, NULL);
      if (!ret)
         gl->shader = NULL;
      context_bind_hw_render(gl, true);
      return false;
   }

   gl_update_tex_filter_frame(gl);

   if (gl->shader)
   {
      unsigned textures = gl->shader->get_prev_textures() + 1;

      if (textures > gl->textures) // Have to reinit a bit.
      {
#if defined(HAVE_FBO) && !defined(HAVE_GCMGL)
         gl_deinit_hw_render(gl);
#endif

         glDeleteTextures(gl->textures, gl->texture);
#if defined(HAVE_PSGL)
         glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
         glDeleteBuffers(1, &gl->pbo);
#endif
         gl->textures = textures;
         RARCH_LOG("GL: Using %u textures.\n", gl->textures);
         gl->tex_index = 0;
         gl_init_textures(gl, &gl->video_info);
         gl_init_textures_data(gl);

#if defined(HAVE_FBO) && !defined(HAVE_GCMGL)
         if (gl->hw_render_use)
            gl_init_hw_render(gl, gl->tex_w, gl->tex_h);
#endif
      }
   }

#ifdef HAVE_FBO
   gl_init_fbo(gl, gl->tex_w, gl->tex_h);
#endif

   /* Apparently need to set viewport for passes when we aren't using FBOs. */
   gl_set_shader_viewport(gl, 0);
   gl_set_shader_viewport(gl, 1);
   context_bind_hw_render(gl, true);
   return true;
#else
   return false;
#endif
}

static void gl_viewport_info(void *data, struct rarch_viewport *vp)
{
   unsigned top_y, top_dist;
   gl_t *gl = (gl_t*)data;
   *vp = gl->vp;
   vp->full_width  = gl->win_width;
   vp->full_height = gl->win_height;

   /* Adjust as GL viewport is bottom-up. */
   top_y = vp->y + vp->height;
   top_dist = gl->win_height - top_y;
   vp->y = top_dist;
}

static bool gl_read_viewport(void *data, uint8_t *buffer)
{
#ifndef NO_GL_READ_PIXELS
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return false;

   context_bind_hw_render(gl, false);
    
   RARCH_PERFORMANCE_INIT(read_viewport);
   RARCH_PERFORMANCE_START(read_viewport);

#ifdef HAVE_GL_ASYNC_READBACK
   if (gl->pbo_readback_enable)
   {
      /* Don't readback if we're in menu mode. */
      if (!gl->pbo_readback_valid[gl->pbo_readback_index]) 
      {
         /* We haven't buffered up enough frames yet, come back later. */
         context_bind_hw_render(gl, true);
         return false;
      }

      gl->pbo_readback_valid[gl->pbo_readback_index] = false;
      glBindBuffer(GL_PIXEL_PACK_BUFFER, gl->pbo_readback[gl->pbo_readback_index]);
#ifdef HAVE_OPENGLES3
      /* Slower path, but should work on all implementations at least. */
      unsigned num_pixels = gl->vp.width * gl->vp.height;
      const uint8_t *ptr = (const uint8_t*)glMapBufferRange(GL_PIXEL_PACK_BUFFER,
            0, num_pixels * sizeof(uint32_t), GL_MAP_READ_BIT);
      if (ptr)
      {
         unsigned x, y;
         for (y = 0; y < gl->vp.height; y++)
         {
            for (x = 0; x < gl->vp.width; x++, buffer += 3, ptr += 4)
            {
               buffer[0] = ptr[2]; /* RGBA -> BGR. */
               buffer[1] = ptr[1];
               buffer[2] = ptr[0];
            }
         }
      }
      else
      {
         RARCH_ERR("[GL]: Failed to map pixel unpack buffer.\n");
         context_bind_hw_render(gl, true);
         return false;
      }
#else
      const void *ptr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
      if (!ptr)
      {
         RARCH_ERR("[GL]: Failed to map pixel unpack buffer.\n");
         context_bind_hw_render(gl, true);
         return false;
      }

      scaler_ctx_scale(&gl->pbo_readback_scaler, buffer, ptr);
#endif
      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   }
   else /* Use slow synchronous readbacks. Use this with plain screenshots as we don't really care about performance in this case. */
#endif
   {
      /* GLES2 only guarantees GL_RGBA/GL_UNSIGNED_BYTE 
       * readbacks so do just that.
       * GLES2 also doesn't support reading back data 
       * from front buffer, so render a cached frame 
       * and have gl_frame() do the readback while it's 
       * in the back buffer.
       *
       * Keep codepath similar for GLES and desktop GL.
       */

      unsigned num_pixels = gl->vp.width * gl->vp.height;

      gl->readback_buffer_screenshot = malloc(num_pixels * sizeof(uint32_t));
      if (!gl->readback_buffer_screenshot)
      {
         RARCH_PERFORMANCE_STOP(read_viewport);
         context_bind_hw_render(gl, true);
         return false;
      }

      rarch_render_cached_frame();

      uint8_t *dst = buffer;
      const uint8_t *src = (const uint8_t*)gl->readback_buffer_screenshot;
      unsigned i;
      for (i = 0; i < num_pixels; i++, dst += 3, src += 4)
      {
         dst[0] = src[2]; /* RGBA -> BGR. */
         dst[1] = src[1];
         dst[2] = src[0];
      }

      free(gl->readback_buffer_screenshot);
      gl->readback_buffer_screenshot = NULL;
   }

   RARCH_PERFORMANCE_STOP(read_viewport);
   context_bind_hw_render(gl, true);
   return true;
#else
   return false;
#endif
}

#ifdef HAVE_OVERLAY
static void gl_free_overlay(gl_t *gl);
static bool gl_overlay_load(void *data, 
      const struct texture_image *images, unsigned num_images)
{
   unsigned i, j;
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return false;

   context_bind_hw_render(gl, false);

   gl_free_overlay(gl);
   gl->overlay_tex = (GLuint*)calloc(num_images, sizeof(*gl->overlay_tex));
   if (!gl->overlay_tex)
   {
      context_bind_hw_render(gl, true);
      return false;
   }

   gl->overlay_vertex_coord = (GLfloat*)calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_tex_coord    = (GLfloat*)calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_color_coord  = (GLfloat*)calloc(4 * 4 * num_images, sizeof(GLfloat));
   if (!gl->overlay_vertex_coord || !gl->overlay_tex_coord || !gl->overlay_color_coord)
      return false;

   gl->overlays = num_images;
   glGenTextures(num_images, gl->overlay_tex);

   for (i = 0; i < num_images; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->overlay_tex[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

      glPixelStorei(GL_UNPACK_ALIGNMENT,
            get_alignment(images[i].width * sizeof(uint32_t)));

      glTexImage2D(GL_TEXTURE_2D, 0, driver.gfx_use_rgba ? 
            GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
            images[i].width, images[i].height, 0,
            driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
            RARCH_GL_FORMAT32, images[i].pixels);

      /* Default. Stretch to whole screen. */
      gl_overlay_tex_geom(gl, i, 0, 0, 1, 1);
      gl_overlay_vertex_geom(gl, i, 0, 0, 1, 1);
      for (j = 0; j < 16; j++)
         gl->overlay_color_coord[16 * i + j] = 1.0f;
   }

   context_bind_hw_render(gl, true);
   return true;
}

static void gl_overlay_tex_geom(void *data,
      unsigned image,
      GLfloat x, GLfloat y,
      GLfloat w, GLfloat h)
{
   GLfloat *tex = NULL;
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;

   tex = (GLfloat*)&gl->overlay_tex_coord[image * 8];

   if (!tex)
      return;

   tex[0] = x;     tex[1] = y;
   tex[2] = x + w; tex[3] = y;
   tex[4] = x;     tex[5] = y + h;
   tex[6] = x + w; tex[7] = y + h;
}

static void gl_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y,
      float w, float h)
{
   GLfloat *vertex = NULL;
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;

   vertex = (GLfloat*)&gl->overlay_vertex_coord[image * 8];

   /* Flipped, so we preserve top-down semantics. */
   y = 1.0f - y;
   h = -h;

   if (!vertex)
      return;

   vertex[0] = x;     vertex[1] = y;
   vertex[2] = x + w; vertex[3] = y;
   vertex[4] = x;     vertex[5] = y + h;
   vertex[6] = x + w; vertex[7] = y + h;
}

static void gl_overlay_enable(void *data, bool state)
{
   gl_t *gl = (gl_t*)data;

   if (!gl)
      return;

   gl->overlay_enable = state;
   if (gl->ctx_driver->show_mouse && gl->fullscreen)
      gl->ctx_driver->show_mouse(gl, state);
}

static void gl_overlay_full_screen(void *data, bool enable)
{
   gl_t *gl = (gl_t*)data;

   if (gl)
      gl->overlay_full_screen = enable;
}

static void gl_overlay_set_alpha(void *data, unsigned image, float mod)
{
   GLfloat *color = NULL;
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;

   color = (GLfloat*)&gl->overlay_color_coord[image * 16];

   if (!color)
      return;

   color[ 0 + 3] = mod;
   color[ 4 + 3] = mod;
   color[ 8 + 3] = mod;
   color[12 + 3] = mod;
}

static void gl_render_overlay(void *data)
{
   unsigned i;
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;

   glEnable(GL_BLEND);

   if (gl->overlay_full_screen)
      glViewport(0, 0, gl->win_width, gl->win_height);

   /* Ensure that we reset the attrib array. */
   if (gl->shader)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
   gl->coords.vertex    = gl->overlay_vertex_coord;
   gl->coords.tex_coord = gl->overlay_tex_coord;
   gl->coords.color     = gl->overlay_color_coord;
   gl->coords.vertices  = 4 * gl->overlays;
   gl_shader_set_coords(gl, &gl->coords, &gl->mvp_no_rot);

   for (i = 0; i < gl->overlays; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->overlay_tex[i]);
      glDrawArrays(GL_TRIANGLE_STRIP, 4 * i, 4);
   }

   glDisable(GL_BLEND);
   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_info.coord;
   gl->coords.color     = gl->white_color_ptr;
   gl->coords.vertices  = 4;
   if (gl->overlay_full_screen)
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
}

static const video_overlay_interface_t gl_overlay_interface = {
   gl_overlay_enable,
   gl_overlay_load,
   gl_overlay_tex_geom,
   gl_overlay_vertex_geom,
   gl_overlay_full_screen,
   gl_overlay_set_alpha,
};

static void gl_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &gl_overlay_interface;
}
#endif

#ifdef HAVE_FBO
static uintptr_t gl_get_current_framebuffer(void *data)
{
   gl_t *gl = (gl_t*)data;
   return gl->hw_render_fbo[(gl->tex_index + 1) % gl->textures];
}

static retro_proc_address_t gl_get_proc_address(void *data, const char *sym)
{
   gl_t *gl = (gl_t*)data;
   return gl->ctx_driver->get_proc_address(sym);
}
#endif

static void gl_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl_t *gl = (gl_t*)data;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         gfx_set_square_pixel_viewport(
               g_extern.system.av_info.geometry.base_width,
               g_extern.system.av_info.geometry.base_height);
         break;

      case ASPECT_RATIO_CORE:
         gfx_set_core_viewport();
         break;

      case ASPECT_RATIO_CONFIG:
         gfx_set_config_viewport();
         break;

      default:
         break;
   }

   g_extern.system.aspect_ratio = aspectratio_lut[aspect_ratio_idx].value;

   if (!gl)
      return;

   gl->keep_aspect = true;
   gl->should_resize = true;
}

#if defined(HAVE_MENU)
static void gl_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;

   context_bind_hw_render(gl, false);

   if (!gl->menu_texture)
   {
      glGenTextures(1, &gl->menu_texture);
      glBindTexture(GL_TEXTURE_2D, gl->menu_texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   }
   else
      glBindTexture(GL_TEXTURE_2D, gl->menu_texture);

   gl->menu_texture_alpha = alpha;

   unsigned base_size = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(width * base_size));

   if (rgb32)
   {
      glTexImage2D(GL_TEXTURE_2D,
            0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
            width, height,
            0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
            RARCH_GL_FORMAT32, frame);
   }
   else
   {
      glTexImage2D(GL_TEXTURE_2D,
            0, GL_RGBA, width, height, 0, GL_RGBA,
            GL_UNSIGNED_SHORT_4_4_4_4, frame);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   context_bind_hw_render(gl, true);
}

static void gl_set_texture_enable(void *data, bool state, bool full_screen)
{
   gl_t *gl = (gl_t*)data;

   if (!gl)
      return;

   gl->menu_texture_enable = state;
   gl->menu_texture_full_screen = full_screen;
}
#endif

static void gl_apply_state_changes(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (gl)
      gl->should_resize = true;
}

static void gl_set_osd_msg(void *data, const char *msg,
      const struct font_params *params)
{
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;

   if (gl->font_driver && gl->font_handle)
   {
      context_bind_hw_render(gl, false);
      gl->font_driver->render_msg(gl->font_handle, msg, params);
      context_bind_hw_render(gl, true);
   }
}

static void gl_show_mouse(void *data, bool state)
{
   gl_t *gl = (gl_t*)data;

   if (gl && gl->ctx_driver->show_mouse)
      gl->ctx_driver->show_mouse(gl, state);
}

static struct gfx_shader *gl_get_current_shader(void *data)
{
   gl_t *gl = (gl_t*)data;
   return (gl && gl->shader) ? gl->shader->get_current_shader() : NULL;
}

static const video_poke_interface_t gl_poke_interface = {
   NULL,
#ifdef HAVE_FBO
   gl_get_current_framebuffer,
   gl_get_proc_address,
#endif
   gl_set_aspect_ratio,
   gl_apply_state_changes,
#if defined(HAVE_MENU)
   gl_set_texture_frame,
   gl_set_texture_enable,
#endif
   gl_set_osd_msg,

   gl_show_mouse,
   NULL,

   gl_get_current_shader,
};

static void gl_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &gl_poke_interface;
}

video_driver_t video_gl = {
   gl_init,
   gl_frame,
   gl_set_nonblock_state,
   gl_alive,
   gl_focus,

   gl_set_shader,

   gl_free,
   "gl",

   gl_set_rotation,

   gl_viewport_info,

   gl_read_viewport,

#ifdef HAVE_OVERLAY
   gl_get_overlay_interface,
#endif
   gl_get_poke_interface,
};

