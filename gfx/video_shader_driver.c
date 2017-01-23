/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

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

const shader_backend_t *current_shader = NULL;
void *shader_data                      = NULL;

static const shader_backend_t *video_shader_set_backend(enum rarch_shader_type type)
{
   switch (type)
   {
      case RARCH_SHADER_CG:
         {

#ifdef HAVE_CG
            gfx_ctx_flags_t flags;
            video_context_driver_get_flags(&flags);

            if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
            {
               RARCH_ERR("[Shader driver]: Cg cannot be used with core GL context. Trying to fall back to GLSL...\n");
               return video_shader_set_backend(RARCH_SHADER_GLSL);
            }

            RARCH_LOG("[Shader driver]: Using Cg shader backend.\n");
            return &gl_cg_backend;
#else
            break;
#endif
         }
      case RARCH_SHADER_GLSL:
#ifdef HAVE_GLSL
         RARCH_LOG("[Shader driver]: Using GLSL shader backend.\n");
         return &gl_glsl_backend;
#else
         break;
#endif
      case RARCH_SHADER_NONE:
      default:
         break;
   }

   return NULL;
}

bool video_shader_driver_get_prev_textures(video_shader_ctx_texture_t *texture)
{
   if (!texture || !current_shader)
   {
      if (texture)
         texture->id = 0;
      return false;
   }
   texture->id = current_shader->get_prev_textures(shader_data);

   return true;
}

bool video_shader_driver_get_ident(video_shader_ctx_ident_t *ident)
{
   if (!current_shader || !ident)
      return false;
   ident->ident = current_shader->ident;
   return true;
}

bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader)
{
   void *video_driver                       = video_driver_get_ptr(false);
   const video_poke_interface_t *video_poke = video_driver_get_poke();

   shader->data = NULL;
   if (!video_poke || !video_driver)
      return false;
   if (!video_poke->get_current_shader)
      return false;
   shader->data = video_poke->get_current_shader(video_driver);
   return true;
}

bool video_shader_driver_direct_get_current_shader(video_shader_ctx_t *shader)
{
   shader->data = NULL;
   if (!current_shader || !current_shader->get_current_shader)
      return false;

   shader->data = current_shader->get_current_shader(shader_data);
   return true;
}

bool video_shader_driver_deinit(void)
{
   if (!current_shader)
      return false;

   if (current_shader->deinit)
      current_shader->deinit(shader_data);

   shader_data    = NULL;
   current_shader = NULL;
   return true;
}

/* Finds first suitable shader context driver. */
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

bool video_shader_driver_init(video_shader_ctx_init_t *init)
{
   void *tmp = NULL;

   if (!init->shader || !init->shader->init)
   {
      init->shader = video_shader_set_backend(init->shader_type);

      if (!init->shader)
         return false;
   }

   tmp = init->shader->init(init->data, init->path);

   if (!tmp)
      return false;

   shader_data    = tmp;
   current_shader = init->shader;

   return true;
}

bool video_shader_driver_get_feedback_pass(unsigned *data)
{
   if (     current_shader 
         && current_shader->get_feedback_pass
         && current_shader->get_feedback_pass(shader_data, data))
      return true;
   return false;
}

bool video_shader_driver_mipmap_input(unsigned *index)
{
   if (     current_shader 
         && current_shader->mipmap_input(shader_data, *index))
      return true;
   return false;
}

bool video_shader_driver_scale(video_shader_ctx_scale_t *scaler)
{
   if (!scaler || !scaler->scale)
      return false;

   scaler->scale->valid = false;

   if (!current_shader || !current_shader->shader_scale)
      return false;

   current_shader->shader_scale(shader_data, scaler->idx, scaler->scale);
   return true;
}

bool video_shader_driver_info(video_shader_ctx_info_t *shader_info)
{
   if (!shader_info || !current_shader)
      return false;

   shader_info->num = 0;
   if (current_shader->num_shaders)
      shader_info->num = current_shader->num_shaders(shader_data);
   return true;
}

bool video_shader_driver_filter_type(video_shader_ctx_filter_t *filter)
{
   if (     filter
         && current_shader 
         && current_shader->filter_type 
         && current_shader->filter_type(shader_data,
            filter->index, filter->smooth))
      return true;
   return false;
}

bool video_shader_driver_compile_program(struct shader_program_info *program_info)
{
   if (!current_shader || !program_info)
      return false;
   return current_shader->compile_program(program_info->data,
         program_info->idx, NULL, program_info);
}

bool video_shader_driver_wrap_type(video_shader_ctx_wrap_t *wrap)
{
   if (!current_shader || !current_shader->wrap_type)
      return false;
   wrap->type = current_shader->wrap_type(shader_data, wrap->idx);
   return true;
}
