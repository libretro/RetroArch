/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

static const shader_backend_t *video_shader_set_backend(enum rarch_shader_type type)
{
   switch (type)
   {
      case RARCH_SHADER_CG:
         {

#ifdef HAVE_CG
            gfx_ctx_flags_t flags;
            gfx_ctx_ctl(GFX_CTL_GET_FLAGS, &flags);

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

bool video_shader_driver_ctl(enum video_shader_driver_ctl_state state, void *data)
{
   static const shader_backend_t *current_shader = NULL;
   static void *shader_data                      = NULL;

   switch (state)
   {
      case SHADER_CTL_GET_PREV_TEXTURES:
         {
            video_shader_ctx_texture_t *texture = (video_shader_ctx_texture_t*)data;
            if (!!texture || !current_shader)
            {
               texture->id = 0;
               return false;
            }
            texture->id = current_shader->get_prev_textures(shader_data);
         }
         break;
      case SHADER_CTL_GET_IDENT:
         {
            video_shader_ctx_ident_t *ident = (video_shader_ctx_ident_t*)data;
            if (!current_shader || !ident)
               return false;
            ident->ident = current_shader->ident;
         }
         break;
      case SHADER_CTL_GET_CURRENT_SHADER:
         {
            video_shader_ctx_t *shader = (video_shader_ctx_t*)data;
            void *video_driver                       = video_driver_get_ptr(false);
            const video_poke_interface_t *video_poke = video_driver_get_poke();

            shader->data = NULL;
            if (!video_poke || !video_driver)
               return false;
            if (!video_poke->get_current_shader)
               return false;
            shader->data = video_poke->get_current_shader(video_driver);
         }
         break;
      case SHADER_CTL_DIRECT_GET_CURRENT_SHADER:
         {
            video_shader_ctx_t *shader = (video_shader_ctx_t*)data;

            shader->data = NULL;
            if (!current_shader || !current_shader->get_current_shader)
               return false;

            shader->data = current_shader->get_current_shader(shader_data);
         }
         break;
      case SHADER_CTL_DEINIT:
         if (!current_shader)
            return false;

         if (current_shader->deinit)
            current_shader->deinit(shader_data);

         shader_data    = NULL;
         current_shader = NULL;
         break;
      case SHADER_CTL_SET_PARAMETER:
         {
            struct uniform_info *param = (struct uniform_info*)data;

            if (!current_shader || !param)
               return false;
            current_shader->set_uniform_parameter(shader_data,
                  param, NULL);
         }
         break;
      case SHADER_CTL_SET_PARAMS:
         {
            video_shader_ctx_params_t *params = 
               (video_shader_ctx_params_t*)data;

            if (!current_shader || !current_shader->set_params)
               return false;
            current_shader->set_params(
                  params->data,
                  shader_data,
                  params->width,
                  params->height,
                  params->tex_width,
                  params->tex_height,
                  params->out_width,
                  params->out_height,
                  params->frame_counter,
                  params->info,
                  params->prev_info,
                  params->feedback_info,
                  params->fbo_info,
                  params->fbo_info_cnt);
         }
         break;
         /* Finds first suitable shader context driver. */
      case SHADER_CTL_INIT_FIRST:
         {
            unsigned i;

            for (i = 0; shader_ctx_drivers[i]; i++)
            {
               current_shader = shader_ctx_drivers[i];
               return true;
            }
         }
         return false;
      case SHADER_CTL_INIT:
         {
            video_shader_ctx_init_t *init = (video_shader_ctx_init_t*)data;
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
         }
         break;
      case SHADER_CTL_GET_FEEDBACK_PASS:
         if (!current_shader || !current_shader->get_feedback_pass)
            return false;
         if (!current_shader->get_feedback_pass(shader_data,
               (unsigned*)data))
            return false;
         break;
      case SHADER_CTL_MIPMAP_INPUT:
         if (!current_shader)
            return false;
         {
            unsigned *index = (unsigned*)data;
            if (!current_shader->mipmap_input(shader_data, *index))
               return false;
         }
         break;
      case SHADER_CTL_SET_COORDS:
         {
            video_shader_ctx_coords_t *coords = (video_shader_ctx_coords_t*)
               data;
            if (!current_shader || !current_shader->set_coords)
               return false;
            if (!current_shader->set_coords(coords->handle_data,
                  shader_data, (const struct gfx_coords*)coords->data))
               return false;
         }
         break;
      case SHADER_CTL_SCALE:
         {
            video_shader_ctx_scale_t *scaler = (video_shader_ctx_scale_t*)data;
            if (!scaler || !scaler->scale)
               return false;

            scaler->scale->valid = false;

            if (!current_shader || !current_shader->shader_scale)
               return false;

            current_shader->shader_scale(shader_data, scaler->idx, scaler->scale);
         }
         break;
      case SHADER_CTL_INFO:
         {
            video_shader_ctx_info_t *shader_info = 
               (video_shader_ctx_info_t*)data;
            if (!shader_info || !current_shader)
               return false;

            shader_info->num = 0;
            if (current_shader->num_shaders)
               shader_info->num = current_shader->num_shaders(shader_data);
         }
         break;
      case SHADER_CTL_SET_MVP:
         {
            video_shader_ctx_mvp_t *mvp = (video_shader_ctx_mvp_t*)data;
            if (!current_shader || !current_shader->set_mvp)
               return false;
            if (!mvp || !mvp->matrix)
               return false;
            if (!current_shader->set_mvp(mvp->data, shader_data, mvp->matrix))
               return false;
         }
         break;
      case SHADER_CTL_FILTER_TYPE:
         {
            video_shader_ctx_filter_t *filter = 
               (video_shader_ctx_filter_t*)data;
            if (!current_shader || !current_shader->filter_type || !filter)
               return false;
            if (!current_shader->filter_type(shader_data,
                  filter->index, filter->smooth))
               return false;
         }
         break;
      case SHADER_CTL_COMPILE_PROGRAM:
         {
            struct shader_program_info *program_info = (struct shader_program_info*)data;
            if (!current_shader || !program_info)
               return false;
            return current_shader->compile_program(program_info->data,
                  program_info->idx, NULL, program_info);
         }
      case SHADER_CTL_USE:
         {
            video_shader_ctx_info_t *shader_info = (video_shader_ctx_info_t*)data;
            if (!current_shader || !shader_info)
               return false;
            current_shader->use(shader_info->data, shader_data, shader_info->idx, shader_info->set_active);
         }
         break;
      case SHADER_CTL_WRAP_TYPE:
         {
            video_shader_ctx_wrap_t *wrap = (video_shader_ctx_wrap_t*)data;
            if (!current_shader || !current_shader->wrap_type)
               return false;
            wrap->type = current_shader->wrap_type(shader_data, wrap->idx);
         }
         break;
      case SHADER_CTL_NONE:
      default:
         break;
   }

   return true;
}
