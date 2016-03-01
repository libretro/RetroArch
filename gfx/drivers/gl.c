/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <gfx/math/matrix_4x4.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "../../driver.h"
#include "../../record/record_driver.h"
#include "../../performance.h"

#include "../../libretro.h"
#include "../../general.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../common/gl_common.h"

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../font_driver.h"
#include "../video_context_driver.h"

#ifdef HAVE_GLSL
#include "../drivers_shader/shader_glsl.h"
#endif

#ifdef GL_DEBUG
#include <string/string_list.h>
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#if defined(_WIN32) && !defined(_XBOX)
#include "../common/win32_common.h"
#endif

#include "../video_shader_driver.h"

#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE     0x9117
#endif

#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT        0x00000001
#endif

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

static INLINE void context_bind_hw_render(gl_t *gl, bool enable)
{
   if (gl && gl->shared_context_use)
      gfx_ctx_ctl(GFX_CTL_BIND_HW_RENDER, &enable);
}

static INLINE bool gl_query_extension(gl_t *gl, const char *ext)
{
   bool ret = false;

   if (gl->core_context)
   {
#ifdef GL_NUM_EXTENSIONS
      GLint i;
      GLint exts = 0;
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
static void gl_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y, float w, float h);
static void gl_overlay_tex_geom(void *data,
      unsigned image,
      float x, float y, float w, float h);
#endif

#define set_texture_coords(coords, xamt, yamt) \
   coords[2] = xamt; \
   coords[6] = xamt; \
   coords[5] = yamt; \
   coords[7] = yamt

#if defined(HAVE_EGL) && defined(HAVE_OPENGLES2)
static bool gl_check_eglimage_proc(void)
{
   return glEGLImageTargetTexture2DOES != NULL;
}
#endif

#ifdef HAVE_GL_SYNC
static bool gl_check_sync_proc(gl_t *gl)
{
   if (!gl_query_extension(gl, "ARB_sync"))
      return false;

   return glFenceSync && glDeleteSync && glClientWaitSync;
}
#endif

#ifndef HAVE_OPENGLES
static bool gl_init_vao(gl_t *gl)
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
#define gl_check_fbo_proc(gl) (true)
#elif !defined(HAVE_OPENGLES2)
static bool gl_check_fbo_proc(gl_t *gl)
{
   if (!gl->core_context && !gl_query_extension(gl, "ARB_framebuffer_object"))
      return false;

   return glGenFramebuffers 
      && glBindFramebuffer 
      && glFramebufferTexture2D 
      && glCheckFramebufferStatus 
      && glDeleteFramebuffers 
      && glGenRenderbuffers 
      && glBindRenderbuffer 
      && glFramebufferRenderbuffer 
      && glRenderbufferStorage 
      && glDeleteRenderbuffers;
}
#else
#define gl_check_fbo_proc(gl) (true)
#endif
#endif

/* Shaders */

static bool gl_shader_init(gl_t *gl)
{
   video_shader_ctx_init_t init_data;
   enum rarch_shader_type type;
   bool ret                        = false;
   const shader_backend_t *backend = NULL;
   settings_t *settings            = config_get_ptr();
   const char *shader_path         = (settings->video.shader_enable 
         && *settings->video.shader_path) ? settings->video.shader_path : NULL;

   if (!gl)
   {
      RARCH_ERR("Invalid GL instance passed.\n");
      return false;
   }

   type = video_shader_parse_type(shader_path,
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
#ifdef HAVE_CG
   if (gl->core_context && backend == &gl_cg_backend)
   {
      RARCH_ERR("[GL]: Cg cannot be used with core GL context. Falling back to GLSL.\n");
      backend = &gl_glsl_backend;
      shader_path = NULL;
   }
#endif
#endif

   init_data.shader = backend;
   init_data.data   = gl;
   init_data.path   = shader_path;

   ret = video_shader_driver_ctl(SHADER_CTL_INIT, &init_data);

   if (!ret)
   {
      RARCH_ERR("[GL]: Failed to initialize shader, falling back to stock.\n");

      init_data.shader = backend;
      init_data.data   = gl;
      init_data.path   = NULL;

      ret = video_shader_driver_ctl(SHADER_CTL_INIT, &init_data);
   }

   return ret;
}

static void gl_shader_deinit(gl_t *gl)
{
   video_shader_driver_ctl(SHADER_CTL_DEINIT, NULL);
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

#ifdef IOS
/* There is no default frame buffer on iOS. */
void cocoagl_bind_game_view_fbo(void);
#define gl_bind_backbuffer() cocoagl_bind_game_view_fbo()
#else
#define gl_bind_backbuffer() glBindFramebuffer(RARCH_GL_FRAMEBUFFER, 0)
#endif

static INLINE GLenum min_filter_to_mag(GLenum type)
{
   switch (type)
   {
      case GL_LINEAR_MIPMAP_LINEAR:
         return GL_LINEAR;
      case GL_NEAREST_MIPMAP_NEAREST:
         return GL_NEAREST;
      default:
         break;
   }

   return type;
}

#ifdef HAVE_FBO
/* Compute FBO geometry.
 * When width/height changes or window sizes change, 
 * we have to recalculate geometry of our FBO. */

static void gl_compute_fbo_geometry(gl_t *gl,
      unsigned width, unsigned height,
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
      struct gfx_fbo_rect  *fbo_rect   = &gl->fbo_rect[i];
      struct gfx_fbo_scale *fbo_scale  = &gl->fbo_scale[i];

      switch (gl->fbo_scale[i].type_x)
      {
         case RARCH_SCALE_INPUT:
            fbo_rect->img_width      = last_width * fbo_scale->scale_x;
            fbo_rect->max_img_width  = last_max_width * fbo_scale->scale_x;
            break;

         case RARCH_SCALE_ABSOLUTE:
            fbo_rect->img_width      = fbo_rect->max_img_width = 
               fbo_scale->abs_x;
            break;

         case RARCH_SCALE_VIEWPORT:
            fbo_rect->img_width      = fbo_rect->max_img_width = 
               fbo_scale->scale_x * vp_width;
            break;
      }

      switch (fbo_scale->type_y)
      {
         case RARCH_SCALE_INPUT:
            fbo_rect->img_height     = last_height * fbo_scale->scale_y;
            fbo_rect->max_img_height = last_max_height * fbo_scale->scale_y;
            break;

         case RARCH_SCALE_ABSOLUTE:
            fbo_rect->img_height     = fbo_scale->abs_y;
            fbo_rect->max_img_height = fbo_scale->abs_y;
            break;

         case RARCH_SCALE_VIEWPORT:
            fbo_rect->img_height     = fbo_rect->max_img_height = 
               fbo_scale->scale_y * vp_height;
            break;
      }

      if (fbo_rect->img_width > (unsigned)max_size)
      {
         size_modified = true;
         fbo_rect->img_width = max_size;
      }

      if (fbo_rect->img_height > (unsigned)max_size)
      {
         size_modified = true;
         fbo_rect->img_height = max_size;
      }

      if (fbo_rect->max_img_width > (unsigned)max_size)
      {
         size_modified = true;
         fbo_rect->max_img_width = max_size;
      }

      if (fbo_rect->max_img_height > (unsigned)max_size)
      {
         size_modified = true;
         fbo_rect->max_img_height = max_size;
      }

      if (size_modified)
         RARCH_WARN("FBO textures exceeded maximum size of GPU (%dx%d). Resizing to fit.\n", max_size, max_size);

      last_width      = fbo_rect->img_width;
      last_height     = fbo_rect->img_height;
      last_max_width  = fbo_rect->max_img_width;
      last_max_height = fbo_rect->max_img_height;
   }
}

static void gl_create_fbo_texture(gl_t *gl, unsigned i, GLuint texture)
{
   unsigned mip_level;
   video_shader_ctx_wrap_t wrap;
   bool fp_fbo, srgb_fbo;
   GLenum min_filter, mag_filter, wrap_enum;
   video_shader_ctx_filter_t filter_type;
   bool mipmapped = false;
   bool smooth = false;
   settings_t *settings = config_get_ptr();
   GLuint base_filt     = settings->video.smooth ? GL_LINEAR : GL_NEAREST;
   GLuint base_mip_filt = settings->video.smooth ? 
      GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;

   glBindTexture(GL_TEXTURE_2D, texture);

   mip_level          = i + 2;
   mipmapped          = video_shader_driver_ctl(SHADER_CTL_MIPMAP_INPUT, &mip_level);
   min_filter         = mipmapped ? base_mip_filt : base_filt;
   filter_type.index  = i + 2;
   filter_type.smooth = &smooth;

   if (video_shader_driver_ctl(SHADER_CTL_FILTER_TYPE, &filter_type))
   {
      min_filter = mipmapped ? (smooth ? 
            GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
         : (smooth ? GL_LINEAR : GL_NEAREST);
   }

   mag_filter = min_filter_to_mag(min_filter);

   wrap.idx = i + 2;

   video_shader_driver_ctl(SHADER_CTL_WRAP_TYPE, &wrap);

   wrap_enum = gl_wrap_type_to_enum(wrap.type);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_enum);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_enum);

   fp_fbo   = gl->fbo_scale[i].fp_fbo;
   srgb_fbo = gl->fbo_scale[i].srgb_fbo;

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

   if (settings->video.force_srgb_disable)
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

static void gl_create_fbo_textures(gl_t *gl)
{
   int i;
   glGenTextures(gl->fbo_pass, gl->fbo_texture);

   for (i = 0; i < gl->fbo_pass; i++)
   {
      gl_create_fbo_texture(gl, i, gl->fbo_texture[i]);
   }

   if (gl->fbo_feedback_enable)
   {
      glGenTextures(1, &gl->fbo_feedback_texture);
      gl_create_fbo_texture(gl,
            gl->fbo_feedback_pass, gl->fbo_feedback_texture);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

static bool gl_create_fbo_targets(gl_t *gl)
{
   int i;
   GLenum status;

   if (!gl)
      return false;

   glBindTexture(GL_TEXTURE_2D, 0);
   glGenFramebuffers(gl->fbo_pass, gl->fbo);

   for (i = 0; i < gl->fbo_pass; i++)
   {
      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo[i]);
      glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

      status = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);
      if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
         goto error;
   }

   if (gl->fbo_feedback_texture)
   {
      glGenFramebuffers(1, &gl->fbo_feedback);
      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo_feedback);
      glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            gl->fbo_feedback_texture, 0);

      status = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);
      if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
         goto error;

      /* Make sure the feedback textures are cleared 
       * so we don't feedback noise. */
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);
   }

   return true;

error:
   glDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   if (gl->fbo_feedback)
      glDeleteFramebuffers(1, &gl->fbo_feedback);
   RARCH_ERR("Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
   return false;
}

static void gl_deinit_fbo(gl_t *gl)
{
   if (!gl->fbo_inited)
      return;

   glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
   glDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   memset(gl->fbo_texture, 0, sizeof(gl->fbo_texture));
   memset(gl->fbo, 0, sizeof(gl->fbo));
   gl->fbo_inited = false;
   gl->fbo_pass = 0;

   if (gl->fbo_feedback)
      glDeleteFramebuffers(1, &gl->fbo_feedback);
   if (gl->fbo_feedback_texture)
      glDeleteTextures(1, &gl->fbo_feedback_texture);

   gl->fbo_feedback_enable = false;
   gl->fbo_feedback_pass = -1;
   gl->fbo_feedback_texture = 0;
   gl->fbo_feedback = 0;
}

/* Set up render to texture. */

static void gl_init_fbo(gl_t *gl, unsigned fbo_width, unsigned fbo_height)
{
   int i;
   unsigned width, height;
   video_shader_ctx_scale_t scaler;
   video_shader_ctx_info_t shader_info;
   struct gfx_fbo_scale scale, scale_last;

   if (!video_shader_driver_ctl(SHADER_CTL_INFO, &shader_info))
      return;

   if (!gl || shader_info.num == 0)
      return;

   video_driver_get_size(&width, &height);

   scaler.idx   = 1;
   scaler.scale = &scale;

   video_shader_driver_ctl(SHADER_CTL_SCALE, &scaler);

   scaler.idx   = shader_info.num;
   scaler.scale = &scale_last;

   video_shader_driver_ctl(SHADER_CTL_SCALE, &scaler);

   /* we always want FBO to be at least initialized on startup for consoles */
   if (shader_info.num == 1 && !scale.valid)
      return;

   if (!gl_check_fbo_proc(gl))
   {
      RARCH_ERR("Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
      return;
   }

   gl->fbo_pass = shader_info.num - 1;
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
      scaler.idx   = i + 1;
      scaler.scale = &gl->fbo_scale[i];

      video_shader_driver_ctl(SHADER_CTL_SCALE, &scaler);

      if (!gl->fbo_scale[i].valid)
      {
         gl->fbo_scale[i].scale_x = gl->fbo_scale[i].scale_y = 1.0f;
         gl->fbo_scale[i].type_x  = gl->fbo_scale[i].type_y  = 
            RARCH_SCALE_INPUT;
         gl->fbo_scale[i].valid   = true;
      }
   }

   gl_compute_fbo_geometry(gl, fbo_width, fbo_height, width, height);

   for (i = 0; i < gl->fbo_pass; i++)
   {
      gl->fbo_rect[i].width  = next_pow2(gl->fbo_rect[i].img_width);
      gl->fbo_rect[i].height = next_pow2(gl->fbo_rect[i].img_height);
      RARCH_LOG("[GL]: Creating FBO %d @ %ux%u\n", i,
            gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   gl->fbo_feedback_enable = video_shader_driver_ctl(
         SHADER_CTL_GET_FEEDBACK_PASS, &gl->fbo_feedback_pass);

   if (gl->fbo_feedback_enable && gl->fbo_feedback_pass 
         < (unsigned)gl->fbo_pass)
   {
      RARCH_LOG("[GL]: Creating feedback FBO %d @ %ux%u\n", i,
            gl->fbo_rect[gl->fbo_feedback_pass].width,
            gl->fbo_rect[gl->fbo_feedback_pass].height);
   }
   else if (gl->fbo_feedback_enable)
   {
      RARCH_WARN("[GL]: Tried to create feedback FBO of pass #%u, but there are only %d FBO passes. Will use input texture as feedback texture.\n",
              gl->fbo_feedback_pass, gl->fbo_pass);
      gl->fbo_feedback_enable = false;
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
   GLenum status;
   unsigned i;
   bool depth = false, stencil = false;
   GLint max_fbo_size = 0, max_renderbuffer_size = 0;
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();

   /* We can only share texture objects through contexts.
    * FBOs are "abstract" objects and are not shared. */
   context_bind_hw_render(gl, true);

   RARCH_LOG("[GL]: Initializing HW render (%u x %u).\n", width, height);
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_fbo_size);
   glGetIntegerv(RARCH_GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size);
   RARCH_LOG("[GL]: Max texture size: %d px, renderbuffer size: %d px.\n",
         max_fbo_size, max_renderbuffer_size);

   if (!gl_check_fbo_proc(gl))
      return false;

   glBindTexture(GL_TEXTURE_2D, 0);
   glGenFramebuffers(gl->textures, gl->hw_render_fbo);

   depth   = hw_render->depth;
   stencil = hw_render->stencil;

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

      status = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);
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

static void gl_set_projection(gl_t *gl,
      struct gfx_ortho *ortho, bool allow_rotate)
{
   math_matrix_4x4 rot;

   /* Calculate projection. */
   matrix_4x4_ortho(&gl->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      gl->mvp = gl->mvp_no_rot;
      return;
   }

   matrix_4x4_rotate_z(&rot, M_PI * gl->rotation / 180.0f);
   matrix_4x4_multiply(&gl->mvp, &rot, &gl->mvp_no_rot);
}

static void gl_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   gfx_ctx_aspect_t aspect_data;
   unsigned width, height;
   int x = 0, y = 0;
   float device_aspect   = (float)viewport_width / viewport_height;
   struct gfx_ortho ortho = {0, 1, 0, 1, -1, 1};
   settings_t *settings  = config_get_ptr();
   gl_t           *gl    = (gl_t*)data;

   video_driver_get_size(&width, &height);

   aspect_data.aspect   = &device_aspect;
   aspect_data.width    = viewport_width;
   aspect_data.height   = viewport_height;

   gfx_ctx_ctl(GFX_CTL_TRANSLATE_ASPECT, &aspect_data);

   if (settings->video.scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&gl->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(), gl->keep_aspect);
      viewport_width  = gl->vp.width;
      viewport_height = gl->vp.height;
   }
   else if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct video_viewport *custom = video_viewport_get_custom();

         /* GL has bottom-left origin viewport. */
         x      = custom->x;
         y      = height - custom->y - custom->height;
         viewport_width  = custom->width;
         viewport_height = custom->height;
      }
      else
#endif
      {
         float delta;

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
            x     = (int)roundf(viewport_width * (0.5f - delta));
            viewport_width = (unsigned)roundf(2.0f * viewport_width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            y      = (int)roundf(viewport_height * (0.5f - delta));
            viewport_height = (unsigned)roundf(2.0f * viewport_height * delta);
         }
      }

      gl->vp.x      = x;
      gl->vp.y      = y;
      gl->vp.width  = viewport_width;
      gl->vp.height = viewport_height;
   }
   else
   {
      gl->vp.x      = gl->vp.y = 0;
      gl->vp.width  = viewport_width;
      gl->vp.height = viewport_height;
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
      gl->vp_out_width  = viewport_width;
      gl->vp_out_height = viewport_height;
   }

#if 0
   RARCH_LOG("Setting viewport @ %ux%u\n", viewport_width, viewport_height);
#endif
}

static void gl_set_rotation(void *data, unsigned rotation)
{
   gl_t *gl = (gl_t*)data;
   struct gfx_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (!gl)
      return;

   gl->rotation = 90 * rotation;
   gl_set_projection(gl, &ortho, true);
}

static void gl_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_mode_t mode;

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   gfx_ctx_ctl(GFX_CTL_SET_VIDEO_MODE, &mode);
}

#ifdef HAVE_FBO
static INLINE void gl_start_frame_fbo(gl_t *gl)
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

static void gl_check_fbo_dimension(gl_t *gl, unsigned i,
      GLuint fbo, GLuint texture, bool update_feedback)
{
   GLenum status;
   unsigned img_width, img_height, max, pow2_size;
   bool check_dimensions         = false;
   struct gfx_fbo_rect *fbo_rect = &gl->fbo_rect[i];

   if (!fbo_rect)
      return;

   check_dimensions = 
      (fbo_rect->max_img_width > fbo_rect->width) ||
      (fbo_rect->max_img_height > fbo_rect->height);

   if (!check_dimensions)
      return;

   /* Check proactively since we might suddently 
    * get sizes of tex_w width or tex_h height. */
   img_width             = fbo_rect->max_img_width;
   img_height            = fbo_rect->max_img_height;
   max                   = img_width > img_height ? img_width : img_height;
   pow2_size             = next_pow2(max);

   fbo_rect->width = fbo_rect->height = pow2_size;

   {
      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, fbo);
      glBindTexture(GL_TEXTURE_2D, texture);

      glTexImage2D(GL_TEXTURE_2D,
            0, RARCH_GL_INTERNAL_FORMAT32,
            fbo_rect->width,
            fbo_rect->height,
            0, RARCH_GL_TEXTURE_TYPE32,
            RARCH_GL_FORMAT32, NULL);

      glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            texture, 0);

      status = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);
      if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
         RARCH_WARN("Failed to reinitialize FBO texture.\n");
   }

   /* Update feedback texture in-place so we avoid having to 
    * juggle two different fbo_rect structs since they get updated here. */
   if (update_feedback)
   {
      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo_feedback);
      glBindTexture(GL_TEXTURE_2D, gl->fbo_feedback_texture);

      glTexImage2D(GL_TEXTURE_2D,
            0, RARCH_GL_INTERNAL_FORMAT32,
            fbo_rect->width,
            fbo_rect->height,
            0, RARCH_GL_TEXTURE_TYPE32,
            RARCH_GL_FORMAT32, NULL);

      glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            gl->fbo_feedback_texture, 0);

      status = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);

      if (status == RARCH_GL_FRAMEBUFFER_COMPLETE)
      {
         /* Make sure the feedback textures are cleared 
          * so we don't feedback noise. */
         glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
         glClear(GL_COLOR_BUFFER_BIT);
      }
      else
      {
         RARCH_WARN("Failed to reinitialize FBO texture.\n");
      }
   }

   RARCH_LOG("[GL]: Recreating FBO texture #%d: %ux%u\n",
         i, fbo_rect->width, fbo_rect->height);
}

/* On resize, we might have to recreate our FBOs 
 * due to "Viewport" scale, and set a new viewport. */

static void gl_check_fbo_dimensions(gl_t *gl)
{
   int i;

   /* Check if we have to recreate our FBO textures. */
   for (i = 0; i < gl->fbo_pass; i++)
   {
      bool update_feedback = gl->fbo_feedback_enable 
         && (unsigned)i == gl->fbo_feedback_pass;
      gl_check_fbo_dimension(gl, i, gl->fbo[i],
            gl->fbo_texture[i], update_feedback);
   }
}

static void gl_frame_fbo(gl_t *gl, uint64_t frame_count,
      const struct gfx_tex_info *tex_info,
      const struct gfx_tex_info *feedback_info)
{
   unsigned mip_level;
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   video_shader_ctx_params_t params;
   video_shader_ctx_info_t shader_info;
   unsigned width, height;
   const struct gfx_fbo_rect *prev_rect;
   struct gfx_tex_info *fbo_info;
   struct gfx_tex_info fbo_tex_info[GFX_MAX_SHADERS];
   int i;
   GLfloat xamt, yamt;
   unsigned fbo_tex_info_cnt = 0;
   GLfloat fbo_tex_coords[8] = {0.0f};

   video_driver_get_size(&width, &height);

   /* Render the rest of our passes. */
   gl->coords.tex_coord = fbo_tex_coords;

   /* Calculate viewports, texture coordinates etc,
    * and render all passes from FBOs, to another FBO. */
   for (i = 1; i < gl->fbo_pass; i++)
   {
      video_shader_ctx_mvp_t mvp;
      video_shader_ctx_coords_t coords;
      video_shader_ctx_params_t params;
      const struct gfx_fbo_rect *rect = &gl->fbo_rect[i];

      prev_rect = &gl->fbo_rect[i - 1];
      fbo_info  = &fbo_tex_info[i - 1];

      xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
      yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

      set_texture_coords(fbo_tex_coords, xamt, yamt);

      fbo_info->tex           = gl->fbo_texture[i - 1];
      fbo_info->input_size[0] = prev_rect->img_width;
      fbo_info->input_size[1] = prev_rect->img_height;
      fbo_info->tex_size[0]   = prev_rect->width;
      fbo_info->tex_size[1]   = prev_rect->height;
      memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
      fbo_tex_info_cnt++;

      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo[i]);

      shader_info.data = gl;
      shader_info.idx  = i + 1;

      video_shader_driver_ctl(SHADER_CTL_USE, &shader_info);
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

      mip_level = i + 1;

      if (video_shader_driver_ctl(SHADER_CTL_MIPMAP_INPUT, &mip_level))
         glGenerateMipmap(GL_TEXTURE_2D);

      glClear(GL_COLOR_BUFFER_BIT);

      /* Render to FBO with certain size. */
      gl_set_viewport(gl, rect->img_width, rect->img_height, true, false);

      params.data          = gl;
      params.width         = prev_rect->img_width;
      params.height        = prev_rect->img_height;
      params.tex_width     = prev_rect->width;
      params.tex_height    = prev_rect->height;
      params.out_width     = gl->vp.width;
      params.out_height    = gl->vp.height;
      params.frame_counter = (unsigned int)frame_count;
      params.info          = tex_info;
      params.prev_info     = gl->prev_info;
      params.feedback_info = feedback_info;
      params.fbo_info      = fbo_tex_info;
      params.fbo_info_cnt  = fbo_tex_info_cnt;

      video_shader_driver_ctl(SHADER_CTL_SET_PARAMS, &params);

      gl->coords.vertices = 4;

      coords.handle_data  = NULL;
      coords.data         = &gl->coords;

      video_shader_driver_ctl(SHADER_CTL_SET_COORDS, &coords);

      mvp.data = gl;
      mvp.matrix = &gl->mvp;

      video_shader_driver_ctl(SHADER_CTL_SET_MVP, &mvp);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }

#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (gl->has_srgb_fbo)
      glDisable(GL_FRAMEBUFFER_SRGB);
#endif

   /* Render our last FBO texture directly to screen. */
   prev_rect = &gl->fbo_rect[gl->fbo_pass - 1];
   xamt      = (GLfloat)prev_rect->img_width / prev_rect->width;
   yamt      = (GLfloat)prev_rect->img_height / prev_rect->height;

   set_texture_coords(fbo_tex_coords, xamt, yamt);

   /* Push final FBO to list. */
   fbo_info = &fbo_tex_info[gl->fbo_pass - 1];

   fbo_info->tex           = gl->fbo_texture[gl->fbo_pass - 1];
   fbo_info->input_size[0] = prev_rect->img_width;
   fbo_info->input_size[1] = prev_rect->img_height;
   fbo_info->tex_size[0]   = prev_rect->width;
   fbo_info->tex_size[1]   = prev_rect->height;
   memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
   fbo_tex_info_cnt++;

   /* Render our FBO texture to back buffer. */
   gl_bind_backbuffer();

   shader_info.data = gl;
   shader_info.idx  = gl->fbo_pass + 1;

   video_shader_driver_ctl(SHADER_CTL_USE, &shader_info);

   glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

   mip_level = gl->fbo_pass + 1;

   if (video_shader_driver_ctl(SHADER_CTL_MIPMAP_INPUT, &mip_level))
      glGenerateMipmap(GL_TEXTURE_2D);

   glClear(GL_COLOR_BUFFER_BIT);
   gl_set_viewport(gl, width, height, false, true);

   params.data          = gl;
   params.width         = prev_rect->img_width;
   params.height        = prev_rect->img_height;
   params.tex_width     = prev_rect->width;
   params.tex_height    = prev_rect->height;
   params.out_width     = gl->vp.width;
   params.out_height    = gl->vp.height;
   params.frame_counter = (unsigned int)frame_count;
   params.info          = tex_info;
   params.prev_info     = gl->prev_info;
   params.feedback_info = feedback_info;
   params.fbo_info      = fbo_tex_info;
   params.fbo_info_cnt  = fbo_tex_info_cnt;

   video_shader_driver_ctl(SHADER_CTL_SET_PARAMS, &params);

   gl->coords.vertex = gl->vertex_ptr;

   gl->coords.vertices = 4;

   coords.handle_data = NULL;
   coords.data        = &gl->coords;

   video_shader_driver_ctl(SHADER_CTL_SET_COORDS, &coords);

   mvp.data = gl;
   mvp.matrix = &gl->mvp;

   video_shader_driver_ctl(SHADER_CTL_SET_MVP, &mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_info.coord;
}
#endif

static void gl_update_input_size(gl_t *gl, unsigned width,
      unsigned height, unsigned pitch, bool clear)
{
   GLfloat xamt, yamt;
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
               video_pixel_get_alignment(width * sizeof(uint32_t)));
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

   if (!set_coords)
      return;

   xamt = (GLfloat)width  / gl->tex_w;
   yamt = (GLfloat)height / gl->tex_h;
   set_texture_coords(gl->tex_info.coord, xamt, yamt);
}

/* It is *much* faster (order of magnitude on my setup)
 * to use a custom SIMD-optimized conversion routine 
 * than letting GL do it. */
#if !defined(HAVE_PSGL) && !defined(HAVE_OPENGLES2)
static INLINE void gl_convert_frame_rgb16_32(gl_t *gl, void *output,
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
static INLINE void gl_convert_frame_argb8888_abgr8888(gl_t *gl,
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

static void gl_init_textures_reference(gl_t *gl, unsigned i,
      GLenum internal_fmt, GLenum texture_fmt, GLenum texture_type)
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
   if (gl->egl_images)
      return;

   glTexImage2D(GL_TEXTURE_2D,
         0, internal_fmt, gl->tex_w, gl->tex_h, 0, texture_type,
         texture_fmt, gl->empty_buf ? gl->empty_buf : NULL);
#endif
}

static void gl_init_textures(gl_t *gl, const video_info_t *video)
{
   unsigned i;
   GLenum internal_fmt, texture_type = 0, texture_fmt = 0;

   (void)texture_type;
   (void)texture_fmt;

#if defined(HAVE_EGL) && defined(HAVE_OPENGLES2)
   /* Use regular textures if we use HW render. */
   gl->egl_images = !gl->hw_render_use && gl_check_eglimage_proc() &&
      gfx_ctx_ctl(GFX_CTL_IMAGE_BUFFER_INIT, (void*)video);
#else
   (void)video;
#endif

#ifdef HAVE_PSGL
   if (!gl->pbo)
      glGenBuffers(1, &gl->pbo);

   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
         gl->tex_w * gl->tex_h * gl->base_size * gl->textures, 
         NULL, GL_STREAM_DRAW);
#endif

   internal_fmt = gl->internal_fmt;
#ifndef HAVE_PSGL
   texture_type = gl->texture_type;
   texture_fmt  = gl->texture_fmt;
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
      gl_init_textures_reference(gl, i, internal_fmt,
            texture_fmt, texture_type);

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static INLINE void gl_copy_frame(gl_t *gl, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   static struct retro_perf_counter copy_frame = {0};

   rarch_perf_init(&copy_frame, "copy_frame");
   retro_perf_start(&copy_frame);

#if defined(HAVE_OPENGLES2)
#if defined(HAVE_EGL)
   if (gl->egl_images)
   {
      gfx_ctx_image_t img_info;
      bool new_egl    = false;
      EGLImageKHR img = 0;

      img_info.frame  = frame;
      img_info.width  = width;
      img_info.height = height;
      img_info.pitch  = pitch;
      img_info.index  = gl->tex_index;
      img_info.rgb32  = (gl->base_size == 4);
      img_info.handle = &img;

      new_egl = gfx_ctx_ctl(GFX_CTL_IMAGE_BUFFER_WRITE, &img_info);

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
      bool use_rgba = video_driver_ctl(RARCH_DISPLAY_CTL_SUPPORTS_RGBA, NULL);

      glPixelStorei(GL_UNPACK_ALIGNMENT, 
            video_pixel_get_alignment(width * gl->base_size));

      /* Fallback for GLES devices without GL_BGRA_EXT. */
      if (gl->base_size == 4 && use_rgba)
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
         unsigned pitch_width   = pitch / gl->base_size;

         if (width != pitch_width)
         {
            /* Slow path - conv_buffer is preallocated 
             * just in case we hit this path. */

            unsigned h;
            const unsigned line_bytes = width * gl->base_size;
            uint8_t *dst              = (uint8_t*)gl->conv_buffer;
            const uint8_t *src        = (const uint8_t*)frame;

            for (h = 0; h < height; h++, src += pitch, dst += line_bytes)
               memcpy(dst, src, line_bytes);

            data_buf                  = gl->conv_buffer;
         }

         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, data_buf);         
      }
   }
#elif defined(HAVE_PSGL)
   {
      unsigned h;
      size_t buffer_addr        = gl->tex_w * gl->tex_h * 
         gl->tex_index * gl->base_size;
      size_t buffer_stride      = gl->tex_w * gl->base_size;
      const uint8_t *frame_copy = frame;
      size_t frame_copy_size    = width * gl->base_size;

      uint8_t *buffer = (uint8_t*)glMapBuffer(
            GL_TEXTURE_REFERENCE_BUFFER_SCE, GL_READ_WRITE) + buffer_addr;
      for (h = 0; h < height; h++, buffer += buffer_stride, frame_copy += pitch)
         memcpy(buffer, frame_copy, frame_copy_size);

      glUnmapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE);
   }
#else
   {
      const GLvoid *data_buf = frame;
      glPixelStorei(GL_UNPACK_ALIGNMENT, video_pixel_get_alignment(pitch));

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
   }
#endif
   retro_perf_stop(&copy_frame);
}

static INLINE void gl_set_prev_texture(gl_t *gl,
      const struct gfx_tex_info *tex_info)
{
   memmove(gl->prev_info + 1, gl->prev_info,
         sizeof(*tex_info) * (gl->textures - 1));
   memcpy(&gl->prev_info[0], tex_info,
         sizeof(*tex_info));

#ifdef HAVE_FBO
   /* Implement feedback by swapping out FBO/textures 
    * for FBO pass #N and feedbacks. */
   if (gl->fbo_feedback_enable)
   {
      GLuint tmp_fbo = gl->fbo_feedback;
      GLuint tmp_tex = gl->fbo_feedback_texture;
      gl->fbo_feedback = gl->fbo[gl->fbo_feedback_pass];
      gl->fbo_feedback_texture = gl->fbo_texture[gl->fbo_feedback_pass];
      gl->fbo[gl->fbo_feedback_pass] = tmp_fbo;
      gl->fbo_texture[gl->fbo_feedback_pass] = tmp_tex;
   }
#endif
}

static INLINE void gl_set_shader_viewport(gl_t *gl, unsigned idx)
{
   unsigned width, height;
   video_shader_ctx_info_t shader_info;

   video_driver_get_size(&width, &height);

   shader_info.data = gl;
   shader_info.idx  = idx;

   video_shader_driver_ctl(SHADER_CTL_USE, &shader_info);
   gl_set_viewport(gl, width, height, false, true);
}

#if defined(HAVE_GL_ASYNC_READBACK) && defined(HAVE_MENU)
static void gl_pbo_async_readback(gl_t *gl)
{
   static struct retro_perf_counter async_readback = {0};

   glBindBuffer(GL_PIXEL_PACK_BUFFER,
         gl->pbo_readback[gl->pbo_readback_index++]);
   gl->pbo_readback_index &= 3;

   /* 4 frames back, we can readback. */
   gl->pbo_readback_valid[gl->pbo_readback_index] = true;

   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glPixelStorei(GL_PACK_ALIGNMENT,
         video_pixel_get_alignment(gl->vp.width * sizeof(uint32_t)));

   /* Read asynchronously into PBO buffer. */
   rarch_perf_init(&async_readback, "async_readback");
   retro_perf_start(&async_readback);
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
   retro_perf_stop(&async_readback);

   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}
#endif

#if defined(HAVE_MENU)
static INLINE void gl_draw_texture(gl_t *gl)
{
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   video_shader_ctx_info_t shader_info;
   unsigned width, height;
   GLfloat color[16];

   color[ 0] = 1.0f;
   color[ 1] = 1.0f;
   color[ 2] = 1.0f;
   color[ 3] = gl->menu_texture_alpha;
   color[ 4] = 1.0f;
   color[ 5] = 1.0f;
   color[ 6] = 1.0f;
   color[ 7] = gl->menu_texture_alpha;
   color[ 8] = 1.0f;
   color[ 9] = 1.0f;
   color[10] = 1.0f;
   color[11] = gl->menu_texture_alpha;
   color[12] = 1.0f;
   color[13] = 1.0f;
   color[14] = 1.0f;
   color[15] = gl->menu_texture_alpha;

   if (!gl->menu_texture)
      return;

   video_driver_get_size(&width, &height);

   gl->coords.vertex    = vertexes_flipped;
   gl->coords.tex_coord = tex_coords;
   gl->coords.color     = color;
   glBindTexture(GL_TEXTURE_2D, gl->menu_texture);

   shader_info.data = gl;
   shader_info.idx  = GL_SHADER_STOCK_BLEND;

   video_shader_driver_ctl(SHADER_CTL_USE, &shader_info);

   gl->coords.vertices  = 4;

   coords.handle_data   = NULL;
   coords.data          = &gl->coords;

   video_shader_driver_ctl(SHADER_CTL_SET_COORDS, &coords);

   mvp.data   = gl;
   mvp.matrix = &gl->mvp_no_rot;

   video_shader_driver_ctl(SHADER_CTL_SET_MVP, &mvp);

   glEnable(GL_BLEND);

   if (gl->menu_texture_full_screen)
   {
      glViewport(0, 0, width, height);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   }
   else
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glDisable(GL_BLEND);

   gl->coords.vertex    = gl->vertex_ptr;
   gl->coords.tex_coord = gl->tex_info.coord;
   gl->coords.color     = gl->white_color_ptr;
}
#endif

#ifdef HAVE_OVERLAY
static void gl_render_overlay(gl_t *gl);
#endif

static bool gl_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count,
      unsigned pitch, const char *msg)
{
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   video_shader_ctx_params_t params;
   unsigned width, height;
   struct gfx_tex_info feedback_info;
   video_shader_ctx_info_t shader_info;
   static struct retro_perf_counter frame_run = {0};
   gl_t                            *gl = (gl_t*)data;
   settings_t                *settings = config_get_ptr();

   rarch_perf_init(&frame_run, "frame_run");
   retro_perf_start(&frame_run);

   if (!gl)
      return false;

   video_driver_get_size(&width, &height);

   context_bind_hw_render(gl, false);

#ifndef HAVE_OPENGLES
   if (gl->core_context)
      glBindVertexArray(gl->vao);
#endif


   shader_info.data = gl;
   shader_info.idx  = 1;

   video_shader_driver_ctl(SHADER_CTL_USE, &shader_info);

#ifdef IOS
   /* Apparently the viewport is lost each frame, thanks Apple. */
   gl_set_viewport(gl, width, height, false, true);
#endif

#ifdef HAVE_FBO
   /* Render to texture in first pass. */
   if (gl->fbo_inited)
   {
      gl_compute_fbo_geometry(gl, frame_width, frame_height,
            gl->vp_out_width, gl->vp_out_height);
      gl_start_frame_fbo(gl);
   }
#endif

   if (gl->should_resize)
   {
      gfx_ctx_mode_t mode;

      gl->should_resize = false;

      mode.width  = width;
      mode.height = height;

      gfx_ctx_ctl(GFX_CTL_SET_RESIZE, &mode);

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
         gl_set_viewport(gl, width, height, false, true);
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
         gl_update_input_size(gl, frame_width, frame_height, pitch, true);
         gl_copy_frame(gl, frame, frame_width, frame_height, pitch);
      }

      /* No point regenerating mipmaps 
       * if there are no new frames. */
      if (gl->tex_mipmap)
         glGenerateMipmap(GL_TEXTURE_2D);
   }

   /* Have to reset rendering state which libretro core 
    * could easily have overridden. */
#ifdef HAVE_FBO
   if (gl->hw_render_fbo_init)
   {
      gl_update_input_size(gl, frame_width, frame_height, pitch, false);
      if (!gl->fbo_inited)
      {
         gl_bind_backbuffer();
         gl_set_viewport(gl, width, height, false, true);
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
   gl->tex_info.input_size[0] = frame_width;
   gl->tex_info.input_size[1] = frame_height;
   gl->tex_info.tex_size[0]   = gl->tex_w;
   gl->tex_info.tex_size[1]   = gl->tex_h;

   feedback_info              = gl->tex_info;

#ifdef HAVE_FBO
   if (gl->fbo_feedback_enable)
   {
      const struct gfx_fbo_rect *rect = &gl->fbo_rect[gl->fbo_feedback_pass];
      GLfloat xamt = (GLfloat)rect->img_width / rect->width;
      GLfloat yamt = (GLfloat)rect->img_height / rect->height;

      feedback_info.tex           = gl->fbo_feedback_texture;
      feedback_info.input_size[0] = rect->img_width;
      feedback_info.input_size[1] = rect->img_height;
      feedback_info.tex_size[0]   = rect->width;
      feedback_info.tex_size[1]   = rect->height;

      set_texture_coords(feedback_info.coord, xamt, yamt);
   }
#endif

   glClear(GL_COLOR_BUFFER_BIT);

   params.data          = gl;
   params.width         = frame_width;
   params.height        = frame_height;
   params.tex_width     = gl->tex_w;
   params.tex_height    = gl->tex_h;
   params.out_width     = gl->vp.width;
   params.out_height    = gl->vp.height;
   params.frame_counter = (unsigned int)frame_count;
   params.info          = &gl->tex_info;
   params.prev_info     = gl->prev_info;
   params.feedback_info = &feedback_info;
   params.fbo_info      = NULL;
   params.fbo_info_cnt  = 0;

   video_shader_driver_ctl(SHADER_CTL_SET_PARAMS, &params);

   gl->coords.vertices = 4;
   coords.handle_data   = NULL;
   coords.data          = &gl->coords;

   video_shader_driver_ctl(SHADER_CTL_SET_COORDS, &coords);

   mvp.data             = gl;
   mvp.matrix           = &gl->mvp;

   video_shader_driver_ctl(SHADER_CTL_SET_MVP, &mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#ifdef HAVE_FBO
   if (gl->fbo_inited)
      gl_frame_fbo(gl, frame_count, &gl->tex_info, &feedback_info);
#endif

   gl_set_prev_texture(gl, &gl->tex_info);

#if defined(HAVE_MENU)
   if (gl->menu_texture_enable)
   {
      menu_driver_ctl(RARCH_MENU_CTL_FRAME, NULL);

      if (gl->menu_texture_enable)
         gl_draw_texture(gl);
   }
#endif

   if (font_driver_has_render_msg() && msg)
      font_driver_render_msg(NULL, msg, NULL);

#ifdef HAVE_OVERLAY
   gl_render_overlay(gl);
#endif

   gfx_ctx_ctl(GFX_CTL_UPDATE_WINDOW_TITLE, NULL);

   retro_perf_stop(&frame_run);

#ifdef HAVE_FBO
   /* Reset state which could easily mess up libretro core. */
   if (gl->hw_render_fbo_init)
   {
      shader_info.data = gl;
      shader_info.idx  = 0;

      video_shader_driver_ctl(SHADER_CTL_USE, &shader_info);

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
   if (
         settings->video.black_frame_insertion
         && !input_driver_ctl(RARCH_INPUT_CTL_IS_NONBLOCK_STATE, NULL)
         && !runloop_ctl(RUNLOOP_CTL_IS_SLOWMOTION, NULL)
         && !runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL))
   {
      gfx_ctx_ctl(GFX_CTL_SWAP_BUFFERS, NULL);
      glClear(GL_COLOR_BUFFER_BIT);
   }

   gfx_ctx_ctl(GFX_CTL_SWAP_BUFFERS, NULL);

#ifdef HAVE_GL_SYNC
   if (settings->video.hard_sync && gl->have_sync)
   {
      static struct retro_perf_counter gl_fence = {0};

      rarch_perf_init(&gl_fence, "gl_fence");
      retro_perf_start(&gl_fence);
      glClear(GL_COLOR_BUFFER_BIT);
      gl->fences[gl->fence_count++] = 
         glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

      while (gl->fence_count > settings->video.hard_sync_frames)
      {
         glClientWaitSync(gl->fences[0],
               GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
         glDeleteSync(gl->fences[0]);

         gl->fence_count--;
         memmove(gl->fences, gl->fences + 1,
               gl->fence_count * sizeof(GLsync));
      }

      retro_perf_stop(&gl_fence);
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
   gl->overlay_tex          = NULL;
   gl->overlay_vertex_coord = NULL;
   gl->overlay_tex_coord    = NULL;
   gl->overlay_color_coord  = NULL;
   gl->overlays             = 0;
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

   if (font_driver_has_render_msg())
      font_driver_free(NULL);
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
   gl_deinit_hw_render(gl);
#endif

#ifndef HAVE_OPENGLES
   if (gl->core_context)
   {
      glBindVertexArray(0);
      glDeleteVertexArrays(1, &gl->vao);
   }
#endif

   gfx_ctx_ctl(GFX_CTL_FREE, NULL);

   free(gl->empty_buf);
   free(gl->conv_buffer);
   free(gl);
}

static void gl_set_nonblock_state(void *data, bool state)
{
   unsigned interval;
   gl_t             *gl        = (gl_t*)data;
   settings_t        *settings = config_get_ptr();

   if (!gl)
      return;

   RARCH_LOG("[GL]: VSync => %s\n", state ? "off" : "on");

   context_bind_hw_render(gl, false);

   interval = state ? 0 : settings->video.swap_interval;

   gfx_ctx_ctl(GFX_CTL_SWAP_INTERVAL, &interval);
   context_bind_hw_render(gl, true);
}

static bool resolve_extensions(gl_t *gl, const char *context_ident)
{
   const char *vendor   = (const char*)glGetString(GL_VENDOR);
   const char *renderer = (const char*)glGetString(GL_RENDERER);
   const char *version  = (const char*)glGetString(GL_VERSION);
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();
#if defined(HAVE_GL_SYNC) || defined(HAVE_FBO)
   settings_t *settings = config_get_ptr();
#endif
   unsigned major = 0, minor = 0;
    
   (void)vendor;
   (void)renderer;
   (void)version;
   (void)hw_render;
#ifndef HAVE_OPENGLES
   gl->core_context     = 
      (hw_render->context_type == RETRO_HW_CONTEXT_OPENGL_CORE);
   
   if (gl->core_context)
   {
      RARCH_LOG("[GL]: Using Core GL context.\n");
      if (!gl_init_vao(gl))
      {
         RARCH_ERR("[GL]: Failed to initialize VAOs.\n");
         return false;
      }
   }

   if (version && sscanf(version, "%u.%u", &major, &minor) != 2)
         major = minor = 0;

   /* GL_RGB565 internal format support.
    * Even though ES2 support is claimed, the format 
    * is not supported on older ATI catalyst drivers.
    *
    * The speed gain from using GL_RGB565 is worth 
    * adding some workarounds for.
    */

   if (vendor && renderer && (strstr(vendor, "ATI") || strstr(renderer, "ATI")))
      RARCH_LOG("[GL]: ATI card detected, skipping check for GL_RGB565 support.\n");
   else
      gl->have_es2_compat = gl_query_extension(gl, "ARB_ES2_compatibility");

   if (major >= 3)
      gl->have_full_npot_support = true;
   else
   {  /* try to detect actual npot support. might fail for older cards. */
      GLint max_texture_size = 0;
      GLint max_native_instr = 0;
      bool arb_npot = gl_query_extension(gl, "ARB_texture_non_power_of_two");
      bool arb_frag_program = gl_query_extension(gl, "ARB_fragment_program");

      glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

#ifdef GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB
      if (arb_frag_program && glGetProgramivARB)
         glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
               GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, &max_native_instr);
#endif

      gl->have_full_npot_support = arb_npot && arb_frag_program &&
            (max_texture_size >= 8192) && (max_native_instr >= 4096);
   }
#endif

#ifdef HAVE_GL_SYNC
   gl->have_sync = gl_check_sync_proc(gl);
   if (gl->have_sync && settings->video.hard_sync)
      RARCH_LOG("[GL]: Using ARB_sync to reduce latency.\n");
#endif

   video_driver_ctl(RARCH_DISPLAY_CTL_UNSET_RGBA, NULL);
#if defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES2)
   bool gles3          = false;

   if (version && sscanf(version, "OpenGL ES %u.%u", &major, &minor) != 2)
         major = minor = 0;

   if (major >= 3)
   {
      RARCH_LOG("[GL]: GLES3 or newer detected. Auto-enabling some extensions.\n");
      gles3 = true;
   }

   if (gles3)
      gl->have_full_npot_support = true;
   else
   {
      bool arb_npot = gl_query_extension(gl, "ARB_texture_non_power_of_two");
      bool oes_npot = gl_query_extension(gl, "OES_texture_npot");
      gl->have_full_npot_support = arb_npot || oes_npot;
   }
#endif

#ifdef HAVE_OPENGLES2


   /* There are both APPLE and EXT variants. */
   /* Videocore hardware supports BGRA8888 extension, but
    * should be purposefully avoided. */
   if (gl_query_extension(gl, "BGRA8888") && !strstr(renderer, "VideoCore"))
      RARCH_LOG("[GL]: BGRA8888 extension found for GLES.\n");
   else
   {
      video_driver_ctl(RARCH_DISPLAY_CTL_SET_RGBA, NULL);
      RARCH_WARN("[GL]: GLES implementation does not have BGRA8888 extension.\n"
                 "32-bit path will require conversion.\n");
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
   gl->has_fp_fbo = gl->core_context || gl_query_extension(gl, "ARB_texture_float");
   gl->has_srgb_fbo = gl->core_context || 
      (gl_query_extension(gl, "EXT_texture_sRGB")
       && gl_query_extension(gl, "ARB_framebuffer_sRGB"));
#endif
#endif

#ifdef HAVE_FBO
   if (settings->video.force_srgb_disable)
      gl->has_srgb_fbo = false;
#endif

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
         size_t i;
         struct string_list *list = string_split(ext, " ");

         for (i = 0; i < list->size; i++)
            RARCH_LOG("\t%s\n", list->elems[i].data);
         string_list_free(list);
      }
   }
#endif

   return true;
}

static INLINE void gl_set_texture_fmts(gl_t *gl, bool rgb32)
{
   gl->internal_fmt = RARCH_GL_INTERNAL_FORMAT16;
   gl->texture_type = RARCH_GL_TEXTURE_TYPE16;
   gl->texture_fmt  = RARCH_GL_FORMAT16;
   gl->base_size    = sizeof(uint16_t);

   if (rgb32)
   {
      bool use_rgba    = 
         video_driver_ctl(RARCH_DISPLAY_CTL_SUPPORTS_RGBA, NULL);

      gl->internal_fmt = RARCH_GL_INTERNAL_FORMAT32;
      gl->texture_type = RARCH_GL_TEXTURE_TYPE32;
      gl->texture_fmt  = RARCH_GL_FORMAT32;
      gl->base_size    = sizeof(uint32_t);

      if (use_rgba)
      {
         gl->internal_fmt = GL_RGBA;
         gl->texture_type = GL_RGBA;
      }
   }
#ifndef HAVE_OPENGLES
   else if (gl->have_es2_compat)
   {
      RARCH_LOG("[GL]: Using GL_RGB565 for texture uploads.\n");
      gl->internal_fmt = RARCH_GL_INTERNAL_FORMAT16_565;
      gl->texture_type = RARCH_GL_TEXTURE_TYPE16_565;
      gl->texture_fmt  = RARCH_GL_FORMAT16_565;
   }
#endif
}

#ifdef HAVE_GL_ASYNC_READBACK
static void gl_init_pbo_readback(gl_t *gl)
{
   unsigned i;
   struct scaler_ctx *scaler = NULL;
   settings_t *settings      = config_get_ptr();
   bool *recording_enabled   = recording_is_enabled();

   (void)scaler;

   /* Only bother with this if we're doing GPU recording.
    * Check recording_is_enabled() and not 
    * driver.recording_data, because recording is 
    * not initialized yet.
    */
   gl->pbo_readback_enable = settings->video.gpu_record 
      && *recording_enabled;

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
   scaler              = &gl->pbo_readback_scaler;
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
      RARCH_ERR("Failed to initialize pixel conversion for PBO.\n");
      glDeleteBuffers(4, gl->pbo_readback);
   }
#endif
}
#endif

static const gfx_ctx_driver_t *gl_get_context(gl_t *gl)
{
   const struct retro_hw_render_callback *cb = 
      (const struct retro_hw_render_callback*)video_driver_callback();
   unsigned major = cb->version_major;
   unsigned minor = cb->version_minor;
   settings_t *settings = config_get_ptr();
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
    
   (void)api_name;

   gl->shared_context_use = settings->video.shared_context
      && cb->context_type != RETRO_HW_CONTEXT_NONE;

   return gfx_ctx_init_first(gl, settings->video.context_driver,
         api, major, minor, gl->shared_context_use);
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
   const char      *src = NULL;
   const char **typestr = NULL;
   gl_t             *gl = (gl_t*)userParam; /* Useful for debugger. */

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
   gfx_ctx_mode_t mode;
   gfx_ctx_input_t inp;
   unsigned interval, mip_level;
   video_shader_ctx_wrap_t wrap_info;
   video_shader_ctx_filter_t shader_filter;
   video_shader_ctx_info_t shader_info;
   video_shader_ctx_ident_t ident_info;
   unsigned win_width, win_height, temp_width = 0, temp_height = 0;
   bool force_smooth                  = false;
   const char *vendor                 = NULL;
   const char *renderer               = NULL;
   const char *version                = NULL;
   struct retro_hw_render_callback *hw_render = NULL;
   settings_t *settings               = config_get_ptr();
   gl_t *gl                           = (gl_t*)calloc(1, sizeof(gl_t));
   const gfx_ctx_driver_t *ctx_driver = gl_get_context(gl);
   if (!gl || !ctx_driver)
      goto error;

   gfx_ctx_ctl(GFX_CTL_SET, (void*)ctx_driver);

   gl->video_info        = *video;

   RARCH_LOG("Found GL context: %s\n", ctx_driver->ident);

   gfx_ctx_ctl(GFX_CTL_GET_VIDEO_SIZE, &mode);

   gl->full_x  = mode.width;
   gl->full_y  = mode.height;
   mode.width  = 0;
   mode.height = 0;

   RARCH_LOG("Detecting screen resolution %ux%u.\n", gl->full_x, gl->full_y);

   interval = video->vsync ? settings->video.swap_interval : 0;

   gfx_ctx_ctl(GFX_CTL_SWAP_INTERVAL, &interval);

   win_width  = video->width;
   win_height = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = gl->full_x;
      win_height = gl->full_y;
   }

   mode.width      = win_width;
   mode.height     = win_height;
   mode.fullscreen = video->fullscreen;

   if (!gfx_ctx_ctl(GFX_CTL_SET_VIDEO_MODE, &mode))
      goto error;

   /* Clear out potential error flags in case we use cached context. */
   glGetError(); 

   vendor   = (const char*)glGetString(GL_VENDOR);
   renderer = (const char*)glGetString(GL_RENDERER);
   version  = (const char*)glGetString(GL_VERSION);

   RARCH_LOG("[GL]: Vendor: %s, Renderer: %s.\n", vendor, renderer);
   RARCH_LOG("[GL]: Version: %s.\n", version);

   if (!string_is_empty(version))
      sscanf(version, "%d.%d", &gl->version_major, &gl->version_minor);

#ifndef RARCH_CONSOLE
   rglgen_resolve_symbols(ctx_driver->get_proc_address);
#endif

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   if (!resolve_extensions(gl, ctx_driver->ident))
      goto error;

#ifdef GL_DEBUG
   gl_begin_debug(gl);
#endif

   gl->vsync      = video->vsync;
   gl->fullscreen = video->fullscreen;

   mode.width     = 0;
   mode.height    = 0;

   gfx_ctx_ctl(GFX_CTL_GET_VIDEO_SIZE, &mode);

   temp_width     = mode.width;
   temp_height    = mode.height;
   mode.width     = 0;
   mode.height    = 0;
   
   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   video_driver_get_size(&temp_width, &temp_height);

   RARCH_LOG("GL: Using resolution %ux%u\n", temp_width, temp_height);

   if (gl->full_x || gl->full_y)
   {
      /* We got bogus from gfx_ctx_get_video_size. Replace. */
      gl->full_x = temp_width;
      gl->full_y = temp_height;
   }

   hw_render         = video_driver_callback();
   gl->vertex_ptr    = hw_render->bottom_left_origin 
      ? vertexes : vertexes_flipped;

   /* Better pipelining with GPU due to synchronous glSubTexImage.
    * Multiple async PBOs would be an alternative,
    * but still need multiple textures with PREV.
    */
   gl->textures      = 4;
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
   gl_glsl_set_get_proc_address(ctx_driver->get_proc_address);
   gl_glsl_set_context_type(gl->core_context,
         hw_render->version_major, hw_render->version_minor);
#endif

   if (!video_shader_driver_ctl(SHADER_CTL_INIT_FIRST, NULL))
      goto error;

   video_shader_driver_ctl(SHADER_CTL_GET_IDENT, &ident_info);

   RARCH_LOG("[GL]: Default shader backend found: %s.\n", ident_info.ident);

   if (!gl_shader_init(gl))
   {
      RARCH_ERR("[GL]: Shader initialization failed.\n");
      goto error;
   }

   {
      unsigned minimum;
      video_shader_ctx_texture_t texture_info;

      video_shader_driver_ctl(SHADER_CTL_GET_PREV_TEXTURES, &texture_info);

      minimum          = texture_info.id;
      gl->textures     = MAX(minimum + 1, gl->textures);
   }

   if (!video_shader_driver_ctl(SHADER_CTL_INFO, &shader_info))
      goto error;

   RARCH_LOG("[GL]: Using %u textures.\n", gl->textures);
   RARCH_LOG("[GL]: Loaded %u program(s).\n",
         shader_info.num);

   gl->tex_w = gl->tex_h = (RARCH_SCALE_BASE * video->input_scale);
   gl->keep_aspect     = video->force_aspect;

   /* Apparently need to set viewport for passes 
    * when we aren't using FBOs. */
   gl_set_shader_viewport(gl, 0);
   gl_set_shader_viewport(gl, 1);

   mip_level            = 1;
   gl->tex_mipmap       = video_shader_driver_ctl(SHADER_CTL_MIPMAP_INPUT,
         &mip_level);

   shader_filter.index  = 1;
   shader_filter.smooth = &force_smooth;

   if (video_shader_driver_ctl(SHADER_CTL_FILTER_TYPE, &shader_filter))
      gl->tex_min_filter = gl->tex_mipmap ? (force_smooth ? 
            GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) 
         : (force_smooth ? GL_LINEAR : GL_NEAREST);
   else
      gl->tex_min_filter = gl->tex_mipmap ? 
         (video->smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) 
         : (video->smooth ? GL_LINEAR : GL_NEAREST);
   
   gl->tex_mag_filter = min_filter_to_mag(gl->tex_min_filter);

   wrap_info.idx      = 1;

   video_shader_driver_ctl(SHADER_CTL_WRAP_TYPE, &wrap_info);

   gl->wrap_mode      = gl_wrap_type_to_enum(wrap_info.type);

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
   gl->empty_buf             = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

#if !defined(HAVE_PSGL)
   gl->conv_buffer           = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

   if (!gl->conv_buffer)
      goto error;
#endif

   gl_init_textures(gl, video);
   gl_init_textures_data(gl);

#ifdef HAVE_FBO
   gl_init_fbo(gl, gl->tex_w, gl->tex_h);

   if (gl->hw_render_use && 
         !gl_init_hw_render(gl, gl->tex_w, gl->tex_h))
      goto error;
#endif

   inp.input      = input;
   inp.input_data = input_data;

   gfx_ctx_ctl(GFX_CTL_INPUT_DRIVER, &inp);
   
   if (settings->video.font_enable)
   {
      if (!font_driver_init_first(NULL, NULL, gl, *settings->video.font_path 
            ? settings->video.font_path : NULL, settings->video.font_size, false,
            FONT_DRIVER_RENDER_OPENGL_API))
         RARCH_ERR("[GL]: Failed to initialize font renderer.\n");
   }

#ifdef HAVE_GL_ASYNC_READBACK
   gl_init_pbo_readback(gl);
#endif

   if (!gl_check_error())
      goto error;

   context_bind_hw_render(gl, true);
   return gl;

error:
   gfx_ctx_ctl(GFX_CTL_DESTROY, NULL);
   free(gl);
   return NULL;
}

static bool gl_alive(void *data)
{
   gfx_ctx_size_t size_data;
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool ret             = false;
   bool quit            = false;
   bool resize          = false;
   gl_t         *gl     = (gl_t*)data;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   size_data.quit       = &quit;
   size_data.resize     = &resize;
   size_data.width      = &temp_width;
   size_data.height     = &temp_height;

   if (gfx_ctx_ctl(GFX_CTL_CHECK_WINDOW, &size_data))
   {
      if (quit)
         gl->quitting = true;
      else if (resize)
         gl->should_resize = true;

      ret = !gl->quitting;
   }

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return ret;
}

static bool gl_focus(void *data)
{
   return gfx_ctx_ctl(GFX_CTL_FOCUS, NULL);
}

static bool gl_suppress_screensaver(void *data, bool enable)
{
   bool enabled = enable;
   return gfx_ctx_ctl(GFX_CTL_SUPPRESS_SCREENSAVER, &enabled);
}

static bool gl_has_windowed(void *data)
{
   return gfx_ctx_ctl(GFX_CTL_HAS_WINDOWED, NULL);
}

static void gl_update_tex_filter_frame(gl_t *gl)
{
   video_shader_ctx_wrap_t wrap_info;
   video_shader_ctx_filter_t shader_filter;
   unsigned i, mip_level;
   GLenum wrap_mode;
   GLuint new_filt;
   bool smooth          = false;
   settings_t *settings = config_get_ptr();

   if (!gl)
      return;

   context_bind_hw_render(gl, false);

   shader_filter.index  = 1;
   shader_filter.smooth = &smooth;

   if (!video_shader_driver_ctl(SHADER_CTL_FILTER_TYPE, &shader_filter))
      smooth = settings->video.smooth;

   mip_level             = 1;
   wrap_info.idx         = 1;

   video_shader_driver_ctl(SHADER_CTL_WRAP_TYPE, &wrap_info);

   wrap_mode             =  gl_wrap_type_to_enum(wrap_info.type);

   gl->tex_mipmap        = video_shader_driver_ctl(SHADER_CTL_MIPMAP_INPUT,
         &mip_level);

   gl->video_info.smooth = smooth;
   new_filt = gl->tex_mipmap ? (smooth ? 
         GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST) 
      : (smooth ? GL_LINEAR : GL_NEAREST);

   if (new_filt == gl->tex_min_filter && wrap_mode == gl->wrap_mode)
      return;

   gl->tex_min_filter    = new_filt;
   gl->tex_mag_filter    = min_filter_to_mag(gl->tex_min_filter);
   gl->wrap_mode         = wrap_mode;

   for (i = 0; i < gl->textures; i++)
   {
      if (!gl->texture[i])
         continue;

      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->wrap_mode);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->wrap_mode);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_mag_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_min_filter);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   context_bind_hw_render(gl, true);
}

static bool gl_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_GLSL) || defined(HAVE_CG)
   video_shader_ctx_init_t init_data;
   const shader_backend_t *shader = NULL;
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
         shader = &gl_glsl_backend;
         break;
#endif

#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         shader = &gl_cg_backend;
         break;
#endif

      default:
         break;
   }

   if (!shader)
   {
      RARCH_ERR("[GL]: Cannot find shader core for path: %s.\n", path);
      context_bind_hw_render(gl, true);
      return false;
   }

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
#endif

   init_data.shader = shader;
   init_data.data   = gl;
   init_data.path   = path;

   if (!video_shader_driver_ctl(SHADER_CTL_INIT, &init_data))
   {
      init_data.path = NULL;

      video_shader_driver_ctl(SHADER_CTL_INIT, &init_data);

      RARCH_WARN("[GL]: Failed to set multipass shader. Falling back to stock.\n");

      context_bind_hw_render(gl, true);
      return false;
   }

   gl_update_tex_filter_frame(gl);

   if (shader)
   {
      unsigned textures;
      video_shader_ctx_texture_t texture_info;

      video_shader_driver_ctl(SHADER_CTL_GET_PREV_TEXTURES, &texture_info);

      textures = texture_info.id + 1;

      if (textures > gl->textures) /* Have to reinit a bit. */
      {
#if defined(HAVE_FBO)
         gl_deinit_hw_render(gl);
#endif

         glDeleteTextures(gl->textures, gl->texture);
#if defined(HAVE_PSGL)
         glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
         glDeleteBuffers(1, &gl->pbo);
#endif
         gl->textures = textures;
         RARCH_LOG("[GL]: Using %u textures.\n", gl->textures);
         gl->tex_index = 0;
         gl_init_textures(gl, &gl->video_info);
         gl_init_textures_data(gl);

#if defined(HAVE_FBO)
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
#if defined(_WIN32) && !defined(_XBOX)
   shader_dlg_params_reload();
#endif
   return true;
#else
   return false;
#endif
}

static void gl_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned width, height;
   unsigned top_y, top_dist;
   gl_t *gl         = (gl_t*)data;

   video_driver_get_size(&width, &height);

   *vp             = gl->vp;
   vp->full_width  = width;
   vp->full_height = height;

   /* Adjust as GL viewport is bottom-up. */
   top_y           = vp->y + vp->height;
   top_dist        = height - top_y;
   vp->y           = top_dist;
}

#ifdef NO_GL_READ_PIXELS
static bool gl_read_viewport(void *data, uint8_t *buffer)
{
   return false;
}
#else
static bool gl_read_viewport(void *data, uint8_t *buffer)
{
   static struct retro_perf_counter read_viewport = {0};
   unsigned                     num_pixels = 0;
   gl_t                                *gl = (gl_t*)data;

   if (!gl)
      return false;

   context_bind_hw_render(gl, false);

   rarch_perf_init(&read_viewport, "read_viewport");
   retro_perf_start(&read_viewport);

#ifdef HAVE_GL_ASYNC_READBACK
   if (gl->pbo_readback_enable)
   {
      const uint8_t *ptr  = NULL;

      /* Don't readback if we're in menu mode.
       * We haven't buffered up enough frames yet, come back later. */
      if (!gl->pbo_readback_valid[gl->pbo_readback_index]) 
         goto error;

      gl->pbo_readback_valid[gl->pbo_readback_index] = false;
      glBindBuffer(GL_PIXEL_PACK_BUFFER,
            gl->pbo_readback[gl->pbo_readback_index]);
#ifdef HAVE_OPENGLES3
      /* Slower path, but should work on all implementations at least. */
      num_pixels = gl->vp.width * gl->vp.height;
      ptr = (const uint8_t*)glMapBufferRange(GL_PIXEL_PACK_BUFFER,
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
         goto error;
      }
#else
      ptr = (const uint8_t*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
      if (!ptr)
      {
         RARCH_ERR("[GL]: Failed to map pixel unpack buffer.\n");
         goto error;
      }

      scaler_ctx_scale(&gl->pbo_readback_scaler, buffer, ptr);
#endif
      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   }
   else /* Use slow synchronous readbacks. Use this with plain screenshots 
           as we don't really care about performance in this case. */
#endif
   {
      unsigned i;
      uint8_t *dst = NULL;
      const uint8_t *src = NULL;

      /* GLES2 only guarantees GL_RGBA/GL_UNSIGNED_BYTE 
       * readbacks so do just that.
       * GLES2 also doesn't support reading back data 
       * from front buffer, so render a cached frame 
       * and have gl_frame() do the readback while it's 
       * in the back buffer.
       *
       * Keep codepath similar for GLES and desktop GL.
       */

      num_pixels = gl->vp.width * gl->vp.height;

      gl->readback_buffer_screenshot = malloc(num_pixels * sizeof(uint32_t));
      if (!gl->readback_buffer_screenshot)
      {
         retro_perf_stop(&read_viewport);
         goto error;
      }

      video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);

      dst = buffer;
      src = (const uint8_t*)gl->readback_buffer_screenshot;

      for (i = 0; i < num_pixels; i++, dst += 3, src += 4)
      {
         dst[0] = src[2]; /* RGBA -> BGR. */
         dst[1] = src[1];
         dst[2] = src[0];
      }

      free(gl->readback_buffer_screenshot);
      gl->readback_buffer_screenshot = NULL;
   }

   retro_perf_stop(&read_viewport);
   context_bind_hw_render(gl, true);
   return true;

error:
   context_bind_hw_render(gl, true);
   return false;
}
#endif

#if 0
#define READ_RAW_GL_FRAME_TEST
#endif

#if defined(READ_RAW_GL_FRAME_TEST)
static void* gl_read_frame_raw(void *data, unsigned *width_p,
unsigned *height_p, size_t *pitch_p)
{
   gl_t *gl             = (gl_t*)data;
   unsigned width       = gl->last_width[gl->tex_index];
   unsigned height      = gl->last_height[gl->tex_index];
   size_t pitch         = gl->tex_w * gl->base_size;
#ifdef HAVE_FBO
   void* buffer         = NULL;
#endif
   void* buffer_texture = NULL;

#ifdef HAVE_FBO
   if (gl->hw_render_use)
   {
      buffer = malloc(pitch * height);
      if (!buffer)
         return NULL;
   }
#endif
   buffer_texture = malloc(pitch * gl->tex_h);

   if (!buffer_texture)
   {
#ifdef HAVE_FBO
      if (buffer)
         free(buffer);
#endif
      return NULL;
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   glGetTexImage(GL_TEXTURE_2D, 0,
         gl->texture_type, gl->texture_fmt, buffer_texture);

   *width_p = width;
   *height_p = height;
   *pitch_p = pitch;

#ifdef HAVE_FBO
   if (gl->hw_render_use)
   {
      unsigned i;

      for(i = 0; i < height ; i++)
         memcpy((uint8_t*)buffer + i * pitch,
            (uint8_t*)buffer_texture + (height - 1 - i) * pitch, pitch);

      free(buffer_texture);
      return buffer;
   }
#endif
   return buffer_texture;
}
#endif

#ifdef HAVE_OVERLAY
static void gl_free_overlay(gl_t *gl);
static bool gl_overlay_load(void *data, 
      const void *image_data, unsigned num_images)
{
   unsigned i, j;
   gl_t *gl = (gl_t*)data;
   const struct texture_image *images = 
      (const struct texture_image*)image_data;

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

   gl->overlay_vertex_coord = (GLfloat*)
      calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_tex_coord    = (GLfloat*)
      calloc(2 * 4 * num_images, sizeof(GLfloat));
   gl->overlay_color_coord  = (GLfloat*)
      calloc(4 * 4 * num_images, sizeof(GLfloat));

   if (     !gl->overlay_vertex_coord 
         || !gl->overlay_tex_coord 
         || !gl->overlay_color_coord)
      return false;

   gl->overlays             = num_images;
   glGenTextures(num_images, gl->overlay_tex);

   for (i = 0; i < num_images; i++)
   {
      unsigned alignment = video_pixel_get_alignment(images[i].width 
            * sizeof(uint32_t));

      gl_load_texture_data(gl->overlay_tex[i],
            RARCH_WRAP_EDGE, TEXTURE_FILTER_LINEAR,
            alignment,
            images[i].width, images[i].height, images[i].pixels,
            sizeof(uint32_t));

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
   gl_t *gl     = (gl_t*)data;

   if (!gl)
      return;

   tex          = (GLfloat*)&gl->overlay_tex_coord[image * 8];

   if (!tex)
      return;

   tex[0]       = x;
   tex[1]       = y;
   tex[2]       = x + w;
   tex[3]       = y;
   tex[4]       = x;
   tex[5]       = y + h;
   tex[6]       = x + w;
   tex[7]       = y + h;
}

static void gl_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y,
      float w, float h)
{
   GLfloat *vertex = NULL;
   gl_t *gl        = (gl_t*)data;

   if (!gl)
      return;

   if (image > gl->overlays)
   {
      RARCH_ERR("Invalid overlay id: %u\n", image);
      return;
   }

   vertex          = (GLfloat*)&gl->overlay_vertex_coord[image * 8];

   /* Flipped, so we preserve top-down semantics. */
   y               = 1.0f - y;
   h               = -h;

   if (!vertex)
      return;

   vertex[0]       = x;
   vertex[1]       = y;
   vertex[2]       = x + w;
   vertex[3]       = y;
   vertex[4]       = x;
   vertex[5]       = y + h;
   vertex[6]       = x + w;
   vertex[7]       = y + h;
}

static void gl_overlay_enable(void *data, bool state)
{
   gl_t *gl           = (gl_t*)data;

   if (!gl)
      return;

   gl->overlay_enable = state;

   if (gl->fullscreen)
      gfx_ctx_ctl(GFX_CTL_SHOW_MOUSE, &state);
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
   gl_t *gl       = (gl_t*)data;
   if (!gl)
      return;

   color          = (GLfloat*)&gl->overlay_color_coord[image * 16];

   if (!color)
      return;

   color[ 0 + 3]  = mod;
   color[ 4 + 3]  = mod;
   color[ 8 + 3]  = mod;
   color[12 + 3]  = mod;
}

static void gl_render_overlay(gl_t *gl)
{
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   video_shader_ctx_info_t shader_info;
   unsigned i, width, height;

   if (!gl || !gl->overlay_enable)
      return;

   video_driver_get_size(&width, &height);

   glEnable(GL_BLEND);

   if (gl->overlay_full_screen)
      glViewport(0, 0, width, height);

   /* Ensure that we reset the attrib array. */
   shader_info.data = gl;
   shader_info.idx  = GL_SHADER_STOCK_BLEND;

   video_shader_driver_ctl(SHADER_CTL_USE, &shader_info);

   gl->coords.vertex    = gl->overlay_vertex_coord;
   gl->coords.tex_coord = gl->overlay_tex_coord;
   gl->coords.color     = gl->overlay_color_coord;
   gl->coords.vertices  = 4 * gl->overlays;

   coords.handle_data   = NULL;
   coords.data          = &gl->coords;

   video_shader_driver_ctl(SHADER_CTL_SET_COORDS, &coords);

   mvp.data             = gl;
   mvp.matrix           = &gl->mvp_no_rot;

   video_shader_driver_ctl(SHADER_CTL_SET_MVP, &mvp);

   for (i = 0; i < gl->overlays; i++)
   {
      if (!gl)
         return;
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
#endif

static retro_proc_address_t gl_get_proc_address(void *data, const char *sym)
{
   gfx_ctx_proc_address_t proc_address;

   proc_address.sym = sym;

   gfx_ctx_ctl(GFX_CTL_PROC_ADDRESS_GET, &proc_address);

   return proc_address.addr;
}

static void gl_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl_t *gl         = (gl_t*)data;
   enum rarch_display_ctl_state cmd = RARCH_DISPLAY_CTL_NONE;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         cmd = RARCH_DISPLAY_CTL_SET_VIEWPORT_SQUARE_PIXEL;
         break;

      case ASPECT_RATIO_CORE:
         cmd = RARCH_DISPLAY_CTL_SET_VIEWPORT_CORE;
         break;

      case ASPECT_RATIO_CONFIG:
         cmd = RARCH_DISPLAY_CTL_SET_VIEWPORT_CONFIG;
         break;

      default:
         break;
   }

   if (cmd != RARCH_DISPLAY_CTL_NONE)
      video_driver_ctl(cmd, NULL);

   video_driver_set_aspect_ratio_value(
         aspectratio_lut[aspect_ratio_idx].value);

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
   enum texture_filter_type menu_filter;
   settings_t *settings            = config_get_ptr();
   unsigned base_size              = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   gl_t *gl                        = (gl_t*)data;
   if (!gl)
      return;

   context_bind_hw_render(gl, false);

   menu_filter = settings->menu.linear_filter ? TEXTURE_FILTER_LINEAR : TEXTURE_FILTER_NEAREST;

   if (!gl->menu_texture)
      glGenTextures(1, &gl->menu_texture);


   gl_load_texture_data(gl->menu_texture,
         RARCH_WRAP_EDGE, menu_filter,
         video_pixel_get_alignment(width * base_size),
         width, height, frame,
         base_size);

   gl->menu_texture_alpha = alpha;
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   context_bind_hw_render(gl, true);
}

static void gl_set_texture_enable(void *data, bool state, bool full_screen)
{
   gl_t *gl                     = (gl_t*)data;

   if (!gl)
      return;

   gl->menu_texture_enable      = state;
   gl->menu_texture_full_screen = full_screen;
}
#endif

static void gl_apply_state_changes(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (gl)
      gl->should_resize = true;
}

#ifdef HAVE_MENU
static void gl_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font)
{
   font_driver_render_msg(font, msg, params);
}

static void gl_show_mouse(void *data, bool state)
{
   gfx_ctx_ctl(GFX_CTL_SHOW_MOUSE, &state);
}

static struct video_shader *gl_get_current_shader(void *data)
{
   video_shader_ctx_t shader_info;

   video_shader_driver_ctl(SHADER_CTL_DIRECT_GET_CURRENT_SHADER, &shader_info);

   return shader_info.data;
}
#endif

static void gl_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_size_t size_data;
   size_data.width  = width;
   size_data.height = height;
   gfx_ctx_ctl(GFX_CTL_GET_VIDEO_OUTPUT_SIZE, &size_data);
}

static void gl_get_video_output_prev(void *data)
{
   gfx_ctx_ctl(GFX_CTL_GET_VIDEO_OUTPUT_PREV, NULL);
}

static void gl_get_video_output_next(void *data)
{
   gfx_ctx_ctl(GFX_CTL_GET_VIDEO_OUTPUT_NEXT, NULL);
}

void gl_load_texture_data(uint32_t id_data,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size)
{
   GLint mag_filter, min_filter;
   bool want_mipmap = false;
   bool use_rgba    = video_driver_ctl(RARCH_DISPLAY_CTL_SUPPORTS_RGBA, NULL);
   bool rgb32       = (base_size == (sizeof(uint32_t)));
   GLenum wrap      = gl_wrap_type_to_enum(wrap_type);
   GLuint id        = (GLuint)id_data;

   glBindTexture(GL_TEXTURE_2D, id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    
#if defined(HAVE_OPENGLES2) || defined(HAVE_PSGL) || defined(OSX_PPC)
   if (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR)
       filter_type = TEXTURE_FILTER_LINEAR;
   if (filter_type == TEXTURE_FILTER_MIPMAP_NEAREST)
       filter_type = TEXTURE_FILTER_NEAREST;
#endif

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         min_filter = GL_LINEAR_MIPMAP_NEAREST;
         mag_filter = GL_LINEAR;
         want_mipmap = true;
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
         min_filter = GL_NEAREST_MIPMAP_NEAREST;
         mag_filter = GL_NEAREST;
         want_mipmap = true;
         break;
      case TEXTURE_FILTER_NEAREST:
         min_filter = GL_NEAREST;
         mag_filter = GL_NEAREST;
         break;
      case TEXTURE_FILTER_LINEAR:
      default:
         min_filter = GL_LINEAR;
         mag_filter = GL_LINEAR;
         break;
   }

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);

   glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
   glTexImage2D(GL_TEXTURE_2D,
         0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
         width, height, 0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
         (rgb32) ? RARCH_GL_FORMAT32 : GL_UNSIGNED_SHORT_4_4_4_4, frame);

   if (want_mipmap)
      glGenerateMipmap(GL_TEXTURE_2D);
}

static void video_texture_load_gl(
      struct texture_image *ti,
      enum texture_filter_type filter_type,
      uintptr_t *id)
{
   /* Generate the OpenGL texture object */
   glGenTextures(1, (GLuint*)id);
   gl_load_texture_data((GLuint)*id, 
         RARCH_WRAP_EDGE, filter_type,
         4 /* TODO/FIXME - dehardcode */,
         ti->width, ti->height, ti->pixels,
         sizeof(uint32_t) /* TODO/FIXME - dehardcode */
         );
}

static int video_texture_load_wrap_gl_mipmap(void *data)
{
   uintptr_t id = 0;

   if (!data)
      return 0;
   video_texture_load_gl((struct texture_image*)data,
         TEXTURE_FILTER_MIPMAP_LINEAR, &id);
   return id;
}

static int video_texture_load_wrap_gl(void *data)
{
   uintptr_t id = 0;

   if (!data)
      return 0;
   video_texture_load_gl((struct texture_image*)data,
         TEXTURE_FILTER_LINEAR, &id);
   return id;
}

static uintptr_t gl_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;

   if (threaded)
   {
      custom_command_method_t func = video_texture_load_wrap_gl;

      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            func = video_texture_load_wrap_gl_mipmap;
            break;
         default:
            break;
      }
      return rarch_threaded_video_texture_load(data, func);
   }

   video_texture_load_gl((struct texture_image*)data, filter_type, &id);
   return id;
}

static void gl_unload_texture(void *data, uintptr_t id)
{
   GLuint glid;
   if (!id)
      return;

   glid = (GLuint)id;
   glDeleteTextures(1, &glid);
}

static const video_poke_interface_t gl_poke_interface = {
   gl_load_texture,
   gl_unload_texture,
   gl_set_video_mode,
   NULL,
   gl_get_video_output_size,
   gl_get_video_output_prev,
   gl_get_video_output_next,
#ifdef HAVE_FBO
   gl_get_current_framebuffer,
#else
   NULL,
#endif
   gl_get_proc_address,
   gl_set_aspect_ratio,
   gl_apply_state_changes,
#if defined(HAVE_MENU)
   gl_set_texture_frame,
   gl_set_texture_enable,
   gl_set_osd_msg,
   gl_show_mouse,
#else
   NULL,
   NULL,
   NULL,
   NULL,
#endif

   NULL,
#ifdef HAVE_MENU
   gl_get_current_shader,
#endif
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
   gl_suppress_screensaver,
   gl_has_windowed,

   gl_set_shader,

   gl_free,
   "gl",

   gl_set_viewport,
   gl_set_rotation,

   gl_viewport_info,

   gl_read_viewport,
#if defined(READ_RAW_GL_FRAME_TEST)
   gl_read_frame_raw,
#else
   NULL,
#endif

#ifdef HAVE_OVERLAY
   gl_get_overlay_interface,
#endif
   gl_get_poke_interface,
   gl_wrap_type_to_enum,
};

