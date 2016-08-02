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
