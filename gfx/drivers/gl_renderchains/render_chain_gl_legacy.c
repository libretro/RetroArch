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
#include <libretro.h>

#include "../../../driver.h"
#include "../../../record/record_driver.h"
#include "../../../performance_counters.h"

#include "../../../general.h"
#include "../../../retroarch.h"
#include "../../../verbosity.h"
#include "../../common/gl_common.h"

#include "render_chain_driver.h"

#ifdef HAVE_THREADS
#include "../../video_thread_wrapper.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../../font_driver.h"
#include "../../video_context_driver.h"
#include "../../video_frame.h"

#ifdef HAVE_GLSL
#include "../../drivers_shader/shader_glsl.h"
#endif

#ifdef GL_DEBUG
#include <lists/string_list.h>
#endif

#ifdef HAVE_MENU
#include "../../../menu/menu_driver.h"
#endif

#if defined(_WIN32) && !defined(_XBOX)
#include "../../common/win32_common.h"
#endif

#include "../../video_shader_driver.h"

#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE     0x9117
#endif

#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT        0x00000001
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

void gl_renderchain_convert_geometry(gl_t *gl,
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

void gl_renderchain_bind_prev_texture(
      gl_t *gl,
      const struct video_tex_info *tex_info)
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
      GLuint tmp_fbo                 = gl->fbo_feedback;
      GLuint tmp_tex                 = gl->fbo_feedback_texture;
      gl->fbo_feedback               = gl->fbo[gl->fbo_feedback_pass];
      gl->fbo_feedback_texture       = gl->fbo_texture[gl->fbo_feedback_pass];
      gl->fbo[gl->fbo_feedback_pass] = tmp_fbo;
      gl->fbo_texture[gl->fbo_feedback_pass] = tmp_tex;
   }
#endif
}

bool gl_renderchain_add_lut(const struct video_shader *shader,
      unsigned i, GLuint *textures_lut)
{
   struct texture_image img = {0};
   enum texture_filter_type filter_type = TEXTURE_FILTER_LINEAR;

   RARCH_LOG("Loading texture image from: \"%s\" ...\n",
         shader->lut[i].path);

   if (!image_texture_load(&img, shader->lut[i].path))
   {
      RARCH_ERR("Failed to load texture image from: \"%s\"\n",
            shader->lut[i].path);
      return false;
   }

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

#ifdef HAVE_FBO
void gl_renderchain_render(gl_t *gl,
      uint64_t frame_count,
      const struct video_tex_info *tex_info,
      const struct video_tex_info *feedback_info)
{
   unsigned mip_level;
   video_shader_ctx_mvp_t mvp;
   video_shader_ctx_coords_t coords;
   video_shader_ctx_params_t params;
   video_shader_ctx_info_t shader_info;
   unsigned width, height;
   const struct video_fbo_rect *prev_rect;
   struct video_tex_info *fbo_info;
   struct video_tex_info fbo_tex_info[GFX_MAX_SHADERS];
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

      video_shader_driver_use(&shader_info);
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

      mip_level = i + 1;

      if (video_shader_driver_mipmap_input(&mip_level)
            && gl_check_capability(GL_CAPS_MIPMAP))
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

      video_shader_driver_set_parameters(&params);

      gl->coords.vertices = 4;

      coords.handle_data  = NULL;
      coords.data         = &gl->coords;

      video_shader_driver_set_coords(&coords);

      mvp.data = gl;
      mvp.matrix = &gl->mvp;

      video_shader_driver_set_mvp(&mvp);

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

   video_shader_driver_use(&shader_info);

   glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

   mip_level = gl->fbo_pass + 1;

   if (video_shader_driver_mipmap_input(&mip_level)
         && gl_check_capability(GL_CAPS_MIPMAP))
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

   video_shader_driver_set_parameters(&params);

   gl->coords.vertex    = gl->vertex_ptr;

   gl->coords.vertices  = 4;

   coords.handle_data   = NULL;
   coords.data          = &gl->coords;

   video_shader_driver_set_coords(&coords);

   mvp.data             = gl;
   mvp.matrix           = &gl->mvp;

   video_shader_driver_set_mvp(&mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_info.coord;
}
#endif
