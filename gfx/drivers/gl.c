/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

/* Middle of the road OpenGL driver.
 *
 * Minimum version (desktop): OpenGL 2.0+
 * Minimum version (mobile) : OpenGLES 2.0+
 */

#ifdef _MSC_VER
#if defined(HAVE_OPENGLES)
#pragma comment(lib, "libGLESv2")
#else
#pragma comment(lib, "opengl32")
#endif
#endif

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <gfx/math/matrix_4x4.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <string/stdstring.h>
#include <libretro.h>

#include <gfx/gl_capabilities.h>
#include <gfx/video_frame.h>
#include <glsym/glsym.h>

#include "../../configuration.h"
#include "../../dynamic.h"

#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../common/gl_common.h"

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../font_driver.h"

#ifdef HAVE_GLSL
#include "../drivers_shader/shader_glsl.h"
#endif

#ifdef GL_DEBUG
#include <lists/string_list.h>

#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2)
#define HAVE_GL_DEBUG_ES
#endif
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#ifdef HAVE_MENU_WIDGETS
#include "../../menu/widgets/menu_widgets.h"
#endif
#endif

#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#endif

#define set_texture_coords(coords, xamt, yamt) \
   coords[2] = xamt; \
   coords[6] = xamt; \
   coords[5] = yamt; \
   coords[7] = yamt

static const shader_backend_t *gl2_shader_ctx_drivers[] = {
#ifdef HAVE_GLSL
   &gl_glsl_backend,
#endif
#ifdef HAVE_CG
   &gl_cg_backend,
#endif
   NULL
};

static struct video_ortho default_ortho = {0, 1, 0, 1, -1, 1};

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

static bool gl_shared_context_use = false;

#define gl2_context_bind_hw_render(gl, enable) \
   if (gl_shared_context_use) \
      gl->ctx_driver->bind_hw_render(gl->ctx_data, enable)

#define MAX_FENCES 4

#if !defined(HAVE_PSGL)

#ifndef HAVE_GL_SYNC
#define HAVE_GL_SYNC
#endif

#endif

#ifdef HAVE_GL_SYNC
#if defined(HAVE_OPENGLES2)
typedef struct __GLsync *GLsync;
#endif
#endif

typedef struct gl2_renderchain_data
{
   bool egl_images;
   bool has_fp_fbo;
   bool has_srgb_fbo_gles3;
   bool has_srgb_fbo;
   bool hw_render_depth_init;

   int fbo_pass;

   GLuint vao;
   GLuint fbo[GFX_MAX_SHADERS];
   GLuint fbo_texture[GFX_MAX_SHADERS];
   GLuint hw_render_depth[GFX_MAX_TEXTURES];

   unsigned fence_count;

#ifdef HAVE_GL_SYNC
   GLsync fences[MAX_FENCES];
#endif

   struct gfx_fbo_scale fbo_scale[GFX_MAX_SHADERS];
} gl2_renderchain_data_t;

#if (!defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3))
#ifdef GL_PIXEL_PACK_BUFFER
#define HAVE_GL_ASYNC_READBACK
#endif
#endif

#define set_texture_coords(coords, xamt, yamt) \
   coords[2] = xamt; \
   coords[6] = xamt; \
   coords[5] = yamt; \
   coords[7] = yamt

#if defined(HAVE_PSGL)
#define gl2_fb_texture_2d(a, b, c, d, e) glFramebufferTexture2DOES(a, b, c, d, e)
#define gl2_check_fb_status(target) glCheckFramebufferStatusOES(target)
#define gl2_gen_fb(n, ids)   glGenFramebuffersOES(n, ids)
#define gl2_delete_fb(n, fb) glDeleteFramebuffersOES(n, fb)
#define gl2_bind_fb(id)      glBindFramebufferOES(RARCH_GL_FRAMEBUFFER, id)
#define gl2_gen_rb           glGenRenderbuffersOES
#define gl2_bind_rb          glBindRenderbufferOES
#define gl2_fb_rb            glFramebufferRenderbufferOES
#define gl2_rb_storage       glRenderbufferStorageOES
#define gl2_delete_rb        glDeleteRenderbuffersOES

#elif (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
#define gl2_fb_texture_2d(a, b, c, d, e) glFramebufferTexture2DEXT(a, b, c, d, e)
#define gl2_check_fb_status(target) glCheckFramebufferStatusEXT(target)
#define gl2_gen_fb(n, ids)   glGenFramebuffersEXT(n, ids)
#define gl2_delete_fb(n, fb) glDeleteFramebuffersEXT(n, fb)
#define gl2_bind_fb(id)      glBindFramebufferEXT(RARCH_GL_FRAMEBUFFER, id)
#define gl2_gen_rb           glGenRenderbuffersEXT
#define gl2_bind_rb          glBindRenderbufferEXT
#define gl2_fb_rb            glFramebufferRenderbufferEXT
#define gl2_rb_storage       glRenderbufferStorageEXT
#define gl2_delete_rb        glDeleteRenderbuffersEXT

#else

#define gl2_fb_texture_2d(a, b, c, d, e) glFramebufferTexture2D(a, b, c, d, e)
#define gl2_check_fb_status(target) glCheckFramebufferStatus(target)
#define gl2_gen_fb(n, ids)   glGenFramebuffers(n, ids)
#define gl2_delete_fb(n, fb) glDeleteFramebuffers(n, fb)
#define gl2_bind_fb(id)      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, id)
#define gl2_gen_rb           glGenRenderbuffers
#define gl2_bind_rb          glBindRenderbuffer
#define gl2_fb_rb            glFramebufferRenderbuffer
#define gl2_rb_storage       glRenderbufferStorage
#define gl2_delete_rb        glDeleteRenderbuffers

#endif

#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE     0x9117
#endif

#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT        0x00000001
#endif

/* Prototypes */
#ifdef IOS
/* There is no default frame buffer on iOS. */
void cocoagl_bind_game_view_fbo(void);
#define gl2_renderchain_bind_backbuffer() cocoagl_bind_game_view_fbo()
#else
#define gl2_renderchain_bind_backbuffer() gl2_bind_fb(0)
#endif

static bool gl2_shader_info(gl_t *gl,
      video_shader_ctx_info_t *shader_info)
{
   if (!shader_info)
      return false;

   shader_info->num = gl->shader->num_shaders(gl->shader_data);

   return true;
}

static bool gl2_shader_scale(gl_t *gl,
      video_shader_ctx_scale_t *scaler)
{
   if (!scaler || !scaler->scale)
      return false;

   scaler->scale->valid = false;

   gl->shader->shader_scale(gl->shader_data,
         scaler->idx, scaler->scale);
   return true;
}

static void gl2_size_format(GLint* internalFormat)
{
#ifndef HAVE_PSGL
   switch (*internalFormat)
   {
      case GL_RGB:
         /* FIXME: PS3 does not support this, neither does it have GL_RGB565_OES. */
         *internalFormat = GL_RGB565;
         break;
      case GL_RGBA:
#ifdef HAVE_OPENGLES2
         *internalFormat = GL_RGBA8_OES;
#else
         *internalFormat = GL_RGBA8;
#endif
         break;
   }
#endif
}

/* This function should only be used without mipmaps
   and when data == NULL */
static void gl2_load_texture_image(GLenum target,
      GLint level,
      GLint internalFormat,
      GLsizei width,
      GLsizei height,
      GLint border,
      GLenum format,
      GLenum type,
      const GLvoid * data)
{
#if !defined(HAVE_PSGL) && !defined(ORBIS)
#ifdef HAVE_OPENGLES2
   enum gl_capability_enum cap = GL_CAPS_TEX_STORAGE_EXT;
#else
   enum gl_capability_enum cap = GL_CAPS_TEX_STORAGE;
#endif

   if (gl_check_capability(cap) && internalFormat != GL_BGRA_EXT)
   {
      gl2_size_format(&internalFormat);
#ifdef HAVE_OPENGLES2
      glTexStorage2DEXT(target, 1, internalFormat, width, height);
#else
      glTexStorage2D   (target, 1, internalFormat, width, height);
#endif
   }
   else
#endif
   {
#ifdef HAVE_OPENGLES
      if (gl_check_capability(GL_CAPS_GLES3_SUPPORTED))
#endif
         gl2_size_format(&internalFormat);
      glTexImage2D(target, level, internalFormat, width,
            height, border, format, type, data);
   }
}

static bool gl2_recreate_fbo(
      struct video_fbo_rect *fbo_rect,
      GLuint fbo,
      GLuint* texture
      )
{
   gl2_bind_fb(fbo);
   glDeleteTextures(1, texture);
   glGenTextures(1, texture);
   glBindTexture(GL_TEXTURE_2D, *texture);
   gl2_load_texture_image(GL_TEXTURE_2D,
         0, RARCH_GL_INTERNAL_FORMAT32,
         fbo_rect->width,
         fbo_rect->height,
         0, RARCH_GL_TEXTURE_TYPE32,
         RARCH_GL_FORMAT32, NULL);

   gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER,
         RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
         *texture, 0);

   if (gl2_check_fb_status(RARCH_GL_FRAMEBUFFER)
         == RARCH_GL_FRAMEBUFFER_COMPLETE)
      return true;

   RARCH_WARN("Failed to reinitialize FBO texture.\n");
   return false;
}

static void gl2_set_projection(gl_t *gl,
      struct video_ortho *ortho, bool allow_rotate)
{
   math_matrix_4x4 rot;

   /* Calculate projection. */
   matrix_4x4_ortho(gl->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      gl->mvp = gl->mvp_no_rot;
      return;
   }

   matrix_4x4_rotate_z(rot, M_PI * gl->rotation / 180.0f);
   matrix_4x4_multiply(gl->mvp, rot, gl->mvp_no_rot);
}

static void gl2_set_viewport(gl_t *gl,
      video_frame_info_t *video_info,
      unsigned viewport_width,
      unsigned viewport_height,
      bool force_full, bool allow_rotate)
{
   gfx_ctx_aspect_t aspect_data;
   int x                    = 0;
   int y                    = 0;
   float device_aspect      = (float)viewport_width / viewport_height;
   unsigned height          = video_info->height;

   aspect_data.aspect       = &device_aspect;
   aspect_data.width        = viewport_width;
   aspect_data.height       = viewport_height;

   video_context_driver_translate_aspect(&aspect_data);

   if (video_info->scale_integer && !force_full)
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
      if (video_info->aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         /* GL has bottom-left origin viewport. */
         x      = video_info->custom_vp_x;
         y      = height - video_info->custom_vp_y - video_info->custom_vp_height;
         viewport_width  = video_info->custom_vp_width;
         viewport_height = video_info->custom_vp_height;
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
   gl2_set_projection(gl, &default_ortho, allow_rotate);

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

static void gl2_renderchain_render(
      gl_t *gl,
      gl2_renderchain_data_t *chain,
      video_frame_info_t *video_info,
      uint64_t frame_count,
      const struct video_tex_info *tex_info,
      const struct video_tex_info *feedback_info)
{
   int i;
   video_shader_ctx_params_t params;
   static GLfloat fbo_tex_coords[8]       = {0.0f};
   struct video_tex_info fbo_tex_info[GFX_MAX_SHADERS];
   struct video_tex_info *fbo_info        = NULL;
   const struct video_fbo_rect *prev_rect = NULL;
   GLfloat xamt                           = 0.0f;
   GLfloat yamt                           = 0.0f;
   unsigned mip_level                     = 0;
   unsigned fbo_tex_info_cnt              = 0;
   unsigned width                         = video_info->width;
   unsigned height                        = video_info->height;

   /* Render the rest of our passes. */
   gl->coords.tex_coord      = fbo_tex_coords;

   /* Calculate viewports, texture coordinates etc,
    * and render all passes from FBOs, to another FBO. */
   for (i = 1; i < chain->fbo_pass; i++)
   {
      const struct video_fbo_rect *rect = &gl->fbo_rect[i];

      prev_rect = &gl->fbo_rect[i - 1];
      fbo_info  = &fbo_tex_info[i - 1];

      xamt      = (GLfloat)prev_rect->img_width / prev_rect->width;
      yamt      = (GLfloat)prev_rect->img_height / prev_rect->height;

      set_texture_coords(fbo_tex_coords, xamt, yamt);

      fbo_info->tex           = chain->fbo_texture[i - 1];
      fbo_info->input_size[0] = prev_rect->img_width;
      fbo_info->input_size[1] = prev_rect->img_height;
      fbo_info->tex_size[0]   = prev_rect->width;
      fbo_info->tex_size[1]   = prev_rect->height;
      memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
      fbo_tex_info_cnt++;

      gl2_bind_fb(chain->fbo[i]);

      gl->shader->use(gl, gl->shader_data,
            i + 1, true);

      glBindTexture(GL_TEXTURE_2D, chain->fbo_texture[i - 1]);

      mip_level = i + 1;

      if (gl->shader->mipmap_input(gl->shader_data, mip_level)
            && gl->have_mipmap)
         glGenerateMipmap(GL_TEXTURE_2D);

      glClear(GL_COLOR_BUFFER_BIT);

      /* Render to FBO with certain size. */
      gl2_set_viewport(gl, video_info,
            rect->img_width, rect->img_height, true, false);

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

      gl->shader->set_params(&params, gl->shader_data);

      gl->coords.vertices = 4;

      gl->shader->set_coords(gl->shader_data, &gl->coords);
      gl->shader->set_mvp(gl->shader_data, &gl->mvp);

      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }

#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (chain->has_srgb_fbo)
      glDisable(GL_FRAMEBUFFER_SRGB);
#endif

   /* Render our last FBO texture directly to screen. */
   prev_rect = &gl->fbo_rect[chain->fbo_pass - 1];
   xamt      = (GLfloat)prev_rect->img_width / prev_rect->width;
   yamt      = (GLfloat)prev_rect->img_height / prev_rect->height;

   set_texture_coords(fbo_tex_coords, xamt, yamt);

   /* Push final FBO to list. */
   fbo_info                = &fbo_tex_info[chain->fbo_pass - 1];

   fbo_info->tex           = chain->fbo_texture[chain->fbo_pass - 1];
   fbo_info->input_size[0] = prev_rect->img_width;
   fbo_info->input_size[1] = prev_rect->img_height;
   fbo_info->tex_size[0]   = prev_rect->width;
   fbo_info->tex_size[1]   = prev_rect->height;
   memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
   fbo_tex_info_cnt++;

   /* Render our FBO texture to back buffer. */
   gl2_renderchain_bind_backbuffer();

   gl->shader->use(gl, gl->shader_data,
         chain->fbo_pass + 1, true);

   glBindTexture(GL_TEXTURE_2D, chain->fbo_texture[chain->fbo_pass - 1]);

   mip_level = chain->fbo_pass + 1;

   if (
         gl->shader->mipmap_input(gl->shader_data, mip_level) &&
         gl->have_mipmap)
      glGenerateMipmap(GL_TEXTURE_2D);

   glClear(GL_COLOR_BUFFER_BIT);
   gl2_set_viewport(gl, video_info,
         width, height, false, true);

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

   gl->shader->set_params(&params, gl->shader_data);

   gl->coords.vertex    = gl->vertex_ptr;

   gl->coords.vertices  = 4;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_info.coord;
}

static void gl2_renderchain_deinit_fbo(gl_t *gl,
      gl2_renderchain_data_t *chain)
{
   if (gl)
   {
      if (gl->fbo_feedback)
         gl2_delete_fb(1, &gl->fbo_feedback);
      if (gl->fbo_feedback_texture)
         glDeleteTextures(1, &gl->fbo_feedback_texture);

      gl->fbo_inited           = false;
      gl->fbo_feedback_enable  = false;
      gl->fbo_feedback_pass    = 0;
      gl->fbo_feedback_texture = 0;
      gl->fbo_feedback         = 0;
   }

   if (chain)
   {
      gl2_delete_fb(chain->fbo_pass, chain->fbo);
      glDeleteTextures(chain->fbo_pass, chain->fbo_texture);

      memset(chain->fbo_texture, 0, sizeof(chain->fbo_texture));
      memset(chain->fbo,         0, sizeof(chain->fbo));

      chain->fbo_pass          = 0;
   }
}

static void gl2_renderchain_deinit_hw_render(
      gl_t *gl,
      gl2_renderchain_data_t *chain)
{
   if (!gl)
      return;

   gl2_context_bind_hw_render(gl, true);

   if (gl->hw_render_fbo_init)
      gl2_delete_fb(gl->textures, gl->hw_render_fbo);
   if (chain->hw_render_depth_init)
      gl2_delete_rb(gl->textures, chain->hw_render_depth);
   gl->hw_render_fbo_init = false;

   gl2_context_bind_hw_render(gl, false);
}

static bool gl2_create_fbo_targets(gl_t *gl, gl2_renderchain_data_t *chain)
{
   unsigned i;

   glBindTexture(GL_TEXTURE_2D, 0);
   gl2_gen_fb(chain->fbo_pass, chain->fbo);

   for (i = 0; i < (unsigned)chain->fbo_pass; i++)
   {
      GLenum status;

      gl2_bind_fb(chain->fbo[i]);
      gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, chain->fbo_texture[i], 0);

      status = gl2_check_fb_status(RARCH_GL_FRAMEBUFFER);
      if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
         goto error;
   }

   if (gl->fbo_feedback_texture)
   {
      GLenum status;

      gl2_gen_fb(1, &gl->fbo_feedback);
      gl2_bind_fb(gl->fbo_feedback);
      gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            gl->fbo_feedback_texture, 0);

      status = gl2_check_fb_status(RARCH_GL_FRAMEBUFFER);
      if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
         goto error;

      /* Make sure the feedback textures are cleared
       * so we don't feedback noise. */
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);
   }

   return true;

error:
   gl2_delete_fb(chain->fbo_pass, chain->fbo);
   if (gl->fbo_feedback)
      gl2_delete_fb(1, &gl->fbo_feedback);
   RARCH_ERR("[GL]: Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
   return false;
}

static unsigned gl2_wrap_type_to_enum(enum gfx_wrap_type type)
{
   switch (type)
   {
#ifndef HAVE_OPENGLES
      case RARCH_WRAP_BORDER: /* GL_CLAMP_TO_BORDER: Available since GL 1.3 */
         return GL_CLAMP_TO_BORDER;
#else
      case RARCH_WRAP_BORDER:
#endif
      case RARCH_WRAP_EDGE:
         return GL_CLAMP_TO_EDGE;
      case RARCH_WRAP_REPEAT:
         return GL_REPEAT;
      case RARCH_WRAP_MIRRORED_REPEAT:
         return GL_MIRRORED_REPEAT;
      default:
	 break;
   }

   return 0;
}

static GLenum gl2_min_filter_to_mag(GLenum type)
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

static void gl2_create_fbo_texture(gl_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned i, GLuint texture)
{
   GLenum mag_filter, wrap_enum;
   enum gfx_wrap_type wrap_type;
   bool fp_fbo                   = false;
   bool smooth                   = false;
   settings_t *settings          = config_get_ptr();
   GLuint base_filt              = settings->bools.video_smooth ? GL_LINEAR : GL_NEAREST;
   GLuint base_mip_filt          = settings->bools.video_smooth ?
      GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
   unsigned mip_level            = i + 2;
   bool mipmapped                = gl->shader->mipmap_input(gl->shader_data, mip_level);
   GLenum min_filter             = mipmapped ? base_mip_filt : base_filt;

   if (gl->shader->filter_type(gl->shader_data,
            i + 2, &smooth))
   {
      min_filter = mipmapped ? (smooth ?
            GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
         : (smooth ? GL_LINEAR : GL_NEAREST);
   }

   mag_filter = gl2_min_filter_to_mag(min_filter);

   wrap_type  = gl->shader->wrap_type(gl->shader_data, i + 2);

   wrap_enum  = gl2_wrap_type_to_enum(wrap_type);

   gl_bind_texture(texture, wrap_enum, mag_filter, min_filter);

   fp_fbo   = chain->fbo_scale[i].fp_fbo;

   if (fp_fbo)
   {
      if (!chain->has_fp_fbo)
         RARCH_ERR("[GL]: Floating-point FBO was requested, but is not supported. Falling back to UNORM. Result may band/clip/etc.!\n");
   }

#if !defined(HAVE_OPENGLES2)
   if (fp_fbo && chain->has_fp_fbo)
   {
      RARCH_LOG("[GL]: FBO pass #%d is floating-point.\n", i);
      gl2_load_texture_image(GL_TEXTURE_2D, 0, GL_RGBA32F,
         gl->fbo_rect[i].width, gl->fbo_rect[i].height,
         0, GL_RGBA, GL_FLOAT, NULL);
   }
   else
#endif
   {
#ifndef HAVE_OPENGLES
      bool srgb_fbo = chain->fbo_scale[i].srgb_fbo;

      if (!fp_fbo && srgb_fbo)
      {
         if (!chain->has_srgb_fbo)
               RARCH_ERR("[GL]: sRGB FBO was requested, but it is not supported. Falling back to UNORM. Result may have banding!\n");
      }

      if (settings->bools.video_force_srgb_disable)
         srgb_fbo = false;

      if (srgb_fbo && chain->has_srgb_fbo)
      {
         RARCH_LOG("[GL]: FBO pass #%d is sRGB.\n", i);
#ifdef HAVE_OPENGLES2
         /* EXT defines are same as core GLES3 defines,
          * but GLES3 variant requires different arguments. */
         glTexImage2D(GL_TEXTURE_2D,
               0, GL_SRGB_ALPHA_EXT,
               gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
               chain->has_srgb_fbo_gles3 ? GL_RGBA : GL_SRGB_ALPHA_EXT,
               GL_UNSIGNED_BYTE, NULL);
#else
         gl2_load_texture_image(GL_TEXTURE_2D,
            0, GL_SRGB8_ALPHA8,
            gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#endif
      }
      else
#endif
      {
#if defined(HAVE_OPENGLES2) || defined(HAVE_PSGL)
         glTexImage2D(GL_TEXTURE_2D,
               0, GL_RGBA,
               gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#else
         /* Avoid potential performance
          * reductions on particular platforms. */
         gl2_load_texture_image(GL_TEXTURE_2D,
            0, RARCH_GL_INTERNAL_FORMAT32,
            gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0,
            RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, NULL);
#endif
      }
   }
}

static void gl2_create_fbo_textures(gl_t *gl,
      gl2_renderchain_data_t *chain)
{
   int i;

   glGenTextures(chain->fbo_pass, chain->fbo_texture);

   for (i = 0; i < chain->fbo_pass; i++)
      gl2_create_fbo_texture(gl,
            (gl2_renderchain_data_t*)gl->renderchain_data,
            i, chain->fbo_texture[i]);

   if (gl->fbo_feedback_enable)
   {
      glGenTextures(1, &gl->fbo_feedback_texture);
      gl2_create_fbo_texture(gl,
            (gl2_renderchain_data_t*)gl->renderchain_data,
            gl->fbo_feedback_pass, gl->fbo_feedback_texture);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

/* Compute FBO geometry.
 * When width/height changes or window sizes change,
 * we have to recalculate geometry of our FBO. */

static void gl2_renderchain_recompute_pass_sizes(
      gl_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
{
   unsigned i;
   bool size_modified       = false;
   GLint max_size           = 0;
   unsigned last_width      = width;
   unsigned last_height     = height;
   unsigned last_max_width  = gl->tex_w;
   unsigned last_max_height = gl->tex_h;

   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);

   /* Calculate viewports for FBOs. */
   for (i = 0; i < (unsigned)chain->fbo_pass; i++)
   {
      struct video_fbo_rect  *fbo_rect   = &gl->fbo_rect[i];
      struct gfx_fbo_scale *fbo_scale    = &chain->fbo_scale[i];

      switch (fbo_scale->type_x)
      {
         case RARCH_SCALE_INPUT:
            fbo_rect->img_width      = fbo_scale->scale_x * last_width;
            fbo_rect->max_img_width  = last_max_width     * fbo_scale->scale_x;
            break;

         case RARCH_SCALE_ABSOLUTE:
            fbo_rect->img_width      = fbo_rect->max_img_width =
               fbo_scale->abs_x;
            break;

         case RARCH_SCALE_VIEWPORT:
            if (gl->rotation % 180 == 90)
            {
               fbo_rect->img_width      = fbo_rect->max_img_width =
               fbo_scale->scale_x * vp_height;
            } else {
               fbo_rect->img_width      = fbo_rect->max_img_width =
               fbo_scale->scale_x * vp_width;
            }
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
            if (gl->rotation % 180 == 90)
            {
               fbo_rect->img_height      = fbo_rect->max_img_height =
               fbo_scale->scale_y * vp_width;
            } else {
            fbo_rect->img_height     = fbo_rect->max_img_height =
               fbo_scale->scale_y * vp_height;
            }
            break;
      }

      if (fbo_rect->img_width > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->img_width      = max_size;
      }

      if (fbo_rect->img_height > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->img_height     = max_size;
      }

      if (fbo_rect->max_img_width > (unsigned)max_size)
      {
         size_modified            = true;
         fbo_rect->max_img_width  = max_size;
      }

      if (fbo_rect->max_img_height > (unsigned)max_size)
      {
         size_modified            = true;
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

static void gl2_renderchain_start_render(
      gl_t *gl,
      gl2_renderchain_data_t *chain,
      video_frame_info_t *video_info)
{
   /* Used when rendering to an FBO.
    * Texture coords have to be aligned
    * with vertex coordinates. */
   static const GLfloat fbo_vertexes[] = {
      0, 0,
      1, 0,
      0, 1,
      1, 1
   };
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   gl2_bind_fb(chain->fbo[0]);

   gl2_set_viewport(gl,
         video_info, gl->fbo_rect[0].img_width,
         gl->fbo_rect[0].img_height, true, false);

   /* Need to preserve the "flipped" state when in FBO
    * as well to have consistent texture coordinates.
    *
    * We will "flip" it in place on last pass. */
   gl->coords.vertex = fbo_vertexes;

#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (chain->has_srgb_fbo)
      glEnable(GL_FRAMEBUFFER_SRGB);
#endif
}

/* Set up render to texture. */
static void gl2_renderchain_init(
      gl_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned fbo_width, unsigned fbo_height)
{
   int i;
   unsigned width, height;
   video_shader_ctx_scale_t scaler;
   video_shader_ctx_info_t shader_info;
   struct gfx_fbo_scale scale, scale_last;

   if (!gl2_shader_info(gl, &shader_info))
      return;

   if (!gl || shader_info.num == 0)
      return;

   width        = gl->video_width;
   height       = gl->video_height;

   scaler.idx   = 1;
   scaler.scale = &scale;

   gl2_shader_scale(gl, &scaler);

   scaler.idx   = shader_info.num;
   scaler.scale = &scale_last;

   gl2_shader_scale(gl, &scaler);

   /* we always want FBO to be at least initialized on startup for consoles */
   if (shader_info.num == 1 && !scale.valid)
      return;

   if (!gl->has_fbo)
   {
      RARCH_ERR("[GL]: Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
      return;
   }

   chain->fbo_pass = shader_info.num - 1;
   if (scale_last.valid)
      chain->fbo_pass++;

   if (!scale.valid)
   {
      scale.scale_x = 1.0f;
      scale.scale_y = 1.0f;
      scale.type_x  = scale.type_y = RARCH_SCALE_INPUT;
      scale.valid   = true;
   }

   chain->fbo_scale[0] = scale;

   for (i = 1; i < chain->fbo_pass; i++)
   {
      scaler.idx   = i + 1;
      scaler.scale = &chain->fbo_scale[i];

      gl2_shader_scale(gl, &scaler);

      if (!chain->fbo_scale[i].valid)
      {
         chain->fbo_scale[i].scale_x = chain->fbo_scale[i].scale_y = 1.0f;
         chain->fbo_scale[i].type_x  = chain->fbo_scale[i].type_y  =
            RARCH_SCALE_INPUT;
         chain->fbo_scale[i].valid   = true;
      }
   }

   gl2_renderchain_recompute_pass_sizes(gl,
         chain, fbo_width, fbo_height, width, height);

   for (i = 0; i < chain->fbo_pass; i++)
   {
      gl->fbo_rect[i].width  = next_pow2(gl->fbo_rect[i].img_width);
      gl->fbo_rect[i].height = next_pow2(gl->fbo_rect[i].img_height);
      RARCH_LOG("[GL]: Creating FBO %d @ %ux%u\n", i,
            gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   gl->fbo_feedback_enable = gl->shader->get_feedback_pass(gl->shader_data,
         &gl->fbo_feedback_pass);

   if (gl->fbo_feedback_enable && gl->fbo_feedback_pass
         < (unsigned)chain->fbo_pass)
   {
      RARCH_LOG("[GL]: Creating feedback FBO %d @ %ux%u\n", i,
            gl->fbo_rect[gl->fbo_feedback_pass].width,
            gl->fbo_rect[gl->fbo_feedback_pass].height);
   }
   else if (gl->fbo_feedback_enable)
   {
      RARCH_WARN("[GL]: Tried to create feedback FBO of pass #%u, but there are only %d FBO passes. Will use input texture as feedback texture.\n",
              gl->fbo_feedback_pass, chain->fbo_pass);
      gl->fbo_feedback_enable = false;
   }

   gl2_create_fbo_textures(gl, chain);
   if (!gl || !gl2_create_fbo_targets(gl, chain))
   {
      glDeleteTextures(chain->fbo_pass, chain->fbo_texture);
      RARCH_ERR("[GL]: Failed to create FBO targets. Will continue without FBO.\n");
      return;
   }

   gl->fbo_inited = true;
}

static bool gl2_renderchain_init_hw_render(
      gl_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned width, unsigned height)
{
   GLenum status;
   unsigned i;
   bool depth                           = false;
   bool stencil                         = false;
   GLint max_fbo_size                   = 0;
   GLint max_renderbuffer_size          = 0;
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context();

   /* We can only share texture objects through contexts.
    * FBOs are "abstract" objects and are not shared. */
   gl2_context_bind_hw_render(gl, true);

   RARCH_LOG("[GL]: Initializing HW render (%u x %u).\n", width, height);
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_fbo_size);
   glGetIntegerv(RARCH_GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size);
   RARCH_LOG("[GL]: Max texture size: %d px, renderbuffer size: %d px.\n",
         max_fbo_size, max_renderbuffer_size);

   if (!gl->has_fbo)
      return false;

   RARCH_LOG("[GL]: Supports FBO (render-to-texture).\n");

   glBindTexture(GL_TEXTURE_2D, 0);
   gl2_gen_fb(gl->textures, gl->hw_render_fbo);

   depth   = hwr->depth;
   stencil = hwr->stencil;

   if (depth)
   {
      gl2_gen_rb(gl->textures, chain->hw_render_depth);
      chain->hw_render_depth_init = true;
   }

   for (i = 0; i < gl->textures; i++)
   {
      gl2_bind_fb(gl->hw_render_fbo[i]);
      gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->texture[i], 0);

      if (depth)
      {
         gl2_bind_rb(RARCH_GL_RENDERBUFFER, chain->hw_render_depth[i]);
         gl2_rb_storage(RARCH_GL_RENDERBUFFER,
               stencil ? RARCH_GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT16,
               width, height);
         gl2_bind_rb(RARCH_GL_RENDERBUFFER, 0);

         if (stencil)
         {
#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES1) || ((defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__))))
            /* GLES2 is a bit weird, as always.
             * There's no GL_DEPTH_STENCIL_ATTACHMENT like in desktop GL. */
            gl2_fb_rb(RARCH_GL_FRAMEBUFFER,
                  RARCH_GL_DEPTH_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER,
                  chain->hw_render_depth[i]);
            gl2_fb_rb(RARCH_GL_FRAMEBUFFER,
                  RARCH_GL_STENCIL_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER,
                  chain->hw_render_depth[i]);
#else
            /* We use ARB FBO extensions, no need to check. */
            gl2_fb_rb(RARCH_GL_FRAMEBUFFER,
                  GL_DEPTH_STENCIL_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER,
                  chain->hw_render_depth[i]);
#endif
         }
         else
         {
            gl2_fb_rb(RARCH_GL_FRAMEBUFFER,
                  RARCH_GL_DEPTH_ATTACHMENT,
                  RARCH_GL_RENDERBUFFER,
                  chain->hw_render_depth[i]);
         }
      }

      status = gl2_check_fb_status(RARCH_GL_FRAMEBUFFER);
      if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
      {
         RARCH_ERR("[GL]: Failed to create HW render FBO #%u, error: 0x%04x.\n",
               i, status);
         return false;
      }
   }

   gl2_renderchain_bind_backbuffer();
   gl->hw_render_fbo_init = true;

   gl2_context_bind_hw_render(gl, false);
   return true;
}

static void gl2_renderchain_bind_prev_texture(
      gl_t *gl,
      gl2_renderchain_data_t *chain,
      const struct video_tex_info *tex_info)
{
   memmove(gl->prev_info + 1, gl->prev_info,
         sizeof(*tex_info) * (gl->textures - 1));
   memcpy(&gl->prev_info[0], tex_info,
         sizeof(*tex_info));

   /* Implement feedback by swapping out FBO/textures
    * for FBO pass #N and feedbacks. */
   if (gl->fbo_feedback_enable)
   {
      GLuint tmp_fbo                 = gl->fbo_feedback;
      GLuint tmp_tex                 = gl->fbo_feedback_texture;
      gl->fbo_feedback               = chain->fbo[gl->fbo_feedback_pass];
      gl->fbo_feedback_texture       = chain->fbo_texture[gl->fbo_feedback_pass];
      chain->fbo[gl->fbo_feedback_pass]         = tmp_fbo;
      chain->fbo_texture[gl->fbo_feedback_pass] = tmp_tex;
   }
}

static bool gl2_renderchain_read_viewport(
      gl_t *gl,
      uint8_t *buffer, bool is_idle)
{
   unsigned                     num_pixels = 0;

   gl2_context_bind_hw_render(gl, false);

   num_pixels = gl->vp.width * gl->vp.height;

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
      ptr        = (const uint8_t*)glMapBufferRange(GL_PIXEL_PACK_BUFFER,
            0, num_pixels * sizeof(uint32_t), GL_MAP_READ_BIT);

      if (ptr)
      {
         unsigned y;
         for (y = 0; y < gl->vp.height; y++)
         {
            video_frame_convert_rgba_to_bgr(
                  (const void*)ptr,
                  buffer,
                  gl->vp.width);
         }
      }
#else
      ptr = (const uint8_t*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
      if (ptr)
      {
         struct scaler_ctx *ctx = &gl->pbo_readback_scaler;
         scaler_ctx_scale_direct(ctx, buffer, ptr);
      }
#endif

      if (!ptr)
      {
         RARCH_ERR("[GL]: Failed to map pixel unpack buffer.\n");
         goto error;
      }

      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
   }
   else
#endif
   {
      /* Use slow synchronous readbacks. Use this with plain screenshots
         as we don't really care about performance in this case. */

      /* GLES2 only guarantees GL_RGBA/GL_UNSIGNED_BYTE
       * readbacks so do just that.
       * GLES2 also doesn't support reading back data
       * from front buffer, so render a cached frame
       * and have gl_frame() do the readback while it's
       * in the back buffer.
       *
       * Keep codepath similar for GLES and desktop GL.
       */
      gl->readback_buffer_screenshot = malloc(num_pixels * sizeof(uint32_t));

      if (!gl->readback_buffer_screenshot)
         goto error;

      if (!is_idle)
         video_driver_cached_frame();

      video_frame_convert_rgba_to_bgr(
            (const void*)gl->readback_buffer_screenshot,
            buffer,
            num_pixels);

      free(gl->readback_buffer_screenshot);
      gl->readback_buffer_screenshot = NULL;
   }

   gl2_context_bind_hw_render(gl, true);
   return true;

error:
   gl2_context_bind_hw_render(gl, true);

   return false;
}

#ifdef HAVE_OPENGLES
#define gl2_renderchain_restore_default_state(gl) \
   glDisable(GL_DEPTH_TEST); \
   glDisable(GL_CULL_FACE); \
   glDisable(GL_DITHER)
#else
#define gl2_renderchain_restore_default_state(gl) \
   if (!gl->core_context_in_use) \
      glEnable(GL_TEXTURE_2D); \
   glDisable(GL_DEPTH_TEST); \
   glDisable(GL_CULL_FACE); \
   glDisable(GL_DITHER)
#endif

static void gl2_renderchain_copy_frame(
      gl_t *gl,
      gl2_renderchain_data_t *chain,
      video_frame_info_t *video_info,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
#if defined(HAVE_PSGL)
   {
      unsigned h;
      size_t buffer_addr        = gl->tex_w * gl->tex_h *
         gl->tex_index * gl->base_size;
      size_t buffer_stride      = gl->tex_w * gl->base_size;
      const uint8_t *frame_copy = frame;
      size_t frame_copy_size    = width * gl->base_size;
      uint8_t           *buffer = (uint8_t*)glMapBuffer(
            GL_TEXTURE_REFERENCE_BUFFER_SCE, GL_READ_WRITE) + buffer_addr;
      for (h = 0; h < height; h++, buffer += buffer_stride, frame_copy += pitch)
         memcpy(buffer, frame_copy, frame_copy_size);

      glUnmapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE);
   }
#elif defined(HAVE_OPENGLES)
#if defined(HAVE_EGL)
   if (chain->egl_images)
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

      new_egl         =
         video_context_driver_write_to_image_buffer(&img_info);

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
      glPixelStorei(GL_UNPACK_ALIGNMENT,
            video_pixel_get_alignment(width * gl->base_size));

      /* Fallback for GLES devices without GL_BGRA_EXT. */
      if (gl->base_size == 4 && video_info->use_rgba)
      {
         video_frame_convert_argb8888_to_abgr8888(
               &gl->scaler,
               gl->conv_buffer,
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
#else
   {
      const GLvoid *data_buf = frame;
      glPixelStorei(GL_UNPACK_ALIGNMENT, video_pixel_get_alignment(pitch));

      if (gl->base_size == 2 && !gl->have_es2_compat)
      {
         /* Convert to 32-bit textures on desktop GL.
          *
          * It is *much* faster (order of magnitude on my setup)
          * to use a custom SIMD-optimized conversion routine
          * than letting GL do it. */
         video_frame_convert_rgb16_to_rgb32(
               &gl->scaler,
               gl->conv_buffer,
               frame,
               width,
               height,
               pitch);
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
}

#if !defined(HAVE_OPENGLES2) && !defined(HAVE_PSGL)
#define gl2_renderchain_bind_pbo(idx) glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)idx)
#define gl2_renderchain_unbind_pbo()  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0)
#define gl2_renderchain_init_pbo(size, data) glBufferData(GL_PIXEL_PACK_BUFFER, size, (const GLvoid*)data, GL_STREAM_READ)
#else
#define gl2_renderchain_bind_pbo(idx)
#define gl2_renderchain_unbind_pbo()
#define gl2_renderchain_init_pbo(size, data)
#endif

static void gl2_renderchain_readback(
      gl_t *gl,
      void *chain_data,
      unsigned alignment,
      unsigned fmt, unsigned type,
      void *src)
{
   glPixelStorei(GL_PACK_ALIGNMENT, alignment);
#ifndef HAVE_OPENGLES
   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glReadBuffer(GL_BACK);
#endif

   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         (GLenum)fmt, (GLenum)type, (GLvoid*)src);
}

static void gl2_renderchain_fence_iterate(
      void *data,
      gl2_renderchain_data_t *chain,
      unsigned hard_sync_frames)
{
#ifndef HAVE_OPENGLES
#ifdef HAVE_GL_SYNC
   chain->fences[chain->fence_count++] =
      glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

   while (chain->fence_count > hard_sync_frames)
   {
      glClientWaitSync(chain->fences[0],
            GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
      glDeleteSync(chain->fences[0]);

      chain->fence_count--;
      memmove(chain->fences, chain->fences + 1,
            chain->fence_count * sizeof(void*));
   }
#endif
#endif
}

static void gl2_renderchain_fence_free(void *data,
      gl2_renderchain_data_t *chain)
{
#ifndef HAVE_OPENGLES
#ifdef HAVE_GL_SYNC
   unsigned i;

   for (i = 0; i < chain->fence_count; i++)
   {
      glClientWaitSync(chain->fences[i],
            GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
      glDeleteSync(chain->fences[i]);
   }
   chain->fence_count = 0;
#endif
#endif
}

static void gl2_renderchain_init_texture_reference(
      gl_t *gl,
      gl2_renderchain_data_t *chain,
      unsigned i,
      unsigned internal_fmt, unsigned texture_fmt,
      unsigned texture_type)
{
#ifdef HAVE_PSGL
   glTextureReferenceSCE(GL_TEXTURE_2D, 1,
         gl->tex_w, gl->tex_h, 0,
         (GLenum)internal_fmt,
         gl->tex_w * gl->base_size,
         gl->tex_w * gl->tex_h * i * gl->base_size);
#else
   if (chain->egl_images)
      return;

   gl2_load_texture_image(GL_TEXTURE_2D,
      0,
      (GLenum)internal_fmt,
      gl->tex_w, gl->tex_h, 0,
      (GLenum)texture_type,
      (GLenum)texture_fmt,
      gl->empty_buf ? gl->empty_buf : NULL);
#endif
}

static void gl2_renderchain_resolve_extensions(gl_t *gl,
      gl2_renderchain_data_t *chain,
      const char *context_ident,
      const video_info_t *video)
{
   settings_t *settings             = config_get_ptr();

   if (!chain)
      return;

   chain->has_srgb_fbo              = false;
   chain->has_fp_fbo                = gl_check_capability(GL_CAPS_FP_FBO);
   /* GLES3 has unpack_subimage and sRGB in core. */
   chain->has_srgb_fbo_gles3        = gl_check_capability(GL_CAPS_SRGB_FBO_ES3);

   if (!settings->bools.video_force_srgb_disable)
      chain->has_srgb_fbo           = gl_check_capability(GL_CAPS_SRGB_FBO);

   /* Use regular textures if we use HW render. */
   chain->egl_images                = !gl->hw_render_use 
      && gl_check_capability(GL_CAPS_EGLIMAGE) 
      && gl->ctx_driver->image_buffer_init
      && gl->ctx_driver->image_buffer_init(gl->ctx_data, video);
}

static void gl_load_texture_data(
      GLuint id,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size)
{
   GLint mag_filter, min_filter;
   bool want_mipmap = false;
   bool use_rgba    = video_driver_supports_rgba();
   bool rgb32       = (base_size == (sizeof(uint32_t)));
   GLenum wrap      = gl2_wrap_type_to_enum(wrap_type);
   bool have_mipmap = gl_check_capability(GL_CAPS_MIPMAP);

   if (!have_mipmap)
   {
      /* Assume no mipmapping support. */
      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
            filter_type = TEXTURE_FILTER_LINEAR;
            break;
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            filter_type = TEXTURE_FILTER_NEAREST;
            break;
         default:
            break;
      }
   }

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

   gl_bind_texture(id, wrap, mag_filter, min_filter);

   glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
   glTexImage2D(GL_TEXTURE_2D,
         0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32,
         width, height, 0,
         (use_rgba || !rgb32) ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32,
         (rgb32) ? RARCH_GL_FORMAT32 : GL_UNSIGNED_SHORT_4_4_4_4, frame);

   if (want_mipmap && have_mipmap)
      glGenerateMipmap(GL_TEXTURE_2D);
}

static bool gl2_add_lut(
      const char *lut_path,
      bool lut_mipmap,
      unsigned lut_filter,
      enum gfx_wrap_type lut_wrap_type,
      unsigned i, GLuint *textures_lut)
{
   struct texture_image img;
   enum texture_filter_type filter_type = TEXTURE_FILTER_LINEAR;

   img.width         = 0;
   img.height        = 0;
   img.pixels        = NULL;
   img.supports_rgba = video_driver_supports_rgba();

   if (!image_texture_load(&img, lut_path))
   {
      RARCH_ERR("[GL]: Failed to load texture image from: \"%s\"\n",
            lut_path);
      return false;
   }

   RARCH_LOG("[GL]: Loaded texture image from: \"%s\" ...\n",
         lut_path);

   if (lut_filter == RARCH_FILTER_NEAREST)
      filter_type = TEXTURE_FILTER_NEAREST;

   if (lut_mipmap)
   {
      if (filter_type == TEXTURE_FILTER_NEAREST)
         filter_type = TEXTURE_FILTER_MIPMAP_NEAREST;
      else
         filter_type = TEXTURE_FILTER_MIPMAP_LINEAR;
   }

   gl_load_texture_data(
         textures_lut[i],
         lut_wrap_type,
         filter_type, 4,
         img.width, img.height,
         img.pixels, sizeof(uint32_t));
   image_texture_free(&img);

   return true;
}

bool gl_load_luts(
      const void *shader_data,
      GLuint *textures_lut)
{
   unsigned i;
   const struct video_shader *shader =
      (const struct video_shader*)shader_data;
   unsigned num_luts                 = MIN(shader->luts, GFX_MAX_TEXTURES);

   if (!shader->luts)
      return true;

   glGenTextures(num_luts, textures_lut);

   for (i = 0; i < num_luts; i++)
   {
      if (!gl2_add_lut(
               shader->lut[i].path,
               shader->lut[i].mipmap,
               shader->lut[i].filter,
               shader->lut[i].wrap,
               i, textures_lut))
         return false;
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   return true;
}

#ifdef HAVE_OVERLAY
static void gl2_free_overlay(gl_t *gl)
{
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

static void gl2_overlay_vertex_geom(void *data,
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
      RARCH_ERR("[GL]: Invalid overlay id: %u\n", image);
      return;
   }

   vertex          = (GLfloat*)&gl->overlay_vertex_coord[image * 8];

   /* Flipped, so we preserve top-down semantics. */
   y               = 1.0f - y;
   h               = -h;

   vertex[0]       = x;
   vertex[1]       = y;
   vertex[2]       = x + w;
   vertex[3]       = y;
   vertex[4]       = x;
   vertex[5]       = y + h;
   vertex[6]       = x + w;
   vertex[7]       = y + h;
}

static void gl2_overlay_tex_geom(void *data,
      unsigned image,
      GLfloat x, GLfloat y,
      GLfloat w, GLfloat h)
{
   GLfloat *tex = NULL;
   gl_t *gl     = (gl_t*)data;

   if (!gl)
      return;

   tex          = (GLfloat*)&gl->overlay_tex_coord[image * 8];

   tex[0]       = x;
   tex[1]       = y;
   tex[2]       = x + w;
   tex[3]       = y;
   tex[4]       = x;
   tex[5]       = y + h;
   tex[6]       = x + w;
   tex[7]       = y + h;
}

static void gl2_render_overlay(gl_t *gl, video_frame_info_t *video_info)
{
   unsigned i;
   unsigned width                      = video_info->width;
   unsigned height                     = video_info->height;

   glEnable(GL_BLEND);

   if (gl->overlay_full_screen)
      glViewport(0, 0, width, height);

   /* Ensure that we reset the attrib array. */
   gl->shader->use(gl, gl->shader_data,
         VIDEO_SHADER_STOCK_BLEND, true);

   gl->coords.vertex    = gl->overlay_vertex_coord;
   gl->coords.tex_coord = gl->overlay_tex_coord;
   gl->coords.color     = gl->overlay_color_coord;
   gl->coords.vertices  = 4 * gl->overlays;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);

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
#endif

static void gl2_set_viewport_wrapper(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   video_frame_info_t video_info;
   gl_t               *gl = (gl_t*)data;

   video_driver_build_info(&video_info);

   gl2_set_viewport(gl, &video_info,
         viewport_width, viewport_height, force_full, allow_rotate);
}

/* Shaders */

/**
 * gl2_get_fallback_shader_type:
 * @type                      : shader type which should be used if possible
 *
 * Returns a supported fallback shader type in case the given one is not supported.
 * For gl2, shader support is completely defined by the context driver shader flags.
 *
 * gl2_get_fallback_shader_type(RARCH_SHADER_NONE) returns a default shader type.
 * if gl2_get_fallback_shader_type(type) != type, type was not supported.
 * 
 * Returns: A supported shader type.
 *  If RARCH_SHADER_NONE is returned, no shader backend is supported.
 **/
static enum rarch_shader_type gl2_get_fallback_shader_type(enum rarch_shader_type type)
{
#if defined(HAVE_GLSL) || defined(HAVE_CG)
   unsigned i;

   if (type != RARCH_SHADER_CG && type != RARCH_SHADER_GLSL)
   {
      type = DEFAULT_SHADER_TYPE;

      if (type != RARCH_SHADER_CG && type != RARCH_SHADER_GLSL)
         type = RARCH_SHADER_GLSL;
   }

   for (i = 0; i < 2; i++)
   {
      switch (type)
      {
         case RARCH_SHADER_CG:
#ifdef HAVE_CG
            if (video_shader_is_supported(type))
               return type;
#endif
            type = RARCH_SHADER_GLSL;
            break;

         case RARCH_SHADER_GLSL:
#ifdef HAVE_GLSL
            if (video_shader_is_supported(type))
               return type;
#endif
            type = RARCH_SHADER_CG;
            break;

         default:
            return RARCH_SHADER_NONE;
      }
   }
#endif
   return RARCH_SHADER_NONE;
}

static const shader_backend_t *gl_shader_driver_set_backend(
      enum rarch_shader_type type)
{
   enum rarch_shader_type fallback = gl2_get_fallback_shader_type(type);
   if (fallback != type)
      RARCH_ERR("[Shader driver]: Shader backend %d not supported, falling back to %d.", type, fallback);

   switch (fallback)
   {
#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         RARCH_LOG("[Shader driver]: Using Cg shader backend.\n");
         return &gl_cg_backend;
#endif
#ifdef HAVE_GLSL
      case RARCH_SHADER_GLSL:
         RARCH_LOG("[Shader driver]: Using GLSL shader backend.\n");
         return &gl_glsl_backend;
#endif
      default:
         RARCH_LOG("[Shader driver]: No supported shader backend.\n");
         return NULL;
   }
}

static bool gl_shader_driver_init(video_shader_ctx_init_t *init)
{
   void            *tmp = NULL;
   settings_t *settings = config_get_ptr();

   if (!init->shader || !init->shader->init)
   {
      init->shader = gl_shader_driver_set_backend(init->shader_type);

      if (!init->shader)
         return false;
   }

   tmp = init->shader->init(init->data, init->path);

   if (!tmp)
      return false;

   if (string_is_equal(settings->arrays.menu_driver, "xmb")
         && init->shader->init_menu_shaders)
   {
      RARCH_LOG("Setting up menu pipeline shaders for XMB ... \n");
      init->shader->init_menu_shaders(tmp);
   }

   init->shader_data = tmp;

   return true;
}

static bool gl2_shader_init(gl_t *gl, const gfx_ctx_driver_t *ctx_driver,
      struct retro_hw_render_callback *hwr
      )
{
   video_shader_ctx_init_t init_data;
   bool ret                          = false;
   const char *shader_path           = retroarch_get_shader_preset();
   enum rarch_shader_type parse_type = video_shader_parse_type(shader_path);
   enum rarch_shader_type type;

   type = gl2_get_fallback_shader_type(parse_type);

   if (type == RARCH_SHADER_NONE)
   {
      RARCH_ERR("[GL]: Couldn't find any supported shader backend! Continuing without shaders.\n");
      return true;
   }

   if (type != parse_type)
   {
      if (!string_is_empty(shader_path))
         RARCH_WARN("[GL] Shader preset %s is using unsupported shader type %s, falling back to stock %s.\n",
            shader_path, video_shader_to_str(parse_type), video_shader_to_str(type));

      shader_path = NULL;
   }

#ifdef HAVE_GLSL
   if (type == RARCH_SHADER_GLSL)
   {
      gl_glsl_set_get_proc_address(ctx_driver->get_proc_address);
      gl_glsl_set_context_type(gl->core_context_in_use,
                               hwr->version_major, hwr->version_minor);
   }
#endif

   init_data.gl.core_context_enabled = gl->core_context_in_use;
   init_data.shader_type             = type;
   init_data.shader                  = NULL;
   init_data.data                    = gl;
   init_data.path                    = shader_path;

   if (gl_shader_driver_init(&init_data))
   {
      gl->shader                     = init_data.shader;
      gl->shader_data                = init_data.shader_data;
      return true;
   }

   RARCH_ERR("[GL]: Failed to initialize shader, falling back to stock.\n");

   init_data.shader = NULL;
   init_data.path   = NULL;

   ret              = gl_shader_driver_init(&init_data);

   gl->shader       = init_data.shader;
   gl->shader_data  = init_data.shader_data;

   return ret;
}

static uintptr_t gl2_get_current_framebuffer(void *data)
{
   gl_t *gl = (gl_t*)data;
   if (!gl || !gl->has_fbo)
      return 0;
   return gl->hw_render_fbo[(gl->tex_index + 1) % gl->textures];
}

static void gl2_set_rotation(void *data, unsigned rotation)
{
   gl_t               *gl = (gl_t*)data;

   if (!gl)
      return;

   gl->rotation = 90 * rotation;
   gl2_set_projection(gl, &default_ortho, true);
}

static void gl2_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_mode_t mode;

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   video_context_driver_set_video_mode(&mode);
}

static void gl2_update_input_size(gl_t *gl, unsigned width,
      unsigned height, unsigned pitch, bool clear)
{
   float xamt, yamt;

   if ((width != gl->last_width[gl->tex_index] ||
            height != gl->last_height[gl->tex_index]) && gl->empty_buf)
   {
      /* Resolution change. Need to clear out texture. */

      gl->last_width[gl->tex_index]  = width;
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
   }
   /* We might have used different texture coordinates
    * last frame. Edge case if resolution changes very rapidly. */
   else if ((width !=
            gl->last_width[(gl->tex_index + gl->textures - 1) % gl->textures]) ||
         (height !=
          gl->last_height[(gl->tex_index + gl->textures - 1) % gl->textures])) { }
   else
      return;

   xamt = (float)width  / gl->tex_w;
   yamt = (float)height / gl->tex_h;
   set_texture_coords(gl->tex_info.coord, xamt, yamt);
}

static void gl2_init_textures_data(gl_t *gl)
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

static void gl2_init_textures(gl_t *gl, const video_info_t *video)
{
   unsigned i;
   GLenum internal_fmt = gl->internal_fmt;
   GLenum texture_type = gl->texture_type;
   GLenum texture_fmt  = gl->texture_fmt;

#ifdef HAVE_PSGL
   if (!gl->pbo)
      glGenBuffers(1, &gl->pbo);

   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
         gl->tex_w * gl->tex_h * gl->base_size * gl->textures,
         NULL, GL_STREAM_DRAW);
#endif

#if defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   /* GLES is picky about which format we use here.
    * Without extensions, we can *only* render to 16-bit FBOs. */

   if (gl->hw_render_use && gl->base_size == sizeof(uint32_t))
   {
      if (gl_check_capability(GL_CAPS_ARGB8))
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
      gl_bind_texture(gl->texture[i], gl->wrap_mode, gl->tex_mag_filter,
            gl->tex_min_filter);

      gl2_renderchain_init_texture_reference(
            gl, (gl2_renderchain_data_t*)gl->renderchain_data,
            i, internal_fmt,
            texture_fmt, texture_type);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static INLINE void gl2_set_shader_viewports(gl_t *gl)
{
   unsigned i;
   video_frame_info_t video_info;
   unsigned width                = gl->video_width;
   unsigned height               = gl->video_height;

   video_driver_build_info(&video_info);

   for (i = 0; i < 2; i++)
   {
      gl->shader->use(gl, gl->shader_data, i, true);
      gl2_set_viewport(gl, &video_info,
            width, height, false, true);
   }
}

static void gl2_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   enum texture_filter_type menu_filter;
   settings_t *settings            = config_get_ptr();
   unsigned base_size              = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   gl_t *gl                        = (gl_t*)data;
   if (!gl)
      return;

   gl2_context_bind_hw_render(gl, false);

   menu_filter = settings->bools.menu_linear_filter ? TEXTURE_FILTER_LINEAR : TEXTURE_FILTER_NEAREST;

   if (!gl->menu_texture)
      glGenTextures(1, &gl->menu_texture);

   gl_load_texture_data(gl->menu_texture,
         RARCH_WRAP_EDGE, menu_filter,
         video_pixel_get_alignment(width * base_size),
         width, height, frame,
         base_size);

   gl->menu_texture_alpha = alpha;
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   gl2_context_bind_hw_render(gl, true);
}

static void gl2_set_texture_enable(void *data, bool state, bool full_screen)
{
   gl_t *gl                     = (gl_t*)data;

   if (!gl)
      return;

   gl->menu_texture_enable      = state;
   gl->menu_texture_full_screen = full_screen;
}

static void gl2_render_osd_background(
      gl_t *gl, video_frame_info_t *video_info,
      const char *msg)
{
   video_coords_t coords;
   struct uniform_info uniform_param;
   float colors[4];
   const unsigned
      vertices_total       = 6;
   float *dummy            = (float*)calloc(4 * vertices_total, sizeof(float));
   float *verts            = (float*)malloc(2 * vertices_total * sizeof(float));
   settings_t *settings    = config_get_ptr();
   int msg_width           =
      font_driver_get_message_width(NULL, msg, (unsigned)strlen(msg), 1.0f);

   /* shader driver expects vertex coords as 0..1 */
   float x                 = video_info->font_msg_pos_x;
   float y                 = video_info->font_msg_pos_y;
   float width             = msg_width / (float)video_info->width;
   float height            =
      settings->floats.video_font_size / (float)video_info->height;

   float x2                = 0.005f; /* extend background around text */
   float y2                = 0.005f;

   x                      -= x2;
   y                      -= y2;
   width                  += x2;
   height                 += y2;

   colors[0]               = settings->uints.video_msg_bgcolor_red / 255.0f;
   colors[1]               = settings->uints.video_msg_bgcolor_green / 255.0f;
   colors[2]               = settings->uints.video_msg_bgcolor_blue / 255.0f;
   colors[3]               = settings->floats.video_msg_bgcolor_opacity;

   /* triangle 1 */
   verts[0]                = x;
   verts[1]                = y; /* bottom-left */

   verts[2]                = x;
   verts[3]                = y + height; /* top-left */

   verts[4]                = x + width;
   verts[5]                = y + height; /* top-right */

   /* triangle 2 */
   verts[6]                = x;
   verts[7]                = y; /* bottom-left */

   verts[8]                = x + width;
   verts[9]                = y + height; /* top-right */

   verts[10]               = x + width;
   verts[11]               = y; /* bottom-right */

   coords.color            = dummy;
   coords.vertex           = verts;
   coords.tex_coord        = dummy;
   coords.lut_tex_coord    = dummy;
   coords.vertices         = vertices_total;

   video_driver_set_viewport(video_info->width,
         video_info->height, true, false);

   gl->shader->use(gl, gl->shader_data,
         VIDEO_SHADER_STOCK_BLEND, true);

   gl->shader->set_coords(gl->shader_data, &coords);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);

   uniform_param.type              = UNIFORM_4F;
   uniform_param.enabled           = true;
   uniform_param.location          = 0;
   uniform_param.count             = 0;

   uniform_param.lookup.type       = SHADER_PROGRAM_FRAGMENT;
   uniform_param.lookup.ident      = "bgcolor";
   uniform_param.lookup.idx        = VIDEO_SHADER_STOCK_BLEND;
   uniform_param.lookup.add_prefix = true;
   uniform_param.lookup.enable     = true;

   uniform_param.result.f.v0       = colors[0];
   uniform_param.result.f.v1       = colors[1];
   uniform_param.result.f.v2       = colors[2];
   uniform_param.result.f.v3       = colors[3];

   gl->shader->set_uniform_parameter(gl->shader_data,
         &uniform_param, NULL);

   glDrawArrays(GL_TRIANGLES, 0, coords.vertices);

   /* reset uniform back to zero so it is not used for anything else */
   uniform_param.result.f.v0       = 0.0f;
   uniform_param.result.f.v1       = 0.0f;
   uniform_param.result.f.v2       = 0.0f;
   uniform_param.result.f.v3       = 0.0f;

   gl->shader->set_uniform_parameter(gl->shader_data,
         &uniform_param, NULL);

   free(dummy);
   free(verts);

   video_driver_set_viewport(video_info->width,
         video_info->height, false, true);
}

static void gl2_show_mouse(void *data, bool state)
{
   gl_t                            *gl = (gl_t*)data;

   if (gl && gl->ctx_driver->show_mouse)
      gl->ctx_driver->show_mouse(gl->ctx_data, state);
}

static struct video_shader *gl2_get_current_shader(void *data)
{
   gl_t                            *gl = (gl_t*)data;

   if (!gl)
      return NULL;

   return gl->shader->get_current_shader(gl->shader_data);
}

#if defined(HAVE_MENU)
static INLINE void gl2_draw_texture(gl_t *gl, video_frame_info_t *video_info)
{
   GLfloat color[16];
   unsigned width         = video_info->width;
   unsigned height        = video_info->height;

   color[ 0]              = 1.0f;
   color[ 1]              = 1.0f;
   color[ 2]              = 1.0f;
   color[ 3]              = gl->menu_texture_alpha;
   color[ 4]              = 1.0f;
   color[ 5]              = 1.0f;
   color[ 6]              = 1.0f;
   color[ 7]              = gl->menu_texture_alpha;
   color[ 8]              = 1.0f;
   color[ 9]              = 1.0f;
   color[10]              = 1.0f;
   color[11]              = gl->menu_texture_alpha;
   color[12]              = 1.0f;
   color[13]              = 1.0f;
   color[14]              = 1.0f;
   color[15]              = gl->menu_texture_alpha;

   gl->coords.vertex      = vertexes_flipped;
   gl->coords.tex_coord   = tex_coords;
   gl->coords.color       = color;

   glBindTexture(GL_TEXTURE_2D, gl->menu_texture);

   gl->shader->use(gl,
         gl->shader_data, VIDEO_SHADER_STOCK_BLEND, true);

   gl->coords.vertices    = 4;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);

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

   gl->coords.vertex      = gl->vertex_ptr;
   gl->coords.tex_coord   = gl->tex_info.coord;
   gl->coords.color       = gl->white_color_ptr;
}
#endif

static void gl2_pbo_async_readback(gl_t *gl)
{
#ifdef HAVE_OPENGLES
   GLenum fmt  = GL_RGBA;
   GLenum type = GL_UNSIGNED_BYTE;
#else
   GLenum fmt  = GL_BGRA;
   GLenum type = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif

   gl2_renderchain_bind_pbo(
         gl->pbo_readback[gl->pbo_readback_index++]);
   gl->pbo_readback_index &= 3;

   /* 4 frames back, we can readback. */
   gl->pbo_readback_valid[gl->pbo_readback_index] = true;

   gl2_renderchain_readback(gl, gl->renderchain_data,
         video_pixel_get_alignment(gl->vp.width * sizeof(uint32_t)),
         fmt, type, NULL);
   gl2_renderchain_unbind_pbo();
}

#ifdef HAVE_VIDEO_LAYOUT

static float video_layout_layer_tex_coord[] = {
   0.0f, 1.0f,
   1.0f, 1.0f,
   0.0f, 0.0f,
   1.0f, 0.0f,
};

static void gl2_video_layout_fbo_init(gl_t *gl, unsigned width, unsigned height)
{
   glGenTextures(1, &gl->video_layout_fbo_texture);
   glBindTexture(GL_TEXTURE_2D, gl->video_layout_fbo_texture);

   gl2_load_texture_image(GL_TEXTURE_2D, 0, RARCH_GL_INTERNAL_FORMAT32,
      width, height, 0, GL_RGBA, GL_FLOAT, NULL);

   gl2_gen_fb(1, &gl->video_layout_fbo);
   gl2_bind_fb(gl->video_layout_fbo);

   gl2_fb_texture_2d(RARCH_GL_FRAMEBUFFER, RARCH_GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D, gl->video_layout_fbo_texture, 0);

   if (gl2_check_fb_status(RARCH_GL_FRAMEBUFFER) != 
         RARCH_GL_FRAMEBUFFER_COMPLETE)
      RARCH_LOG("Unable to create FBO for video_layout\n");

   gl2_bind_fb(0);
}

static void gl2_video_layout_fbo_free(gl_t *gl)
{
   if (gl->video_layout_fbo)
   {
      gl2_delete_fb(1, &gl->video_layout_fbo);
      gl->video_layout_fbo = 0;
   }

   if (gl->video_layout_fbo_texture)
   {
      glDeleteTextures(1, &gl->video_layout_fbo_texture);
      gl->video_layout_fbo_texture = 0;
   }
}

static void gl2_video_layout_viewport(gl_t *gl, video_frame_info_t *video_info)
{
   if (!video_layout_valid())
      return;

   if (gl->video_layout_resize)
   {
      if (gl->video_layout_fbo)
         gl2_video_layout_fbo_free(gl);

      gl2_video_layout_fbo_init(gl, video_info->width, video_info->height);

      video_layout_view_change();

      gl->video_layout_resize = false;
   }

   if (video_layout_view_on_change())
   {
      video_layout_bounds_t b;
      b.x = 0.0f;
      b.y = 0.0f;
      b.w = (float)video_info->width;
      b.h = (float)video_info->height;
      video_layout_view_fit_bounds(b);
   }

   if (video_layout_screen_count())
   {
      const video_layout_bounds_t *bounds;
      bounds = video_layout_screen(0);

      glViewport(
         bounds->x, video_info->height - bounds->y - bounds->h,
         bounds->w, bounds->h
      );
   }
}

static void gl2_video_layout_render(gl_t *gl, video_frame_info_t *video_info)
{
   int i;

   if (!video_layout_valid())
      return;

   glViewport(0, 0, video_info->width, video_info->height);
   glEnable(GL_BLEND);

   for (i = 0; i < video_layout_layer_count(); ++i)
      video_layout_layer_render(video_info, i);

   glDisable(GL_BLEND);
}

static void gl2_video_layout_init(gl_t *gl)
{
   uint32_t px;

   gl->video_layout_resize = true;

   /* white 1px texture for drawing solid colors */
   px = 0xFFFFFFFF;

   glGenTextures(1, &gl->video_layout_white_texture);
   gl_load_texture_data(gl->video_layout_white_texture,
      RARCH_WRAP_EDGE, TEXTURE_FILTER_NEAREST,
      sizeof(uint32_t), 1, 1, &px, sizeof(uint32_t));
}

static void gl2_video_layout_free(gl_t *gl)
{
   gl2_video_layout_fbo_free(gl);

   if (gl->video_layout_white_texture)
   {
      glDeleteTextures(1, &gl->video_layout_white_texture);
      gl->video_layout_white_texture = 0;
   }
}

static void *gl2_video_layout_take_image(void *video_driver_data, struct texture_image image)
{
   unsigned alignment;
   GLuint tex;

   tex = 0;
   alignment = video_pixel_get_alignment(image.width * sizeof(uint32_t));

   glGenTextures(1, &tex);

   gl_load_texture_data(tex,
      RARCH_WRAP_EDGE, TEXTURE_FILTER_MIPMAP_LINEAR,
      alignment, image.width, image.height, image.pixels, sizeof(uint32_t));

   free(image.pixels);

   return (void*)(uintptr_t)tex;
}

static void gl2_video_layout_free_image(void *video_driver_data, void *image)
{
   GLuint tex;
   tex = (GLuint)(uintptr_t)image;
   glDeleteTextures(1, &tex);
}

static void gl2_video_layout_layer_begin(const video_layout_render_info_t *info)
{
   gl_t *gl;
   gl = (gl_t*)info->video_driver_data;

   gl2_bind_fb(gl->video_layout_fbo);

   glClearColor(0, 0, 0, 0);
   glClear(GL_COLOR_BUFFER_BIT);

   gl->shader->use(gl, gl->shader_data,
      VIDEO_SHADER_STOCK_BLEND, true);

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void gl2_video_layout_image(const video_layout_render_info_t *info, void *image_handle, void *alpha_handle)
{
   /* TODO alpha_handle */

   gl_t *gl;
   video_frame_info_t *video_info;
   video_layout_bounds_t b;
   float coord[8];
   float color[16];
   int i;

   gl = (gl_t*)info->video_driver_data;
   video_info = (video_frame_info_t*)info->video_driver_frame_data;

   b = info->bounds;
   b.x /= video_info->width;
   b.y /= video_info->height;
   b.w /= video_info->width;
   b.h /= video_info->height;

   coord[0] = b.x;
   coord[1] = 1.f - b.y;
   coord[2] = b.x + b.w;
   coord[3] = 1.f - b.y;
   coord[4] = b.x;
   coord[5] = 1.f - (b.y + b.h);
   coord[6] = b.x + b.w;
   coord[7] = 1.f - (b.y + b.h);

   i = 0;
   while(i < 16)
   {
      color[i++] = info->color.r;
      color[i++] = info->color.g;
      color[i++] = info->color.b;
      color[i++] = info->color.a;
   }

   gl->coords.vertex = coord;
   gl->coords.tex_coord = tex_coords;
   gl->coords.color = color;
   gl->coords.vertices = 4;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);

   glBindTexture(GL_TEXTURE_2D, (GLuint)(uintptr_t)image_handle);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void gl2_video_layout_text(const video_layout_render_info_t *info, const char *str)
{
   /* TODO */
}

static void gl2_video_layout_counter(const video_layout_render_info_t *info, int value)
{
   /* TODO */
}

static void gl2_video_layout_rect(const video_layout_render_info_t *info)
{
   gl_t *gl;
   gl = (gl_t*)info->video_driver_data;

   gl2_video_layout_image(info, (void*)(uintptr_t)gl->video_layout_white_texture, NULL);
}

static void gl2_video_layout_screen(const video_layout_render_info_t *info, int screen_index)
{
   gl2_video_layout_rect(info);
}

static void gl2_video_layout_ellipse(const video_layout_render_info_t *info)
{
   /* TODO */
}

static void gl2_video_layout_led_dot(const video_layout_render_info_t *info, int dot_count, int dot_mask)
{
   /* TODO */
}

static void gl2_video_layout_led_seg(const video_layout_render_info_t *info, video_layout_led_t seg_layout, int seg_mask)
{
   /* TODO */
}

static void gl2_video_layout_layer_end(const video_layout_render_info_t *info, video_layout_blend_t blend_type)
{
   gl_t *gl;
   gl = (gl_t*)info->video_driver_data;

   switch (blend_type)
   {
   case VIDEO_LAYOUT_BLEND_ALPHA:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
   case VIDEO_LAYOUT_BLEND_ADD:
      glBlendFunc(GL_ONE, GL_ONE);
      break;
   case VIDEO_LAYOUT_BLEND_MOD:
      glBlendFunc(GL_DST_COLOR, GL_ZERO);
      break;
   }

   gl2_bind_fb(0);

   gl->coords.vertex = gl->vertex_ptr;
   gl->coords.tex_coord = video_layout_layer_tex_coord;
   gl->coords.color = gl->white_color_ptr;
   gl->coords.vertices = 4;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp_no_rot);

   glBindTexture(GL_TEXTURE_2D, gl->video_layout_fbo_texture);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_info.coord;
}

static video_layout_render_interface_t gl2_video_layout_render_interface =
{
   gl2_video_layout_take_image,
   gl2_video_layout_free_image,
   gl2_video_layout_layer_begin,
   gl2_video_layout_screen,
   gl2_video_layout_image,
   gl2_video_layout_text,
   gl2_video_layout_counter,
   gl2_video_layout_rect,
   gl2_video_layout_ellipse,
   gl2_video_layout_led_dot,
   gl2_video_layout_led_seg,
   gl2_video_layout_layer_end
};

static const video_layout_render_interface_t *gl2_get_video_layout_render_interface(void *data)
{
   return &gl2_video_layout_render_interface;
}

#endif /* HAVE_VIDEO_LAYOUT */

static bool gl2_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count,
      unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   video_shader_ctx_params_t params;
   struct video_tex_info feedback_info;
   gl_t                            *gl = (gl_t*)data;
   gl2_renderchain_data_t       *chain = (gl2_renderchain_data_t*)gl->renderchain_data;
   unsigned width                      = video_info->width;
   unsigned height                     = video_info->height;

   if (!gl)
      return false;

   gl2_context_bind_hw_render(gl, false);

#ifndef HAVE_OPENGLES
   if (gl->core_context_in_use)
      glBindVertexArray(chain->vao);
#endif

   gl->shader->use(gl, gl->shader_data, 1, true);

#ifdef IOS
   /* Apparently the viewport is lost each frame, thanks Apple. */
   gl2_set_viewport(gl, video_info, width, height, false, true);
#endif

   /* Render to texture in first pass. */
   if (gl->fbo_inited)
   {
      gl2_renderchain_recompute_pass_sizes(
            gl, chain,
            frame_width, frame_height,
            gl->vp_out_width, gl->vp_out_height);

      gl2_renderchain_start_render(gl, chain, video_info);
   }

   if (gl->should_resize)
   {
      video_info->cb_set_resize(video_info->context_data,
            width, height);
      gl->should_resize = false;

      if (gl->fbo_inited)
      {
         /* On resize, we might have to recreate our FBOs
          * due to "Viewport" scale, and set a new viewport. */
         unsigned i;

         /* Check if we have to recreate our FBO textures. */
         for (i = 0; i < (unsigned)chain->fbo_pass; i++)
         {
            struct video_fbo_rect *fbo_rect = &gl->fbo_rect[i];
            if (fbo_rect)
            {
               unsigned img_width   = fbo_rect->max_img_width;
               unsigned img_height  = fbo_rect->max_img_height;

               if ((img_width  > fbo_rect->width) ||
                     (img_height > fbo_rect->height))
               {
                  /* Check proactively since we might suddently
                   * get sizes of tex_w width or tex_h height. */
                  unsigned max                    = img_width > img_height ? img_width : img_height;
                  unsigned pow2_size              = next_pow2(max);
                  bool update_feedback            = gl->fbo_feedback_enable
                     && (unsigned)i == gl->fbo_feedback_pass;

                  fbo_rect->width                 = pow2_size;
                  fbo_rect->height                = pow2_size;

                  gl2_recreate_fbo(fbo_rect, chain->fbo[i], &chain->fbo_texture[i]);

                  /* Update feedback texture in-place so we avoid having to
                   * juggle two different fbo_rect structs since they get updated here. */
                  if (update_feedback)
                  {
                     if (gl2_recreate_fbo(fbo_rect, gl->fbo_feedback,
                              &gl->fbo_feedback_texture))
                     {
                        /* Make sure the feedback textures are cleared
                         * so we don't feedback noise. */
                        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                     }
                  }

                  RARCH_LOG("[GL]: Recreating FBO texture #%d: %ux%u\n",
                        i, fbo_rect->width, fbo_rect->height);
               }
            }
         }

         /* Go back to what we're supposed to do,
          * render to FBO #0. */
         gl2_renderchain_start_render(gl, chain, video_info);
      }
      else
         gl2_set_viewport(gl, video_info, width, height, false, true);

#ifdef HAVE_VIDEO_LAYOUT
      gl->video_layout_resize = true;
#endif
   }

#ifdef HAVE_VIDEO_LAYOUT
   gl2_video_layout_viewport(gl, video_info);
#endif

   if (frame)
      gl->tex_index = ((gl->tex_index + 1) % gl->textures);

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

   /* Can be NULL for frame dupe / NULL render. */
   if (frame)
   {
      if (!gl->hw_render_fbo_init)
      {
         gl2_update_input_size(gl, frame_width, frame_height, pitch, true);

         gl2_renderchain_copy_frame(gl, chain,
               video_info, frame, frame_width, frame_height, pitch);
      }

      /* No point regenerating mipmaps
       * if there are no new frames. */
      if (gl->tex_mipmap && gl->have_mipmap)
         glGenerateMipmap(GL_TEXTURE_2D);
   }

   /* Have to reset rendering state which libretro core
    * could easily have overridden. */
   if (gl->hw_render_fbo_init)
   {
      gl2_update_input_size(gl, frame_width, frame_height, pitch, false);
      if (!gl->fbo_inited)
      {
         gl2_renderchain_bind_backbuffer();
         gl2_set_viewport(gl, video_info, width, height, false, true);
      }

      gl2_renderchain_restore_default_state(gl);

      glDisable(GL_STENCIL_TEST);
      glDisable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendEquation(GL_FUNC_ADD);
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   }

   gl->tex_info.tex           = gl->texture[gl->tex_index];
   gl->tex_info.input_size[0] = frame_width;
   gl->tex_info.input_size[1] = frame_height;
   gl->tex_info.tex_size[0]   = gl->tex_w;
   gl->tex_info.tex_size[1]   = gl->tex_h;

   feedback_info              = gl->tex_info;

   if (gl->fbo_feedback_enable)
   {
      const struct video_fbo_rect
         *rect                        = &gl->fbo_rect[gl->fbo_feedback_pass];
      GLfloat xamt                    = (GLfloat)rect->img_width / rect->width;
      GLfloat yamt                    = (GLfloat)rect->img_height / rect->height;

      feedback_info.tex               = gl->fbo_feedback_texture;
      feedback_info.input_size[0]     = rect->img_width;
      feedback_info.input_size[1]     = rect->img_height;
      feedback_info.tex_size[0]       = rect->width;
      feedback_info.tex_size[1]       = rect->height;

      set_texture_coords(feedback_info.coord, xamt, yamt);
   }

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

   gl->shader->set_params(&params, gl->shader_data);

   gl->coords.vertices  = 4;

   gl->shader->set_coords(gl->shader_data, &gl->coords);
   gl->shader->set_mvp(gl->shader_data, &gl->mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   if (gl->fbo_inited)
      gl2_renderchain_render(gl,
            chain,
            video_info,
            frame_count, &gl->tex_info, &feedback_info);

   /* Set prev textures. */
   gl2_renderchain_bind_prev_texture(gl,
         chain, &gl->tex_info);

#ifdef HAVE_VIDEO_LAYOUT
   gl2_video_layout_render(gl, video_info);
#endif

#if defined(HAVE_MENU)
   if (gl->menu_texture_enable)
   {
      menu_driver_frame(video_info);

      if (gl->menu_texture)
         gl2_draw_texture(gl, video_info);
   }
   else if (video_info->statistics_show)
   {
      struct font_params *osd_params = (struct font_params*)
         &video_info->osd_stat_params;

      if (osd_params)
         font_driver_render_msg(gl, video_info, video_info->stat_text,
               (const struct font_params*)&video_info->osd_stat_params, NULL);
   }
#endif

#ifdef HAVE_OVERLAY
   if (gl->overlay_enable)
      gl2_render_overlay(gl, video_info);
#endif

#ifdef HAVE_MENU_WIDGETS
   if (video_info->widgets_inited)
      menu_widgets_frame(video_info);
#endif

   if (!string_is_empty(msg))
   {
      if (video_info->msg_bgcolor_enable)
         gl2_render_osd_background(gl, video_info, msg);
      font_driver_render_msg(gl, video_info, msg, NULL, NULL);
   }

   if (video_info->cb_update_window_title)
      video_info->cb_update_window_title(
            video_info->context_data, video_info);

   /* Reset state which could easily mess up libretro core. */
   if (gl->hw_render_fbo_init)
   {
      gl->shader->use(gl, gl->shader_data, 0, true);
      glBindTexture(GL_TEXTURE_2D, 0);
   }

   /* Screenshots. */
   if (gl->readback_buffer_screenshot)
      gl2_renderchain_readback(gl,
            chain,
            4, GL_RGBA, GL_UNSIGNED_BYTE,
            gl->readback_buffer_screenshot);

   /* Don't readback if we're in menu mode. */
   else if (gl->pbo_readback_enable)
#ifdef HAVE_MENU
         /* Don't readback if we're in menu mode. */
         if (!gl->menu_texture_enable)
#endif
            gl2_pbo_async_readback(gl);

   /* emscripten has to do black frame insertion in its main loop */
#ifndef EMSCRIPTEN
   /* Disable BFI during fast forward, slow-motion,
    * and pause to prevent flicker. */
   if (
         video_info->black_frame_insertion
         && !video_info->input_driver_nonblock_state
         && !video_info->runloop_is_slowmotion
         && !video_info->runloop_is_paused)
   {
      video_info->cb_swap_buffers(video_info->context_data, video_info);
      glClear(GL_COLOR_BUFFER_BIT);
   }
#endif

   video_info->cb_swap_buffers(video_info->context_data, video_info);

   /* check if we are fast forwarding or in menu, if we are ignore hard sync */
   if (  gl->have_sync
         && video_info->hard_sync
         && !video_info->input_driver_nonblock_state
         && !gl->menu_texture_enable)
   {
      glClear(GL_COLOR_BUFFER_BIT);

      gl2_renderchain_fence_iterate(gl, chain,
            video_info->hard_sync_frames);
   }

#ifndef HAVE_OPENGLES
   if (gl->core_context_in_use)
      glBindVertexArray(0);
#endif
   gl2_context_bind_hw_render(gl, true);
   return true;
}

static void gl2_destroy_resources(gl_t *gl)
{
   if (gl)
   {
      if (gl->empty_buf)
         free(gl->empty_buf);
      if (gl->conv_buffer)
         free(gl->conv_buffer);
      free(gl);
   }

   gl_shared_context_use   = false;

   gl_query_core_context_unset();
}

static void gl2_deinit_chain(gl_t *gl)
{
   if (!gl)
      return;

   if (gl->renderchain_data)
      free(gl->renderchain_data);
   gl->renderchain_data   = NULL;
}

static void gl2_free(void *data)
{
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;

#ifdef HAVE_VIDEO_LAYOUT
   gl2_video_layout_free(gl);
#endif

   gl2_context_bind_hw_render(gl, false);

   if (gl->have_sync)
      gl2_renderchain_fence_free(gl,
            (gl2_renderchain_data_t*)
            gl->renderchain_data);

   font_driver_free_osd();

   gl->shader->deinit(gl->shader_data);

   glDeleteTextures(gl->textures, gl->texture);

#if defined(HAVE_MENU)
   if (gl->menu_texture)
      glDeleteTextures(1, &gl->menu_texture);
#endif

#ifdef HAVE_OVERLAY
   gl2_free_overlay(gl);
#endif

#if defined(HAVE_PSGL)
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);
#endif

   scaler_ctx_gen_reset(&gl->scaler);

   if (gl->pbo_readback_enable)
   {
      glDeleteBuffers(4, gl->pbo_readback);
      scaler_ctx_gen_reset(&gl->pbo_readback_scaler);
   }

#ifndef HAVE_OPENGLES
   if (gl->core_context_in_use)
   {
      gl2_renderchain_data_t *chain = (gl2_renderchain_data_t*)
         gl->renderchain_data;

      glBindVertexArray(0);
      glDeleteVertexArrays(1, &chain->vao);
   }
#endif

   gl2_renderchain_deinit_fbo(gl, (gl2_renderchain_data_t*)gl->renderchain_data);
   gl2_renderchain_deinit_hw_render(gl, (gl2_renderchain_data_t*)gl->renderchain_data);
   gl2_deinit_chain(gl);

   video_context_driver_free();

   gl2_destroy_resources(gl);
}

static void gl2_set_nonblock_state(void *data, bool state)
{
   int interval                = 0;
   gl_t             *gl        = (gl_t*)data;
   settings_t        *settings = config_get_ptr();

   if (!gl)
      return;

   RARCH_LOG("[GL]: VSync => %s\n", state ? "off" : "on");

   gl2_context_bind_hw_render(gl, false);

   if (!state)
      interval = settings->uints.video_swap_interval;

   if (gl->ctx_driver->swap_interval)
   {
      bool adaptive_vsync_enabled            = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && settings->bools.video_adaptive_vsync;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      gl->ctx_driver->swap_interval(gl->ctx_data, interval);
   }
   gl2_context_bind_hw_render(gl, true);
}

static bool gl2_resolve_extensions(gl_t *gl, const char *context_ident, const video_info_t *video)
{
   settings_t *settings          = config_get_ptr();

   /* have_es2_compat - GL_RGB565 internal format support.
    * Even though ES2 support is claimed, the format
    * is not supported on older ATI catalyst drivers.
    *
    * The speed gain from using GL_RGB565 is worth
    * adding some workarounds for.
    *
    * have_sync       - Use ARB_sync to reduce latency.
    */
   gl->have_full_npot_support    = gl_check_capability(GL_CAPS_FULL_NPOT_SUPPORT);
   gl->have_mipmap               = gl_check_capability(GL_CAPS_MIPMAP);
   gl->have_es2_compat           = gl_check_capability(GL_CAPS_ES2_COMPAT);
   gl->support_unpack_row_length = gl_check_capability(GL_CAPS_UNPACK_ROW_LENGTH);
   gl->have_sync                 = gl_check_capability(GL_CAPS_SYNC);

   if (gl->have_sync && settings->bools.video_hard_sync)
      RARCH_LOG("[GL]: Using ARB_sync to reduce latency.\n");

   video_driver_unset_rgba();

   gl2_renderchain_resolve_extensions(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data,
         context_ident, video);

#if defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
   if (!gl_check_capability(GL_CAPS_BGRA8888))
   {
      video_driver_set_rgba();
      RARCH_WARN("[GL]: GLES implementation does not have BGRA8888 extension.\n"
                 "32-bit path will require conversion.\n");
   }
   /* TODO/FIXME - No extensions for float FBO currently. */
#endif

#ifdef GL_DEBUG
   /* Useful for debugging, but kinda obnoxious otherwise. */
   RARCH_LOG("[GL]: Supported extensions:\n");

   if (gl->core_context_in_use)
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

static INLINE void gl2_set_texture_fmts(gl_t *gl, bool rgb32)
{
   gl->internal_fmt = RARCH_GL_INTERNAL_FORMAT16;
   gl->texture_type = RARCH_GL_TEXTURE_TYPE16;
   gl->texture_fmt  = RARCH_GL_FORMAT16;
   gl->base_size    = sizeof(uint16_t);

   if (rgb32)
   {
      bool use_rgba    = video_driver_supports_rgba();

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

static bool gl2_init_pbo_readback(gl_t *gl)
{
#if !defined(HAVE_OPENGLES2) && !defined(HAVE_PSGL)
   unsigned i;

   glGenBuffers(4, gl->pbo_readback);

   for (i = 0; i < 4; i++)
   {
      gl2_renderchain_bind_pbo(gl->pbo_readback[i]);
      gl2_renderchain_init_pbo(gl->vp.width *
            gl->vp.height * sizeof(uint32_t), NULL);
   }
   gl2_renderchain_unbind_pbo();

#ifndef HAVE_OPENGLES3
   {
      struct scaler_ctx *scaler = &gl->pbo_readback_scaler;
      scaler->in_width          = gl->vp.width;
      scaler->in_height         = gl->vp.height;
      scaler->out_width         = gl->vp.width;
      scaler->out_height        = gl->vp.height;
      scaler->in_stride         = gl->vp.width * sizeof(uint32_t);
      scaler->out_stride        = gl->vp.width * 3;
      scaler->in_fmt            = SCALER_FMT_ARGB8888;
      scaler->out_fmt           = SCALER_FMT_BGR24;
      scaler->scaler_type       = SCALER_TYPE_POINT;

      if (!scaler_ctx_gen_filter(scaler))
      {
         gl->pbo_readback_enable = false;
         RARCH_ERR("[GL]: Failed to initialize pixel conversion for PBO.\n");
         glDeleteBuffers(4, gl->pbo_readback);
         return false;
      }
   }
#endif

   return true;
#else
   /* If none of these are bound, we have to assume
    * we are not going to use PBOs */
   return false;
#endif
}

static const gfx_ctx_driver_t *gl2_get_context(gl_t *gl)
{
   enum gfx_ctx_api api;
   const gfx_ctx_driver_t *gfx_ctx      = NULL;
   void                      *ctx_data  = NULL;
   const char                 *api_name = NULL;
   settings_t                 *settings = config_get_ptr();
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
   unsigned major                       = hwr->version_major;
   unsigned minor                       = hwr->version_minor;

#ifdef HAVE_OPENGLES
   api                                  = GFX_CTX_OPENGL_ES_API;
   api_name                             = "OpenGL ES 2.0";

   if (hwr->context_type == RETRO_HW_CONTEXT_OPENGLES3)
   {
      major                             = 3;
      minor                             = 0;
      api_name                          = "OpenGL ES 3.0";
   }
   else if (hwr->context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION)
      api_name                          = "OpenGL ES 3.1+";
#else
   api                                  = GFX_CTX_OPENGL_API;
   api_name                             = "OpenGL";
#endif

   (void)api_name;

   gl_shared_context_use = settings->bools.video_shared_context
      && hwr->context_type != RETRO_HW_CONTEXT_NONE;

   if (     (libretro_get_shared_context())
         && (hwr->context_type != RETRO_HW_CONTEXT_NONE))
      gl_shared_context_use = true;

   gfx_ctx = video_context_driver_init_first(gl,
         settings->arrays.video_context_driver,
         api, major, minor, gl_shared_context_use, &ctx_data);

   if (ctx_data)
      gl->ctx_data = ctx_data;

   return gfx_ctx;
}

#ifdef GL_DEBUG
#ifdef HAVE_GL_DEBUG_ES
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

static void DEBUG_CALLBACK_TYPE gl2_debug_cb(GLenum source, GLenum type,
      GLuint id, GLenum severity, GLsizei length,
      const GLchar *message, void *userParam)
{
   const char      *src = NULL;
   const char *typestr  = NULL;
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

static void gl2_begin_debug(gl_t *gl)
{
   if (gl_check_capability(GL_CAPS_DEBUG))
   {
#ifdef HAVE_GL_DEBUG_ES
      glDebugMessageCallbackKHR(gl2_debug_cb, gl);
      glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
#else
      glDebugMessageCallback(gl2_debug_cb, gl);
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
   }
   else
      RARCH_ERR("[GL]: Neither GL_KHR_debug nor GL_ARB_debug_output are implemented. Cannot start GL debugging.\n");
}
#endif

static bool renderchain_gl2_init_first(void **renderchain_handle)
{
   gl2_renderchain_data_t *data = (gl2_renderchain_data_t *)calloc(1, sizeof(*data));

   if (!data)
      return false;

   *renderchain_handle = data;
   return true;
}

static void *gl2_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   enum gfx_wrap_type wrap_type;
   gfx_ctx_mode_t mode;
   gfx_ctx_input_t inp;
   unsigned full_x, full_y;
   video_shader_ctx_info_t shader_info;
   settings_t *settings                 = config_get_ptr();
   int interval                         = 0;
   unsigned mip_level                   = 0;
   unsigned win_width                   = 0;
   unsigned win_height                  = 0;
   unsigned temp_width                  = 0;
   unsigned temp_height                 = 0;
   bool force_smooth                    = false;
   const char *vendor                   = NULL;
   const char *renderer                 = NULL;
   const char *version                  = NULL;
   struct retro_hw_render_callback *hwr = NULL;
   char *error_string                   = NULL;
   gl_t *gl                             = (gl_t*)calloc(1, sizeof(gl_t));
   const gfx_ctx_driver_t *ctx_driver   = gl2_get_context(gl);

   if (!gl || !ctx_driver)
      goto error;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   gl->ctx_driver                       = ctx_driver;
   gl->video_info                       = *video;

   RARCH_LOG("[GL]: Found GL context: %s\n", ctx_driver->ident);

   video_context_driver_get_video_size(&mode);
#if defined(DINGUX)
   mode.width = 320;
   mode.height = 240;
#endif
   full_x      = mode.width;
   full_y      = mode.height;
   mode.width  = 0;
   mode.height = 0;
   interval    = 0;

   RARCH_LOG("[GL]: Detecting screen resolution %ux%u.\n", full_x, full_y);

   if (video->vsync)
      interval = video->swap_interval;

   if (gl->ctx_driver->swap_interval)
   {
      bool adaptive_vsync_enabled            = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && video->adaptive_vsync;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      gl->ctx_driver->swap_interval(gl->ctx_data, interval);
   }

   win_width   = video->width;
   win_height  = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }

   mode.width      = win_width;
   mode.height     = win_height;
   mode.fullscreen = video->fullscreen;

   if (!video_context_driver_set_video_mode(&mode))
      goto error;
#if defined(__APPLE__) && !defined(IOS)
   /* This is a hack for now to work around a very annoying
    * issue that currently eludes us. */
   if (!video_context_driver_set_video_mode(&mode))
      goto error;
#endif

#if !defined(RARCH_CONSOLE) || defined(HAVE_LIBNX)
   rglgen_resolve_symbols(ctx_driver->get_proc_address);
#endif

   /* Clear out potential error flags in case we use cached context. */
   glGetError();

   vendor   = (const char*)glGetString(GL_VENDOR);
   renderer = (const char*)glGetString(GL_RENDERER);
   version  = (const char*)glGetString(GL_VERSION);

   RARCH_LOG("[GL]: Vendor: %s, Renderer: %s.\n", vendor, renderer);
   RARCH_LOG("[GL]: Version: %s.\n", version);

   if (string_is_equal(ctx_driver->ident, "null"))
      goto error;

   if (!string_is_empty(version))
      sscanf(version, "%d.%d", &gl->version_major, &gl->version_minor);

   {
      char device_str[128];

      device_str[0] = '\0';

      strlcpy(device_str, vendor, sizeof(device_str));
      strlcat(device_str, " ", sizeof(device_str));
      strlcat(device_str, renderer, sizeof(device_str));

      video_driver_set_gpu_device_string(device_str);
      video_driver_set_gpu_api_version_string(version);
   }

#ifdef _WIN32
   if (string_is_equal(vendor, "Microsoft Corporation"))
      if (string_is_equal(renderer, "GDI Generic"))
#ifdef HAVE_OPENGL1
         retroarch_force_video_driver_fallback("gl1");
#else
         retroarch_force_video_driver_fallback("gdi");
#endif
#endif

   hwr = video_driver_get_hw_context();

   if (hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE)
   {
      gl_query_core_context_set(true);
      gl->core_context_in_use = true;

      gl_set_core_context(hwr->context_type);

      RARCH_LOG("[GL]: Using Core GL context, setting up VAO...\n");
      if (!gl_check_capability(GL_CAPS_VAO))
      {
         RARCH_ERR("[GL]: Failed to initialize VAOs.\n");
         goto error;
      }
   }

   if (!renderchain_gl2_init_first(&gl->renderchain_data))
   {
      RARCH_ERR("[GL]: Renderchain could not be initialized.\n");
      goto error;
   }

   gl2_renderchain_restore_default_state(gl);

#ifndef HAVE_OPENGLES
   if (hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE)
   {
      gl2_renderchain_data_t *chain = (gl2_renderchain_data_t*)
         gl->renderchain_data;

      glGenVertexArrays(1, &chain->vao);
   }
#endif

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   gl->hw_render_use    = false;
   gl->has_fbo          = gl_check_capability(GL_CAPS_FBO);

   if (gl->has_fbo && hwr->context_type != RETRO_HW_CONTEXT_NONE)
      gl->hw_render_use = true;

   if (!gl2_resolve_extensions(gl, ctx_driver->ident, video))
      goto error;

#ifdef GL_DEBUG
   gl2_begin_debug(gl);
#endif

   gl->vsync      = video->vsync;
   gl->fullscreen = video->fullscreen;

   mode.width     = 0;
   mode.height    = 0;

   video_context_driver_get_video_size(&mode);

#if defined(DINGUX)
   mode.width = 320;
   mode.height = 240;
#endif
   temp_width     = mode.width;
   temp_height    = mode.height;
   mode.width     = 0;
   mode.height    = 0;

   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   video_driver_get_size(&temp_width, &temp_height);
   gl->video_width       = temp_width;
   gl->video_height      = temp_height;

   RARCH_LOG("[GL]: Using resolution %ux%u\n", temp_width, temp_height);

   gl->vertex_ptr        = hwr->bottom_left_origin
      ? vertexes : vertexes_flipped;

   /* Better pipelining with GPU due to synchronous glSubTexImage.
    * Multiple async PBOs would be an alternative,
    * but still need multiple textures with PREV.
    */
   gl->textures         = 4;

   if (gl->hw_render_use)
   {
      /* All on GPU, no need to excessively
       * create textures. */
      gl->textures = 1;
#ifdef GL_DEBUG
      gl2_context_bind_hw_render(gl, true);
      gl2_begin_debug(gl);
      gl2_context_bind_hw_render(gl, false);
#endif
   }

   gl->white_color_ptr = white_color;

   gl->shader          = (shader_backend_t*)gl2_shader_ctx_drivers[0];

   if (!gl->shader)
   {
      RARCH_ERR("[GL:]: Shader driver initialization failed.\n");
      goto error;
   }

   RARCH_LOG("[GL]: Default shader backend found: %s.\n", gl->shader->ident);

   if (!gl2_shader_init(gl, ctx_driver, hwr))
   {
      RARCH_ERR("[GL]: Shader initialization failed.\n");
      goto error;
   }

   {
      unsigned texture_info_id = gl->shader->get_prev_textures(gl->shader_data);
      unsigned minimum         = texture_info_id;
      gl->textures             = MAX(minimum + 1, gl->textures);
   }

   if (!gl2_shader_info(gl, &shader_info))
   {
      RARCH_ERR("[GL]: Shader driver info check failed.\n");
      goto error;
   }

   RARCH_LOG("[GL]: Using %u textures.\n", gl->textures);
   RARCH_LOG("[GL]: Loaded %u program(s).\n",
         shader_info.num);

   gl->tex_w = gl->tex_h = (RARCH_SCALE_BASE * video->input_scale);
   gl->keep_aspect     = video->force_aspect;

   /* Apparently need to set viewport for passes
    * when we aren't using FBOs. */
   gl2_set_shader_viewports(gl);

   mip_level            = 1;
   gl->tex_mipmap       = gl->shader->mipmap_input(gl->shader_data, mip_level);

   if (gl->shader->filter_type(gl->shader_data,
            1, &force_smooth))
      gl->tex_min_filter = gl->tex_mipmap ? (force_smooth ?
            GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
         : (force_smooth ? GL_LINEAR : GL_NEAREST);
   else
      gl->tex_min_filter = gl->tex_mipmap ?
         (video->smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
         : (video->smooth ? GL_LINEAR : GL_NEAREST);

   gl->tex_mag_filter = gl2_min_filter_to_mag(gl->tex_min_filter);

   wrap_type     = gl->shader->wrap_type(
         gl->shader_data, 1);

   gl->wrap_mode      = gl2_wrap_type_to_enum(wrap_type);

   gl2_set_texture_fmts(gl, video->rgb32);

   memcpy(gl->tex_info.coord, tex_coords, sizeof(gl->tex_info.coord));
   gl->coords.vertex         = gl->vertex_ptr;
   gl->coords.tex_coord      = gl->tex_info.coord;
   gl->coords.color          = gl->white_color_ptr;
   gl->coords.lut_tex_coord  = tex_coords;
   gl->coords.vertices       = 4;

   /* Empty buffer that we use to clear out
    * the texture with on res change. */
   gl->empty_buf             = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

   gl->conv_buffer           = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

   if (!gl->conv_buffer)
      goto error;

   gl2_init_textures(gl, video);
   gl2_init_textures_data(gl);

   gl2_renderchain_init(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data,
         gl->tex_w, gl->tex_h);

   if (gl->has_fbo)
   {
      if (gl->hw_render_use &&
            !gl2_renderchain_init_hw_render(gl, (gl2_renderchain_data_t*)gl->renderchain_data, gl->tex_w, gl->tex_h))
      {
         RARCH_ERR("[GL]: Hardware rendering context initialization failed.\n");
         goto error;
      }
   }

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);

   if (video->font_enable)
      font_driver_init_osd(gl, false,
            video->is_threaded,
            FONT_DRIVER_RENDER_OPENGL_API);

   /* Only bother with PBO readback if we're doing GPU recording.
    * Check recording_is_enabled() and not
    * driver.recording_data, because recording is
    * not initialized yet.
    */
   gl->pbo_readback_enable = settings->bools.video_gpu_record
      && recording_is_enabled();

   if (gl->pbo_readback_enable && gl2_init_pbo_readback(gl))
   {
      RARCH_LOG("[GL]: Async PBO readback enabled.\n");
   }

   if (!gl_check_error(&error_string))
   {
      RARCH_ERR("%s\n", error_string);
      free(error_string);
      goto error;
   }

#ifdef HAVE_VIDEO_LAYOUT
   gl2_video_layout_init(gl);
#endif

   gl2_context_bind_hw_render(gl, true);

   return gl;

error:
   video_context_driver_destroy();
   gl2_destroy_resources(gl);
   return NULL;
}

static bool gl2_alive(void *data)
{
   bool ret             = false;
   bool quit            = false;
   bool resize          = false;
   gl_t         *gl     = (gl_t*)data;
   bool is_shutdown     = rarch_ctl(RARCH_CTL_IS_SHUTDOWN, NULL);
   unsigned temp_width  = gl->video_width;
   unsigned temp_height = gl->video_height;

   gl->ctx_driver->check_window(gl->ctx_data,
         &quit, &resize, &temp_width, &temp_height, is_shutdown);

   if (quit)
      gl->quitting = true;
   else if (resize)
      gl->should_resize = true;

   ret = !gl->quitting;

   if (temp_width != 0 && temp_height != 0)
   {
      video_driver_set_size(&temp_width, &temp_height);
      gl->video_width  = temp_width;
      gl->video_height = temp_height;
   }

   return ret;
}

static bool gl2_suppress_screensaver(void *data, bool enable)
{
   bool enabled = enable;
   gl_t *gl     = (gl_t*)data;

   if (gl->ctx_data && gl->ctx_driver->suppress_screensaver)
      return gl->ctx_driver->suppress_screensaver(gl->ctx_data, enabled);
   return false;
}

static void gl2_update_tex_filter_frame(gl_t *gl)
{
   unsigned i, mip_level;
   GLenum wrap_mode;
   GLuint new_filt;
   enum gfx_wrap_type wrap_type;
   bool smooth                       = false;
   settings_t *settings              = config_get_ptr();

   gl2_context_bind_hw_render(gl, false);

   if (!gl->shader->filter_type(gl->shader_data,
            1, &smooth))
      smooth             = settings->bools.video_smooth;

   mip_level             = 1;
   wrap_type             = gl->shader->wrap_type(gl->shader_data, 1);
   wrap_mode             = gl2_wrap_type_to_enum(wrap_type);
   gl->tex_mipmap        = gl->shader->mipmap_input(gl->shader_data, mip_level);
   gl->video_info.smooth = smooth;
   new_filt              = gl->tex_mipmap ? (smooth ?
         GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
      : (smooth ? GL_LINEAR : GL_NEAREST);

   if (new_filt == gl->tex_min_filter && wrap_mode == gl->wrap_mode)
      return;

   gl->tex_min_filter    = new_filt;
   gl->tex_mag_filter    = gl2_min_filter_to_mag(gl->tex_min_filter);
   gl->wrap_mode         = wrap_mode;

   for (i = 0; i < gl->textures; i++)
   {
      if (!gl->texture[i])
         continue;

      gl_bind_texture(gl->texture[i], gl->wrap_mode, gl->tex_mag_filter,
            gl->tex_min_filter);
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   gl2_context_bind_hw_render(gl, true);
}

static bool gl2_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_GLSL) || defined(HAVE_CG)
   unsigned textures;
   video_shader_ctx_init_t init_data;
   enum rarch_shader_type fallback;
   gl_t *gl = (gl_t*)data;

   if (!gl)
      return false;

   gl2_context_bind_hw_render(gl, false);

   fallback = gl2_get_fallback_shader_type(type);

   if (fallback == RARCH_SHADER_NONE)
   {
      RARCH_ERR("[GL]: No supported shader backend found!\n");
      goto error;
   }

   gl->shader->deinit(gl->shader_data);
   gl->shader_data = NULL;

   if (type != fallback)
   {
      RARCH_ERR("[GL]: %s shader not supported, falling back to stock %s\n",
            video_shader_to_str(type), video_shader_to_str(fallback));
      path = NULL;
   }

   if (gl->fbo_inited)
   {
      gl2_renderchain_deinit_fbo(gl,
            (gl2_renderchain_data_t*)gl->renderchain_data);

      glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   }

   init_data.shader_type = fallback;
   init_data.shader      = NULL;
   init_data.data        = gl;
   init_data.path        = path;

   if (!gl_shader_driver_init(&init_data))
   {
      init_data.path = NULL;

      gl_shader_driver_init(&init_data);

      gl->shader       = init_data.shader;
      gl->shader_data  = init_data.shader_data;

      RARCH_WARN("[GL]: Failed to set multipass shader. Falling back to stock.\n");

      goto error;
   }

   gl->shader       = init_data.shader;
   gl->shader_data  = init_data.shader_data;

   gl2_update_tex_filter_frame(gl);

   {
      unsigned texture_info_id = gl->shader->get_prev_textures(gl->shader_data);
      textures                 = texture_info_id + 1;
   }

   if (textures > gl->textures) /* Have to reinit a bit. */
   {
      if (gl->hw_render_use && gl->fbo_inited)
         gl2_renderchain_deinit_hw_render(gl, (gl2_renderchain_data_t*)
               gl->renderchain_data);

      glDeleteTextures(gl->textures, gl->texture);
#if defined(HAVE_PSGL)
      glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
      glDeleteBuffers(1, &gl->pbo);
#endif
      gl->textures = textures;
      RARCH_LOG("[GL]: Using %u textures.\n", gl->textures);
      gl->tex_index = 0;
      gl2_init_textures(gl, &gl->video_info);
      gl2_init_textures_data(gl);

      if (gl->hw_render_use)
         gl2_renderchain_init_hw_render(gl,
               (gl2_renderchain_data_t*)gl->renderchain_data,
               gl->tex_w, gl->tex_h);
   }

   gl2_renderchain_init(gl,
         (gl2_renderchain_data_t*)gl->renderchain_data,
         gl->tex_w, gl->tex_h);

   /* Apparently need to set viewport for passes when we aren't using FBOs. */
   gl2_set_shader_viewports(gl);
   gl2_context_bind_hw_render(gl, true);

   return true;

error:
   gl2_context_bind_hw_render(gl, true);
#endif
   return false;
}

static void gl2_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned top_y, top_dist;
   gl_t *gl        = (gl_t*)data;
   unsigned width  = gl->video_width;
   unsigned height = gl->video_height;

   *vp             = gl->vp;
   vp->full_width  = width;
   vp->full_height = height;

   /* Adjust as GL viewport is bottom-up. */
   top_y           = vp->y + vp->height;
   top_dist        = height - top_y;
   vp->y           = top_dist;
}

static bool gl2_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   gl_t *gl             = (gl_t*)data;

   if (!gl)
      return false;

   return gl2_renderchain_read_viewport(gl, buffer, is_idle);
}

#if 0
#define READ_RAW_GL_FRAME_TEST
#endif

#if defined(READ_RAW_GL_FRAME_TEST)
static void* gl2_read_frame_raw(void *data, unsigned *width_p,
unsigned *height_p, size_t *pitch_p)
{
   gl_t *gl             = (gl_t*)data;
   unsigned width       = gl->last_width[gl->tex_index];
   unsigned height      = gl->last_height[gl->tex_index];
   size_t pitch         = gl->tex_w * gl->base_size;
   void* buffer         = NULL;
   void* buffer_texture = NULL;

   if (gl->hw_render_use)
   {
      buffer = malloc(pitch * height);
      if (!buffer)
         return NULL;
   }

   buffer_texture = malloc(pitch * gl->tex_h);

   if (!buffer_texture)
   {
      if (buffer)
         free(buffer);
      return NULL;
   }

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   glGetTexImage(GL_TEXTURE_2D, 0,
         gl->texture_type, gl->texture_fmt, buffer_texture);

   *width_p  = width;
   *height_p = height;
   *pitch_p  = pitch;

   if (gl->hw_render_use)
   {
      unsigned i;

      for(i = 0; i < height ; i++)
         memcpy((uint8_t*)buffer + i * pitch,
            (uint8_t*)buffer_texture + (height - 1 - i) * pitch, pitch);

      free(buffer_texture);
      return buffer;
   }

   return buffer_texture;
}
#endif

#ifdef HAVE_OVERLAY
static bool gl2_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, j;
   gl_t *gl = (gl_t*)data;
   const struct texture_image *images =
      (const struct texture_image*)image_data;

   if (!gl)
      return false;

   gl2_context_bind_hw_render(gl, false);

   gl2_free_overlay(gl);
   gl->overlay_tex = (GLuint*)
      calloc(num_images, sizeof(*gl->overlay_tex));

   if (!gl->overlay_tex)
   {
      gl2_context_bind_hw_render(gl, true);
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

   gl->overlays = num_images;
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
      gl2_overlay_tex_geom(gl, i, 0, 0, 1, 1);
      gl2_overlay_vertex_geom(gl, i, 0, 0, 1, 1);

      for (j = 0; j < 16; j++)
         gl->overlay_color_coord[16 * i + j] = 1.0f;
   }

   gl2_context_bind_hw_render(gl, true);
   return true;
}

static void gl2_overlay_enable(void *data, bool state)
{
   gl_t *gl = (gl_t*)data;

   if (!gl)
      return;

   gl->overlay_enable = state;

   if (gl->fullscreen && gl->ctx_driver->show_mouse)
      gl->ctx_driver->show_mouse(gl->ctx_data, state);
}

static void gl2_overlay_full_screen(void *data, bool enable)
{
   gl_t *gl = (gl_t*)data;

   if (gl)
      gl->overlay_full_screen = enable;
}

static void gl2_overlay_set_alpha(void *data, unsigned image, float mod)
{
   GLfloat *color;
   gl_t *gl = (gl_t*)data;

   if (!gl)
      return;

   color = (GLfloat*)&gl->overlay_color_coord[image * 16];

   color[ 0 + 3] = mod;
   color[ 4 + 3] = mod;
   color[ 8 + 3] = mod;
   color[12 + 3] = mod;
}

static const video_overlay_interface_t gl2_overlay_interface = {
   gl2_overlay_enable,
   gl2_overlay_load,
   gl2_overlay_tex_geom,
   gl2_overlay_vertex_geom,
   gl2_overlay_full_screen,
   gl2_overlay_set_alpha,
};

static void gl2_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &gl2_overlay_interface;
}
#endif

static retro_proc_address_t gl2_get_proc_address(void *data, const char *sym)
{
   gl_t *gl = (gl_t*)data;

   if (gl && gl->ctx_driver->get_proc_address)
      return gl->ctx_driver->get_proc_address(sym);

   return NULL;
}

static void gl2_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl_t *gl = (gl_t*)data;

   if (!gl)
      return;

   gl->keep_aspect   = true;
   gl->should_resize = true;
}

static void gl2_apply_state_changes(void *data)
{
   gl_t *gl = (gl_t*)data;

   if (gl)
      gl->should_resize = true;
}

static void gl2_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_size_t size_data;
   size_data.width  = width;
   size_data.height = height;
   video_context_driver_get_video_output_size(&size_data);
}

static void gl2_get_video_output_prev(void *data)
{
   video_context_driver_get_video_output_prev();
}

static void gl2_get_video_output_next(void *data)
{
   video_context_driver_get_video_output_next();
}

static void video_texture_load_gl2(
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

#ifdef HAVE_THREADS
static int video_texture_load_wrap_gl2_mipmap(void *data)
{
   uintptr_t id = 0;

   if (!data)
      return 0;
   video_texture_load_gl2((struct texture_image*)data,
         TEXTURE_FILTER_MIPMAP_LINEAR, &id);
   return (int)id;
}

static int video_texture_load_wrap_gl2(void *data)
{
   uintptr_t id = 0;

   if (!data)
      return 0;
   video_texture_load_gl2((struct texture_image*)data,
         TEXTURE_FILTER_LINEAR, &id);
   return (int)id;
}
#endif

static uintptr_t gl2_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;

#ifdef HAVE_THREADS
   if (threaded)
   {
      custom_command_method_t func = video_texture_load_wrap_gl2;

      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            func = video_texture_load_wrap_gl2_mipmap;
            break;
         default:
            break;
      }
      return video_thread_texture_load(data, func);
   }
#endif

   video_texture_load_gl2((struct texture_image*)data, filter_type, &id);
   return id;
}

static void gl2_unload_texture(void *data, uintptr_t id)
{
   GLuint glid;
   if (!id)
      return;

   glid = (GLuint)id;
   glDeleteTextures(1, &glid);
}

static float gl2_get_refresh_rate(void *data)
{
   float refresh_rate = 0.0f;
   if (video_context_driver_get_refresh_rate(&refresh_rate))
      return refresh_rate;
   return 0.0f;
}

static uint32_t gl2_get_flags(void *data)
{
   uint32_t             flags   = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_HARD_SYNC);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);

   return flags;
}

static const video_poke_interface_t gl2_poke_interface = {
   gl2_get_flags,
   gl2_load_texture,
   gl2_unload_texture,
   gl2_set_video_mode,
   gl2_get_refresh_rate,
   NULL,
   gl2_get_video_output_size,
   gl2_get_video_output_prev,
   gl2_get_video_output_next,
   gl2_get_current_framebuffer,
   gl2_get_proc_address,
   gl2_set_aspect_ratio,
   gl2_apply_state_changes,
   gl2_set_texture_frame,
   gl2_set_texture_enable,
   font_driver_render_msg,
   gl2_show_mouse,
   NULL,
   gl2_get_current_shader,
   NULL,                      /* get_current_software_framebuffer */
   NULL                       /* get_hw_render_interface */
};

static void gl2_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &gl2_poke_interface;
}

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
static bool gl2_menu_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

video_driver_t video_gl2 = {
   gl2_init,
   gl2_frame,
   gl2_set_nonblock_state,
   gl2_alive,
   NULL,                    /* focus */
   gl2_suppress_screensaver,
   NULL,                    /* has_windowed */

   gl2_set_shader,

   gl2_free,
   "gl",

   gl2_set_viewport_wrapper,
   gl2_set_rotation,

   gl2_viewport_info,

   gl2_read_viewport,
#if defined(READ_RAW_GL_FRAME_TEST)
   gl2_read_frame_raw,
#else
   NULL,
#endif

#ifdef HAVE_OVERLAY
   gl2_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
   gl2_get_video_layout_render_interface,
#endif
   gl2_get_poke_interface,
   gl2_wrap_type_to_enum,
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   gl2_menu_widgets_enabled
#endif
};
