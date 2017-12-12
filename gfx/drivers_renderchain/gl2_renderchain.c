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

#include <retro_common_api.h>
#include <libretro.h>

#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <string/stdstring.h>

#include <gfx/gl_capabilities.h>
#include <gfx/video_frame.h>
#include <glsym/glsym.h>

#include "../video_driver.h"
#include "../video_shader_parse.h"
#include "../common/gl_common.h"

#include "../../driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

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

typedef struct gl2_renderchain
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
} gl2_renderchain_t;

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
static void gl2_renderchain_bind_backbuffer(void *data,
      void *chain_data)
{
#ifdef IOS
   /* There is no default frame buffer on iOS. */
   void cocoagl_bind_game_view_fbo(void);
   cocoagl_bind_game_view_fbo();
#else
   gl2_bind_fb(0);
#endif
}

void context_bind_hw_render(bool enable);

GLenum min_filter_to_mag(GLenum type);

void gl_load_texture_data(
      uint32_t id_data,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size);

void gl_set_viewport(
      void *data, video_frame_info_t *video_info,
      unsigned viewport_width,
      unsigned viewport_height,
      bool force_full, bool allow_rotate);

static void gl2_renderchain_convert_geometry(
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
   gl2_bind_fb(fbo);
   glDeleteTextures(1, texture);
   glGenTextures(1, texture);
   glBindTexture(GL_TEXTURE_2D, *texture);
   gl_load_texture_image(GL_TEXTURE_2D,
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

static void gl_check_fbo_dimension(gl_t *gl,
      void *chain_data,
      unsigned i,
      bool update_feedback)
{
   struct video_fbo_rect *fbo_rect = &gl->fbo_rect[i];
   /* Check proactively since we might suddently
    * get sizes of tex_w width or tex_h height. */
   gl2_renderchain_t *chain        = (gl2_renderchain_t*)chain_data;
   unsigned img_width              = fbo_rect->max_img_width;
   unsigned img_height             = fbo_rect->max_img_height;
   unsigned max                    = img_width > img_height ? img_width : img_height;
   unsigned pow2_size              = next_pow2(max);

   fbo_rect->width                 = pow2_size;
   fbo_rect->height                = pow2_size;

   gl_recreate_fbo(fbo_rect, chain->fbo[i], &chain->fbo_texture[i]);

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

static void gl2_renderchain_check_fbo_dimensions(void *data,
      void *chain_data)
{
   int i;
   gl_t *gl                 = (gl_t*)data;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

   /* Check if we have to recreate our FBO textures. */
   for (i = 0; i < chain->fbo_pass; i++)
   {
      struct video_fbo_rect *fbo_rect = &gl->fbo_rect[i];
      if (fbo_rect)
      {
         bool update_feedback = gl->fbo_feedback_enable
            && (unsigned)i == gl->fbo_feedback_pass;

         if ((fbo_rect->max_img_width  > fbo_rect->width) ||
             (fbo_rect->max_img_height > fbo_rect->height))
               gl_check_fbo_dimension(gl, chain_data, i, update_feedback);
      }
   }
}

static void gl2_renderchain_render(
      void *data,
      void *chain_data,
      video_frame_info_t *video_info,
      uint64_t frame_count,
      const struct video_tex_info *tex_info,
      const struct video_tex_info *feedback_info)
{
   int i;
   video_shader_ctx_coords_t coords;
   video_shader_ctx_params_t params;
   video_shader_ctx_info_t shader_info;
   gl_t *gl                               = (gl_t*)data;
   gl2_renderchain_t *chain               = (gl2_renderchain_t*)chain_data;
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
      video_shader_ctx_coords_t coords;
      video_shader_ctx_params_t params;
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

      shader_info.data       = gl;
      shader_info.idx        = i + 1;
      shader_info.set_active = true;

      video_shader_driver_use(shader_info);
      glBindTexture(GL_TEXTURE_2D, chain->fbo_texture[i - 1]);

      mip_level = i + 1;

      if (video_shader_driver_mipmap_input(&mip_level)
            && gl->have_mipmap)
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

      video_driver_set_coords(&coords);

      video_info->cb_set_mvp(gl,
            video_info->shader_data, &gl->mvp);

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
   gl2_renderchain_bind_backbuffer(gl, chain_data);

   shader_info.data       = gl;
   shader_info.idx        = chain->fbo_pass + 1;
   shader_info.set_active = true;

   video_shader_driver_use(shader_info);

   glBindTexture(GL_TEXTURE_2D, chain->fbo_texture[chain->fbo_pass - 1]);

   mip_level = chain->fbo_pass + 1;

   if (video_shader_driver_mipmap_input(&mip_level)
         && gl->have_mipmap)
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

   video_driver_set_coords(&coords);

   video_info->cb_set_mvp(gl,
         video_info->shader_data, &gl->mvp);

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_info.coord;
}

static void gl2_renderchain_deinit_fbo(void *data,
      void *chain_data)
{
   gl_t *gl                 = (gl_t*)data;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

   if (!gl)
      return;

   glDeleteTextures(chain->fbo_pass, chain->fbo_texture);
   gl2_delete_fb(chain->fbo_pass, chain->fbo);

   memset(chain->fbo_texture, 0, sizeof(chain->fbo_texture));
   memset(chain->fbo,         0, sizeof(chain->fbo));

   if (gl->fbo_feedback)
      gl2_delete_fb(1, &gl->fbo_feedback);
   if (gl->fbo_feedback_texture)
      glDeleteTextures(1, &gl->fbo_feedback_texture);

   chain->fbo_pass          = 0;

   gl->fbo_inited           = false;
   gl->fbo_feedback_enable  = false;
   gl->fbo_feedback_pass    = 0;
   gl->fbo_feedback_texture = 0;
   gl->fbo_feedback         = 0;
}

static void gl2_renderchain_deinit_hw_render(
      void *data,
      void *chain_data)
{
   gl_t                 *gl = (gl_t*)data;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;
   if (!gl)
      return;

   context_bind_hw_render(true);

   if (gl->hw_render_fbo_init)
      gl2_delete_fb(gl->textures, gl->hw_render_fbo);
   if (chain->hw_render_depth_init)
      gl2_delete_rb(gl->textures, chain->hw_render_depth);
   gl->hw_render_fbo_init = false;

   context_bind_hw_render(false);
}

static void gl2_renderchain_free(void *data, void *chain_data)
{
   gl_t *gl = (gl_t*)data;

   gl2_renderchain_deinit_fbo(gl, chain_data);
   gl2_renderchain_deinit_hw_render(gl, chain_data);
}

static bool gl_create_fbo_targets(gl_t *gl, void *chain_data)
{
   int i;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

   glBindTexture(GL_TEXTURE_2D, 0);
   gl2_gen_fb(chain->fbo_pass, chain->fbo);

   for (i = 0; i < chain->fbo_pass; i++)
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

static void gl_create_fbo_texture(gl_t *gl,
      void *chain_data, unsigned i, GLuint texture)
{
   GLenum mag_filter, wrap_enum;
   video_shader_ctx_filter_t filter_type;
   video_shader_ctx_wrap_t wrap = {0};
   bool fp_fbo                  = false;
   bool smooth                  = false;
   gl2_renderchain_t *chain     = (gl2_renderchain_t*)chain_data;
   settings_t *settings         = config_get_ptr();
   GLuint base_filt             = settings->bools.video_smooth ? GL_LINEAR : GL_NEAREST;
   GLuint base_mip_filt         = settings->bools.video_smooth ?
      GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
   unsigned mip_level           = i + 2;
   bool mipmapped               = video_shader_driver_mipmap_input(&mip_level);
   GLenum min_filter            = mipmapped ? base_mip_filt : base_filt;

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
      gl_load_texture_image(GL_TEXTURE_2D, 0, GL_RGBA32F,
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

static void gl_create_fbo_textures(gl_t *gl, void *chain_data)
{
   int i;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

   glGenTextures(chain->fbo_pass, chain->fbo_texture);

   for (i = 0; i < chain->fbo_pass; i++)
      gl_create_fbo_texture(gl, gl->renderchain_data,
            i, chain->fbo_texture[i]);

   if (gl->fbo_feedback_enable)
   {
      glGenTextures(1, &gl->fbo_feedback_texture);
      gl_create_fbo_texture(gl,
            gl->renderchain_data,
            gl->fbo_feedback_pass, gl->fbo_feedback_texture);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

/* Compute FBO geometry.
 * When width/height changes or window sizes change,
 * we have to recalculate geometry of our FBO. */

static void gl2_renderchain_recompute_pass_sizes(
      void *data,
      void *chain_data,
      unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
{
   int i;
   gl_t *gl                 = (gl_t*)data;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;
   bool size_modified       = false;
   GLint max_size           = 0;
   unsigned last_width      = width;
   unsigned last_height     = height;
   unsigned last_max_width  = gl->tex_w;
   unsigned last_max_height = gl->tex_h;

   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);

   /* Calculate viewports for FBOs. */
   for (i = 0; i < chain->fbo_pass; i++)
   {
      struct video_fbo_rect  *fbo_rect   = &gl->fbo_rect[i];
      struct gfx_fbo_scale *fbo_scale    = &chain->fbo_scale[i];

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

static void gl2_renderchain_start_render(void *data,
      void *chain_data,
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
   gl_t                 *gl = (gl_t*)data;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   gl2_bind_fb(chain->fbo[0]);

   gl_set_viewport(gl,
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
void gl2_renderchain_init(
      void *data, void *chain_data,
      unsigned fbo_width, unsigned fbo_height)
{
   int i;
   unsigned width, height;
   video_shader_ctx_scale_t scaler;
   video_shader_ctx_info_t shader_info;
   struct gfx_fbo_scale scale, scale_last;
   gl_t                 *gl = (gl_t*)data;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

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

      video_shader_driver_scale(&scaler);

      if (!chain->fbo_scale[i].valid)
      {
         chain->fbo_scale[i].scale_x = chain->fbo_scale[i].scale_y = 1.0f;
         chain->fbo_scale[i].type_x  = chain->fbo_scale[i].type_y  =
            RARCH_SCALE_INPUT;
         chain->fbo_scale[i].valid   = true;
      }
   }

   gl2_renderchain_recompute_pass_sizes(gl,
         chain_data,
         fbo_width, fbo_height, width, height);

   for (i = 0; i < chain->fbo_pass; i++)
   {
      gl->fbo_rect[i].width  = next_pow2(gl->fbo_rect[i].img_width);
      gl->fbo_rect[i].height = next_pow2(gl->fbo_rect[i].img_height);
      RARCH_LOG("[GL]: Creating FBO %d @ %ux%u\n", i,
            gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   gl->fbo_feedback_enable = video_shader_driver_get_feedback_pass(
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

   gl_create_fbo_textures(gl, chain);
   if (!gl || !gl_create_fbo_targets(gl, chain))
   {
      glDeleteTextures(chain->fbo_pass, chain->fbo_texture);
      RARCH_ERR("[GL]: Failed to create FBO targets. Will continue without FBO.\n");
      return;
   }

   gl->fbo_inited = true;
}

static bool gl2_renderchain_init_hw_render(
      void *data,
      void *chain_data,
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
   gl2_renderchain_t             *chain = (gl2_renderchain_t*)chain_data;

   /* We can only share texture objects through contexts.
    * FBOs are "abstract" objects and are not shared. */
   context_bind_hw_render(true);

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
         RARCH_ERR("[GL]: Failed to create HW render FBO #%u, error: 0x%u.\n",
               i, (unsigned)status);
         return false;
      }
   }

   gl2_renderchain_bind_backbuffer(gl, chain_data);
   gl->hw_render_fbo_init = true;

   context_bind_hw_render(false);
   return true;
}

static void gl2_renderchain_bind_prev_texture(
      void *data,
      void *chain_data,
      const struct video_tex_info *tex_info)
{
   gl_t                 *gl = (gl_t*)data;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

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

static void gl2_renderchain_viewport_info(
      void *data, void *chain_data,
      struct video_viewport *vp)
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

static bool gl2_renderchain_read_viewport(
      void *data,
      void *chain_data,
      uint8_t *buffer, bool is_idle)
{
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

   context_bind_hw_render(true);
   return true;

error:
   context_bind_hw_render(true);

   return false;
}

void gl2_renderchain_free_internal(void *data, void *chain_data)
{
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

   if (!chain)
      return;

   free(chain);
}

static void *gl2_renderchain_new(void)
{
   gl2_renderchain_t *renderchain = (gl2_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   return renderchain;
}

#ifndef HAVE_OPENGLES
static void gl2_renderchain_bind_vao(void *data,
      void *chain_data)
{
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;
   if (!chain)
      return;
   glBindVertexArray(chain->vao);
}

static void gl2_renderchain_unbind_vao(void *data,
      void *chain_data)
{
   glBindVertexArray(0);
}

static void gl2_renderchain_new_vao(void *data,
      void *chain_data)
{
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;
   if (!chain)
      return;
   glGenVertexArrays(1, &chain->vao);
}

static void gl2_renderchain_free_vao(void *data,
      void *chain_data)
{
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;
   if (!chain)
      return;
   glDeleteVertexArrays(1, &chain->vao);
}
#endif

static void gl2_renderchain_restore_default_state(
      void *data,
      void *chain_data)
{
   gl_t *gl = (gl_t*)data;
   if (!gl)
      return;
#ifndef HAVE_OPENGLES
   if (!gl->core_context_in_use)
      glEnable(GL_TEXTURE_2D);
#endif
   glDisable(GL_DEPTH_TEST);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DITHER);
}

static void gl2_renderchain_copy_frame(
      void *data,
      void *chain_data,
      video_frame_info_t *video_info,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   gl_t                 *gl = (gl_t*)data;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

   (void)chain;

#if defined(HAVE_PSGL)
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
static void gl2_renderchain_bind_pbo(unsigned idx)
{
   glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)idx);
}

static void gl2_renderchain_unbind_pbo(void *data,
      void *chain_data)
{
   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

static void gl2_renderchain_init_pbo(unsigned size,
      const void *data)
{
   glBufferData(GL_PIXEL_PACK_BUFFER, size,
         (const GLvoid*)data, GL_STREAM_READ);
}
#endif

static void gl2_renderchain_readback(void *data,
      void *chain_data,
      unsigned alignment,
      unsigned fmt, unsigned type,
      void *src)
{
   gl_t *gl = (gl_t*)data;

   glPixelStorei(GL_PACK_ALIGNMENT, alignment);
#ifndef HAVE_OPENGLES
   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
   glReadBuffer(GL_BACK);
#endif

   glReadPixels(gl->vp.x, gl->vp.y,
         gl->vp.width, gl->vp.height,
         (GLenum)fmt, (GLenum)type, (GLvoid*)src);
}

#ifndef HAVE_OPENGLES
static void gl2_renderchain_fence_iterate(
      void *data,
      void *chain_data,
      unsigned hard_sync_frames)
{
#ifdef HAVE_GL_SYNC
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

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
}

static void gl2_renderchain_fence_free(void *data,
      void *chain_data)
{
#ifdef HAVE_GL_SYNC
   unsigned i;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

   for (i = 0; i < chain->fence_count; i++)
   {
      glClientWaitSync(chain->fences[i],
            GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
      glDeleteSync(chain->fences[i]);
   }
   chain->fence_count = 0;
#endif
}
#endif

static void gl2_renderchain_init_textures_reference(
      void *data, void *chain_data, unsigned i,
      unsigned internal_fmt, unsigned texture_fmt,
      unsigned texture_type)
{
   gl_t                 *gl = (gl_t*)data;
   gl2_renderchain_t *chain = (gl2_renderchain_t*)chain_data;

   (void)chain;

#ifdef HAVE_PSGL
   glTextureReferenceSCE(GL_TEXTURE_2D, 1,
         gl->tex_w, gl->tex_h, 0,
         (GLenum)internal_fmt,
         gl->tex_w * gl->base_size,
         gl->tex_w * gl->tex_h * i * gl->base_size);
#else
   if (chain->egl_images)
      return;

   gl_load_texture_image(GL_TEXTURE_2D,
      0,
      (GLenum)internal_fmt,
      gl->tex_w, gl->tex_h, 0,
      (GLenum)texture_type,
      (GLenum)texture_fmt,
      gl->empty_buf ? gl->empty_buf : NULL);
#endif
}

static void gl2_renderchain_resolve_extensions(void *data,
      void *chain_data, const char *context_ident,
      const video_info_t *video)
{
   gl_t              *gl            = (gl_t*)data;
   gl2_renderchain_t *chain         = (gl2_renderchain_t*)chain_data;
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
   chain->egl_images                = !gl->hw_render_use && gl_check_capability(GL_CAPS_EGLIMAGE) &&
      video_context_driver_init_image_buffer(video);
}

gl_renderchain_driver_t gl2_renderchain = {
   NULL,                                        /* set_coords */
   NULL,                                        /* set_mvp    */
   gl2_renderchain_init_textures_reference,
#ifdef HAVE_OPENGLES
   NULL,
   NULL,
#else
   gl2_renderchain_fence_iterate,
   gl2_renderchain_fence_free,
#endif
   gl2_renderchain_readback,
#if !defined(HAVE_OPENGLES2) && !defined(HAVE_PSGL)
   gl2_renderchain_init_pbo,
   gl2_renderchain_bind_pbo,
   gl2_renderchain_unbind_pbo,
#else
   NULL,
   NULL,
   NULL,
#endif
   gl2_renderchain_copy_frame,
   gl2_renderchain_restore_default_state,
#ifdef HAVE_OPENGLES
   NULL,
   NULL,
   NULL,
   NULL,
#else
   gl2_renderchain_new_vao,
   gl2_renderchain_free_vao,
   gl2_renderchain_bind_vao,
   gl2_renderchain_unbind_vao,
#endif
   NULL,
   NULL,
   NULL,
   gl2_renderchain_bind_backbuffer,
   gl2_renderchain_deinit_fbo,
   gl2_renderchain_viewport_info,
   gl2_renderchain_read_viewport,
   gl2_renderchain_bind_prev_texture,
   gl2_renderchain_free_internal,
   gl2_renderchain_new,
   gl2_renderchain_init,
   gl2_renderchain_init_hw_render,
   gl2_renderchain_free,
   gl2_renderchain_deinit_hw_render,
   gl2_renderchain_start_render,
   gl2_renderchain_check_fbo_dimensions,
   gl2_renderchain_recompute_pass_sizes,
   gl2_renderchain_render,
   gl2_renderchain_resolve_extensions,
   "gl2",
};
