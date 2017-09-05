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

#ifdef _MSC_VER
#pragma comment(lib, "opengl32")
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
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <string/stdstring.h>

#include <gfx/gl_capabilities.h>
#include <gfx/video_frame.h>

#include "gl_legacy_renderchain.h"
#include "../video_driver.h"
#include "../common/gl_common.h"

#include "../../driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#if defined(_WIN32) && !defined(_XBOX)
#include "../common/win32_common.h"
#endif

#define set_texture_coords(coords, xamt, yamt) \
   coords[2] = xamt; \
   coords[6] = xamt; \
   coords[5] = yamt; \
   coords[7] = yamt

#ifdef IOS
/* There is no default frame buffer on iOS. */
void cocoagl_bind_game_view_fbo(void);
#define gl_bind_backbuffer() cocoagl_bind_game_view_fbo()
#else
#define gl_bind_backbuffer() glBindFramebuffer(RARCH_GL_FRAMEBUFFER, 0)
#endif

/* Prototypes */
GLenum min_filter_to_mag(GLenum type);
void gl_set_viewport(
      void *data, video_frame_info_t *video_info, 
      unsigned viewport_width,
      unsigned viewport_height,
      bool force_full, bool allow_rotate);

#ifdef HAVE_FBO
void gl2_renderchain_convert_geometry(
      void *data,
      struct video_fbo_rect *fbo_rect,
      struct gfx_fbo_scale *fbo_scale,
      unsigned last_width, unsigned last_max_width,
      unsigned last_height, unsigned last_max_height,
      unsigned vp_width, unsigned vp_height)
{
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
}

static bool gl_recreate_fbo(
      struct video_fbo_rect *fbo_rect,
      GLuint fbo,
      GLuint* texture
      )
{
   glBindFramebuffer(RARCH_GL_FRAMEBUFFER, fbo);
   glDeleteTextures(1, texture);
   glGenTextures(1, texture);
   glBindTexture(GL_TEXTURE_2D, *texture);
   gl_load_texture_image(GL_TEXTURE_2D,
         0, RARCH_GL_INTERNAL_FORMAT32,
         fbo_rect->width,
         fbo_rect->height,
         0, RARCH_GL_TEXTURE_TYPE32,
         RARCH_GL_FORMAT32, NULL);

   glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER,
         RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
         *texture, 0);

   if (glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER) 
         == RARCH_GL_FRAMEBUFFER_COMPLETE)
      return true;

   RARCH_WARN("Failed to reinitialize FBO texture.\n");
   return false;
}

static void gl_check_fbo_dimension(gl_t *gl, unsigned i,
      bool update_feedback)
{
   struct video_fbo_rect *fbo_rect = &gl->fbo_rect[i];
   /* Check proactively since we might suddently 
    * get sizes of tex_w width or tex_h height. */
   unsigned img_width              = fbo_rect->max_img_width;
   unsigned img_height             = fbo_rect->max_img_height;
   unsigned max                    = img_width > img_height ? img_width : img_height;
   unsigned pow2_size              = next_pow2(max);

   fbo_rect->width                 = pow2_size;
   fbo_rect->height                = pow2_size;

   gl_recreate_fbo(fbo_rect, gl->fbo[i], &gl->fbo_texture[i]);

   /* Update feedback texture in-place so we avoid having to 
    * juggle two different fbo_rect structs since they get updated here. */
   if (update_feedback)
   {
      if (gl_recreate_fbo(fbo_rect, gl->fbo_feedback,
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

/* On resize, we might have to recreate our FBOs 
 * due to "Viewport" scale, and set a new viewport. */

void gl2_renderchain_check_fbo_dimensions(void *data)
{
   int i;
   gl_t *gl = (gl_t*)data;

   /* Check if we have to recreate our FBO textures. */
   for (i = 0; i < gl->fbo_pass; i++)
   {
      struct video_fbo_rect *fbo_rect = &gl->fbo_rect[i];
      if (fbo_rect)
      {
         bool update_feedback = gl->fbo_feedback_enable 
            && (unsigned)i == gl->fbo_feedback_pass;

         if ((fbo_rect->max_img_width  > fbo_rect->width) ||
             (fbo_rect->max_img_height > fbo_rect->height))
               gl_check_fbo_dimension(gl, i, update_feedback);
      }
   }
}

void gl2_renderchain_render(
      void *data,
      video_frame_info_t *video_info,
      uint64_t frame_count,
      const struct video_tex_info *tex_info,
      const struct video_tex_info *feedback_info)
{
   int i;
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   video_shader_ctx_params_t params;
   video_shader_ctx_info_t shader_info;
   gl_t *gl                               = (gl_t*)data;
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
   for (i = 1; i < gl->fbo_pass; i++)
   {
      video_shader_ctx_mvp_t mvp;
      video_shader_ctx_coords_t coords;
      video_shader_ctx_params_t params;
      const struct video_fbo_rect *rect = &gl->fbo_rect[i];

      prev_rect = &gl->fbo_rect[i - 1];
      fbo_info  = &fbo_tex_info[i - 1];

      xamt      = (GLfloat)prev_rect->img_width / prev_rect->width;
      yamt      = (GLfloat)prev_rect->img_height / prev_rect->height;

      set_texture_coords(fbo_tex_coords, xamt, yamt);

      fbo_info->tex           = gl->fbo_texture[i - 1];
      fbo_info->input_size[0] = prev_rect->img_width;
      fbo_info->input_size[1] = prev_rect->img_height;
      fbo_info->tex_size[0]   = prev_rect->width;
      fbo_info->tex_size[1]   = prev_rect->height;
      memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));
      fbo_tex_info_cnt++;

      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo[i]);

      shader_info.data       = gl;
      shader_info.idx        = i + 1;
      shader_info.set_active = true;

      video_shader_driver_use(shader_info);
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

      mip_level = i + 1;

      if (video_shader_driver_mipmap_input(&mip_level)
            && gl_check_capability(GL_CAPS_MIPMAP))
         glGenerateMipmap(GL_TEXTURE_2D);

      glClear(GL_COLOR_BUFFER_BIT);

      /* Render to FBO with certain size. */
      gl_set_viewport(gl, video_info,
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

      video_shader_driver_set_parameters(params);

      gl->coords.vertices = 4;

      coords.handle_data  = NULL;
      coords.data         = &gl->coords;

      video_shader_driver_set_coords(coords);

      mvp.data = gl;
      mvp.matrix = &gl->mvp;

      video_shader_driver_set_mvp(mvp);

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

   shader_info.data       = gl;
   shader_info.idx        = gl->fbo_pass + 1;
   shader_info.set_active = true;

   video_shader_driver_use(shader_info);

   glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

   mip_level = gl->fbo_pass + 1;

   if (video_shader_driver_mipmap_input(&mip_level)
         && gl_check_capability(GL_CAPS_MIPMAP))
      glGenerateMipmap(GL_TEXTURE_2D);

   glClear(GL_COLOR_BUFFER_BIT);
   gl_set_viewport(gl, video_info,
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

   video_shader_driver_set_parameters(params);

   gl->coords.vertex    = gl->vertex_ptr;

   gl->coords.vertices  = 4;

   coords.handle_data   = NULL;
   coords.data          = &gl->coords;

   video_shader_driver_set_coords(coords);

   mvp.data             = gl;
   mvp.matrix           = &gl->mvp;

   video_shader_driver_set_mvp(mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_info.coord;
}

void gl2_renderchain_deinit_fbo(void *data)
{
   gl_t *gl = (gl_t*)data;
   if (!gl->fbo_inited)
      return;

   glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
   glDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   memset(gl->fbo_texture, 0, sizeof(gl->fbo_texture));
   memset(gl->fbo, 0, sizeof(gl->fbo));
   gl->fbo_inited = false;
   gl->fbo_pass   = 0;

   if (gl->fbo_feedback)
      glDeleteFramebuffers(1, &gl->fbo_feedback);
   if (gl->fbo_feedback_texture)
      glDeleteTextures(1, &gl->fbo_feedback_texture);

   gl->fbo_feedback_enable  = false;
   gl->fbo_feedback_pass    = 0;
   gl->fbo_feedback_texture = 0;
   gl->fbo_feedback         = 0;
}

void gl2_renderchain_deinit_hw_render(void *data)
{
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;

   context_bind_hw_render(true);

   if (gl->hw_render_fbo_init)
      glDeleteFramebuffers(gl->textures, gl->hw_render_fbo);
   if (gl->hw_render_depth_init)
      glDeleteRenderbuffers(gl->textures, gl->hw_render_depth);
   gl->hw_render_fbo_init = false;

   context_bind_hw_render(false);
}

void gl2_renderchain_free(void *data)
{
   gl_t *gl = (gl_t*)data;

   gl2_renderchain_deinit_fbo(gl);
   gl2_renderchain_deinit_hw_render(gl);
}

static bool gl_create_fbo_targets(gl_t *gl)
{
   int i;

   glBindTexture(GL_TEXTURE_2D, 0);
   glGenFramebuffers(gl->fbo_pass, gl->fbo);

   for (i = 0; i < gl->fbo_pass; i++)
   {
      GLenum status;

      glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo[i]);
      glFramebufferTexture2D(RARCH_GL_FRAMEBUFFER,
            RARCH_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

      status = glCheckFramebufferStatus(RARCH_GL_FRAMEBUFFER);
      if (status != RARCH_GL_FRAMEBUFFER_COMPLETE)
         goto error;
   }

   if (gl->fbo_feedback_texture)
   {
      GLenum status;

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
   RARCH_ERR("[GL]: Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
   return false;
}

static void gl_create_fbo_texture(gl_t *gl, unsigned i, GLuint texture)
{
   unsigned mip_level;
   GLenum min_filter, mag_filter, wrap_enum;
   video_shader_ctx_filter_t filter_type;
   video_shader_ctx_wrap_t wrap = {0};
   bool fp_fbo                  = false;
   bool mipmapped               = false;
   bool smooth                  = false;
   settings_t *settings         = config_get_ptr();
   GLuint base_filt             = settings->bools.video_smooth ? GL_LINEAR : GL_NEAREST;
   GLuint base_mip_filt         = settings->bools.video_smooth ? 
      GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;

   glBindTexture(GL_TEXTURE_2D, texture);

   mip_level                    = i + 2;
   mipmapped                    = video_shader_driver_mipmap_input(&mip_level);
   min_filter                   = mipmapped ? base_mip_filt : base_filt;
   filter_type.index            = i + 2;
   filter_type.smooth           = &smooth;

   if (video_shader_driver_filter_type(&filter_type))
   {
      min_filter = mipmapped ? (smooth ? 
            GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST)
         : (smooth ? GL_LINEAR : GL_NEAREST);
   }

   mag_filter = min_filter_to_mag(min_filter);
   wrap.idx   = i + 2;

   video_shader_driver_wrap_type(&wrap);

   wrap_enum  = gl_wrap_type_to_enum(wrap.type);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_enum);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_enum);

   fp_fbo   = gl->fbo_scale[i].fp_fbo;

   if (fp_fbo)
   {
      if (!gl->has_fp_fbo)
         RARCH_ERR("[GL]: Floating-point FBO was requested, but is not supported. Falling back to UNORM. Result may band/clip/etc.!\n");
   }

#if !defined(HAVE_OPENGLES2)
   if (fp_fbo && gl->has_fp_fbo)
   {
      RARCH_LOG("[GL]: FBO pass #%d is floating-point.\n", i);
      gl_load_texture_image(GL_TEXTURE_2D, 0, GL_RGBA32F,
         gl->fbo_rect[i].width, gl->fbo_rect[i].height,
         0, GL_RGBA, GL_FLOAT, NULL);
   }
   else
#endif
   {
#ifndef HAVE_OPENGLES
      bool srgb_fbo = gl->fbo_scale[i].srgb_fbo;
       
      if (!fp_fbo && srgb_fbo)
      {
         if (!gl->has_srgb_fbo)
               RARCH_ERR("[GL]: sRGB FBO was requested, but it is not supported. Falling back to UNORM. Result may have banding!\n");
      }
       
      if (settings->bools.video_force_srgb_disable)
         srgb_fbo = false;
       
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
         gl_load_texture_image(GL_TEXTURE_2D,
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
         gl_load_texture_image(GL_TEXTURE_2D,
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
      gl_create_fbo_texture(gl, i, gl->fbo_texture[i]);

   if (gl->fbo_feedback_enable)
   {
      glGenTextures(1, &gl->fbo_feedback_texture);
      gl_create_fbo_texture(gl,
            gl->fbo_feedback_pass, gl->fbo_feedback_texture);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

/* Compute FBO geometry.
 * When width/height changes or window sizes change, 
 * we have to recalculate geometry of our FBO. */

void gl2_renderchain_recompute_pass_sizes(
      void *data,
      unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
{
   int i;
   gl_t *gl                 = (gl_t*)data;
   bool size_modified       = false;
   GLint max_size           = 0;
   unsigned last_width      = width;
   unsigned last_height     = height;
   unsigned last_max_width  = gl->tex_w;
   unsigned last_max_height = gl->tex_h;

   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);

   /* Calculate viewports for FBOs. */
   for (i = 0; i < gl->fbo_pass; i++)
   {
      struct video_fbo_rect  *fbo_rect   = &gl->fbo_rect[i];
      struct gfx_fbo_scale *fbo_scale    = &gl->fbo_scale[i];

      gl2_renderchain_convert_geometry(
            gl, fbo_rect, fbo_scale,
            last_width, last_max_width,
            last_height, last_max_height,
            vp_width, vp_height
            );

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

void gl2_renderchain_start_render(void *data, video_frame_info_t *video_info)
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
   gl_t *gl = (gl_t*)data;

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   glBindFramebuffer(RARCH_GL_FRAMEBUFFER, gl->fbo[0]);

   gl_set_viewport(gl,
         video_info, gl->fbo_rect[0].img_width,
         gl->fbo_rect[0].img_height, true, false);

   /* Need to preserve the "flipped" state when in FBO 
    * as well to have consistent texture coordinates.
    *
    * We will "flip" it in place on last pass. */
   gl->coords.vertex = fbo_vertexes;

#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (gl->has_srgb_fbo)
      glEnable(GL_FRAMEBUFFER_SRGB);
#endif
}

/* Set up render to texture. */
void gl2_renderchain_init(
      void *data, unsigned fbo_width, unsigned fbo_height)
{
   int i;
   unsigned width, height;
   video_shader_ctx_scale_t scaler;
   video_shader_ctx_info_t shader_info;
   struct gfx_fbo_scale scale, scale_last;
   gl_t *gl = (gl_t*)data;

   if (!video_shader_driver_info(&shader_info))
      return;

   if (!gl || shader_info.num == 0)
      return;

   video_driver_get_size(&width, &height);

   scaler.idx   = 1;
   scaler.scale = &scale;

   video_shader_driver_scale(&scaler);

   scaler.idx   = shader_info.num;
   scaler.scale = &scale_last;

   video_shader_driver_scale(&scaler);

   /* we always want FBO to be at least initialized on startup for consoles */
   if (shader_info.num == 1 && !scale.valid)
      return;

   if (!gl_check_capability(GL_CAPS_FBO))
   {
      RARCH_ERR("[GL]: Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
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

      video_shader_driver_scale(&scaler);

      if (!gl->fbo_scale[i].valid)
      {
         gl->fbo_scale[i].scale_x = gl->fbo_scale[i].scale_y = 1.0f;
         gl->fbo_scale[i].type_x  = gl->fbo_scale[i].type_y  = 
            RARCH_SCALE_INPUT;
         gl->fbo_scale[i].valid   = true;
      }
   }

   gl2_renderchain_recompute_pass_sizes(gl,
         fbo_width, fbo_height, width, height);

   for (i = 0; i < gl->fbo_pass; i++)
   {
      gl->fbo_rect[i].width  = next_pow2(gl->fbo_rect[i].img_width);
      gl->fbo_rect[i].height = next_pow2(gl->fbo_rect[i].img_height);
      RARCH_LOG("[GL]: Creating FBO %d @ %ux%u\n", i,
            gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   gl->fbo_feedback_enable = video_shader_driver_get_feedback_pass(
         &gl->fbo_feedback_pass);

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
   if (!gl || !gl_create_fbo_targets(gl))
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      RARCH_ERR("[GL]: Failed to create FBO targets. Will continue without FBO.\n");
      return;
   }

   gl->fbo_inited = true;
}

bool gl2_renderchain_init_hw_render(void *data,
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
   gl_t *gl                             = (gl_t*)data;

   /* We can only share texture objects through contexts.
    * FBOs are "abstract" objects and are not shared. */
   context_bind_hw_render(true);

   RARCH_LOG("[GL]: Initializing HW render (%u x %u).\n", width, height);
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_fbo_size);
   glGetIntegerv(RARCH_GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size);
   RARCH_LOG("[GL]: Max texture size: %d px, renderbuffer size: %d px.\n",
         max_fbo_size, max_renderbuffer_size);

   if (!gl_check_capability(GL_CAPS_FBO))
      return false;

   RARCH_LOG("[GL]: Supports FBO (render-to-texture).\n");

   glBindTexture(GL_TEXTURE_2D, 0);
   glGenFramebuffers(gl->textures, gl->hw_render_fbo);

   depth   = hwr->depth;
   stencil = hwr->stencil;

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
#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES1) || ((defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__))))
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

   context_bind_hw_render(false);
   return true;
}

#endif

void gl_renderchain_bind_prev_texture(
      void *data,
      const struct video_tex_info *tex_info)
{
   gl_t *gl = (gl_t*)data;

   memmove(gl->prev_info + 1, gl->prev_info,
         sizeof(*tex_info) * (gl->textures - 1));
   memcpy(&gl->prev_info[0], tex_info,
         sizeof(*tex_info));

#ifdef HAVE_FBO
   /* Implement feedback by swapping out FBO/textures 
    * for FBO pass #N and feedbacks. */
   if (gl->fbo_feedback_enable)
   {
      GLuint tmp_fbo                 = gl->fbo_feedback;
      GLuint tmp_tex                 = gl->fbo_feedback_texture;
      gl->fbo_feedback               = gl->fbo[gl->fbo_feedback_pass];
      gl->fbo_feedback_texture       = gl->fbo_texture[gl->fbo_feedback_pass];
      gl->fbo[gl->fbo_feedback_pass] = tmp_fbo;
      gl->fbo_texture[gl->fbo_feedback_pass] = tmp_tex;
   }
#endif
}

bool gl2_renderchain_add_lut(
      const struct video_shader *shader,
      unsigned i, void *textures_data)
{
   struct texture_image img;
   GLuint *textures_lut                 = (GLuint*)textures_data;
   enum texture_filter_type filter_type = TEXTURE_FILTER_LINEAR;

   img.width         = 0;
   img.height        = 0;
   img.pixels        = NULL;
   img.supports_rgba = video_driver_supports_rgba();

   if (!image_texture_load(&img, shader->lut[i].path))
   {
      RARCH_ERR("[GL]: Failed to load texture image from: \"%s\"\n",
            shader->lut[i].path);
      return false;
   }

   RARCH_LOG("[GL]: Loaded texture image from: \"%s\" ...\n",
         shader->lut[i].path);

   if (shader->lut[i].filter == RARCH_FILTER_NEAREST)
      filter_type = TEXTURE_FILTER_NEAREST;

   if (shader->lut[i].mipmap)
   {
      if (filter_type == TEXTURE_FILTER_NEAREST)
         filter_type = TEXTURE_FILTER_MIPMAP_NEAREST;
      else
         filter_type = TEXTURE_FILTER_MIPMAP_LINEAR;
   }

   gl_load_texture_data(textures_lut[i],
         shader->lut[i].wrap,
         filter_type, 4,
         img.width, img.height,
         img.pixels, sizeof(uint32_t));
   image_texture_free(&img);

   return true;
}

void gl_renderchain_viewport_info(void *data, struct video_viewport *vp)
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

bool gl_renderchain_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
#ifndef NO_GL_READ_PIXELS
   unsigned                     num_pixels = 0;
   gl_t                                *gl = (gl_t*)data;

   if (!gl)
      return false;

   context_bind_hw_render(false);

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
   else /* Use slow synchronous readbacks. Use this with plain screenshots
           as we don't really care about performance in this case. */
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

   context_bind_hw_render(true);
   return true;

error:
   context_bind_hw_render(true);
#endif

   return false;
}

gl_renderchain_driver_t gl2_renderchain = {
   gl2_renderchain_init,
   gl2_renderchain_init_hw_render,
   gl2_renderchain_free,
   gl2_renderchain_deinit_hw_render,
   gl2_renderchain_start_render,
   gl2_renderchain_check_fbo_dimensions,
   gl2_renderchain_recompute_pass_sizes,
   gl2_renderchain_render,
   "gl2",
};
