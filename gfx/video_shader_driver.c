/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <string.h>

#include "video_shader_driver.h"
#include "../verbosity.h"

static const shader_backend_t *shader_ctx_drivers[] = {
#ifdef HAVE_GLSL
   &gl_glsl_backend,
#endif
#ifdef HAVE_CG
   &gl_cg_backend,
#endif
#ifdef HAVE_HLSL
   &hlsl_backend,
#endif
   &shader_null_backend,
   NULL
};

static const shader_backend_t *current_shader;
static void *shader_data;

/**
 * shader_ctx_find_driver:
 * @ident                   : Identifier of shader context driver to find.
 *
 * Finds shader context driver and initializes.
 *
 * Returns: shader context driver if found, otherwise NULL.
 **/
const shader_backend_t *shader_ctx_find_driver(const char *ident)
{
   unsigned i;

   for (i = 0; shader_ctx_drivers[i]; i++)
   {
      if (!strcmp(shader_ctx_drivers[i]->ident, ident))
         return shader_ctx_drivers[i];
   }

   return NULL;
}

/**
 * video_shader_driver_init_first:
 *
 * Finds first suitable shader context driver.
 *
 * Returns: true (1) if found, otherwise false (0).
 **/
bool video_shader_driver_init_first(void)
{
   unsigned i;

   for (i = 0; shader_ctx_drivers[i]; i++)
   {
      current_shader = shader_ctx_drivers[i];
      return true;
   }

   return false;
}

struct video_shader *video_shader_driver_get_current_shader(void)
{
   void *video_driver                       = video_driver_get_ptr(false);
   const video_poke_interface_t *video_poke = video_driver_get_poke();
   if (!video_poke || !video_driver)
      return NULL;
   if (!video_poke->get_current_shader)
      return NULL;
   return video_poke->get_current_shader(video_driver);
}

void video_shader_driver_scale(unsigned idx, struct gfx_fbo_scale *scale)
{
   if (!scale || !current_shader)
      return;

   scale->valid = false;

   if (current_shader->shader_scale)
      current_shader->shader_scale(shader_data, idx, scale);
}

bool video_shader_driver_init(const shader_backend_t *shader, void *data, const char *path)
{
   void *tmp = NULL;

   if (!shader || !shader->init)
      return false;

   tmp = shader->init(data, path);

   if (!tmp)
      return false;

   shader_data    = tmp;
   current_shader = shader;

   return true;
}

void video_shader_driver_deinit(void)
{
   if (!current_shader)
      return;

   if (current_shader->deinit)
      current_shader->deinit(shader_data);

   shader_data    = NULL;
   current_shader = NULL;
}

void video_shader_driver_use(void *data, unsigned index)
{
   if (!current_shader)
      return;
   current_shader->use(data, shader_data, index);
}

const char *video_shader_driver_get_ident(void)
{
   if (!current_shader)
      return NULL;
   return current_shader->ident;
}

bool video_shader_driver_mipmap_input(unsigned index)
{
   if (!current_shader)
      return false;
   return current_shader->mipmap_input(shader_data, index);
}

unsigned video_shader_driver_num_shaders(void)
{
   if (!current_shader)
      return 0;
   return current_shader->num_shaders(shader_data);
}

unsigned video_shader_driver_get_prev_textures(void)
{
   if (!current_shader)
      return 0;
   return current_shader->get_prev_textures(shader_data);
}

bool video_shader_driver_set_coords(void *handle_data, const void *data)
{
   if (!current_shader || !current_shader->set_coords)
      return false;
   return current_shader->set_coords(handle_data, shader_data, data);
}

bool video_shader_driver_set_mvp(void *data, const math_matrix_4x4 *mat)
{
   if (!current_shader || !current_shader->set_mvp || !mat)
      return false;
   return current_shader->set_mvp(data, shader_data, mat);
}

bool video_shader_driver_filter_type(unsigned index, bool *smooth)
{
   if (!current_shader || !current_shader->filter_type)
      return false;
   return current_shader->filter_type(shader_data, index, smooth);
}

enum gfx_wrap_type video_shader_driver_wrap_type(unsigned index)
{
   return current_shader->wrap_type(shader_data, index);
}

bool video_shader_driver_get_feedback_pass(unsigned *pass)
{
   if (!current_shader || !current_shader->get_feedback_pass)
      return false;
   return current_shader->get_feedback_pass(shader_data, pass);
}

struct video_shader *video_shader_driver_direct_get_current_shader(void)
{
   if (!current_shader || !current_shader->get_current_shader)
      return NULL;
   return current_shader->get_current_shader(shader_data);
}

void video_shader_driver_set_params( 
      void *data, unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_counter,
      const void *info, 
      const void *prev_info,
      const void *feedback_info,
      const void *fbo_info, unsigned fbo_info_cnt)
{
   if (!current_shader || !current_shader->set_params)
      return;
   return current_shader->set_params(data, shader_data,
         width, height, tex_width, tex_height,
         out_width, out_height, frame_counter, info, prev_info, feedback_info,
         fbo_info, fbo_info_cnt);
}
