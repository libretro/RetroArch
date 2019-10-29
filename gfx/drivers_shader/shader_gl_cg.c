/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifdef _MSC_VER
#pragma comment(lib, "cg")
#pragma comment(lib, "cggl")
#endif

#include <stdint.h>
#include <string.h>

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_OPENGL
#include "../common/gl_common.h"
#include "../include/Cg/cgGL.h"
#endif

#ifdef HAVE_SHADERPIPELINE
#include "../drivers/gl_shaders/pipeline_xmb_ribbon_simple.cg.h"
#include "../drivers/gl_shaders/pipeline_snow.cg.h"
#endif

#include "../include/Cg/cg.h"

#include "../video_shader_parse.h"
#include "../../core.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../managers/state_manager.h"

#define PREV_TEXTURES         (GFX_MAX_TEXTURES - 1)

#define set_param_2f(param, x, y)    if (param) cgGLSetParameter2f(param, x, y)
#define cg_gl_set_param_1f(param, x) if (param) cgGLSetParameter1f(param, x)

#if 0
#define RARCH_CG_DEBUG
#endif

struct cg_fbo_params
{
   CGparameter vid_size_f;
   CGparameter tex_size_f;
   CGparameter vid_size_v;
   CGparameter tex_size_v;
   CGparameter tex;
   CGparameter coord;
};

struct shader_program_cg
{
   CGprogram vprg;
   CGprogram fprg;

   CGparameter tex;
   CGparameter lut_tex;
   CGparameter color;
   CGparameter vertex;

   CGparameter vid_size_f;
   CGparameter tex_size_f;
   CGparameter out_size_f;
   CGparameter frame_cnt_f;
   CGparameter frame_dir_f;
   CGparameter vid_size_v;
   CGparameter tex_size_v;
   CGparameter out_size_v;
   CGparameter frame_cnt_v;
   CGparameter frame_dir_v;
   CGparameter mvp;

   struct cg_fbo_params fbo[GFX_MAX_SHADERS];
   struct cg_fbo_params orig;
   struct cg_fbo_params feedback;
   struct cg_fbo_params prev[PREV_TEXTURES];
};

typedef struct cg_shader_data
{
   struct video_shader *shader;
   char alias_define[GFX_MAX_SHADERS][128];
   unsigned active_idx;
   unsigned attribs_index;
   CGparameter attribs_elems[32 * PREV_TEXTURES + 2 + 4 + GFX_MAX_SHADERS];
   CGprofile cgVProf;
   CGprofile cgFProf;
   struct shader_program_cg prg[GFX_MAX_SHADERS];
   GLuint lut_textures[GFX_MAX_TEXTURES];
   CGcontext cgCtx;
} cg_shader_data_t;

struct uniform_cg
{
   CGparameter loc;
};

#define gl_cg_set_coord_array(param, cg, ptr, len) \
{ \
   cgGLSetParameterPointer(param, len, GL_FLOAT, 0, ptr); \
   cgGLEnableClientState(param); \
   cg->attribs_elems[cg->attribs_index++] = param; \
}

#define cg_gl_set_texture_parameter(param, texture) \
   if (param) \
   { \
      cgGLSetTextureParameter(param, texture); \
      cgGLEnableTextureParameter(param); \
   }

#include "../drivers/gl_shaders/opaque.cg.h"

static void gl_cg_set_uniform_parameter(
      void *data,
      struct uniform_info *param,
      void *uniform_data)
{
   CGparameter location;
   cg_shader_data_t *cg        = (cg_shader_data_t*)data;

   if (!param || !param->enabled)
      return;

   if (param->lookup.enable)
   {
      char ident[64];
      CGprogram prog = 0;

      ident[0] = '\0';

      switch (param->lookup.type)
      {
         case SHADER_PROGRAM_VERTEX:
            prog = cg->prg[param->lookup.idx].vprg;
            break;
         case SHADER_PROGRAM_FRAGMENT:
         default:
            prog = cg->prg[param->lookup.idx].fprg;
            break;
      }

      if (param->lookup.add_prefix)
         snprintf(ident, sizeof(ident), "IN.%s", param->lookup.ident);
      location = cgGetNamedParameter(prog, param->lookup.add_prefix ? ident : param->lookup.ident);
   }
   else
   {
      struct uniform_cg *cg_param = (struct uniform_cg*)uniform_data;
      location = cg_param->loc;
   }

   switch (param->type)
   {
      case UNIFORM_1F:
         cgGLSetParameter1f(location, param->result.f.v0);
         break;
      case UNIFORM_2F:
         cgGLSetParameter2f(location, param->result.f.v0, param->result.f.v1);
         break;
      case UNIFORM_3F:
         cgGLSetParameter3f(location, param->result.f.v0, param->result.f.v1,
               param->result.f.v2);
         break;
      case UNIFORM_4F:
         cgGLSetParameter4f(location, param->result.f.v0, param->result.f.v1,
               param->result.f.v2, param->result.f.v3);
         break;
      case UNIFORM_1FV:
         cgGLSetParameter1fv(location, param->result.floatv);
         break;
      case UNIFORM_2FV:
         cgGLSetParameter2fv(location, param->result.floatv);
         break;
      case UNIFORM_3FV:
         cgGLSetParameter3fv(location, param->result.floatv);
         break;
      case UNIFORM_4FV:
         cgGLSetParameter3fv(location, param->result.floatv);
         break;
      case UNIFORM_1I:
         /* Unimplemented - Cg limitation */
         break;
   }
}

#ifdef RARCH_CG_DEBUG
static void cg_error_handler(CGcontext ctx, CGerror error, void *data)
{
   (void)ctx;
   (void)data;

   switch (error)
   {
      case CG_INVALID_PARAM_HANDLE_ERROR:
         RARCH_ERR("CG: Invalid param handle.\n");
         break;

      case CG_INVALID_PARAMETER_ERROR:
         RARCH_ERR("CG: Invalid parameter.\n");
         break;

      default:
         break;
   }

   RARCH_ERR("CG error: \"%s\"\n", cgGetErrorString(error));
}
#endif

static void gl_cg_reset_attrib(void *data)
{
   unsigned i;
   cg_shader_data_t *cg = (cg_shader_data_t*)data;

   /* Add sanity check that we did not overflow. */
   retro_assert(cg->attribs_index <= ARRAY_SIZE(cg->attribs_elems));

   for (i = 0; i < cg->attribs_index; i++)
      cgGLDisableClientState(cg->attribs_elems[i]);
   cg->attribs_index = 0;
}

static bool gl_cg_set_mvp(void *shader_data,
      const void *mat_data)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)shader_data;
   if (cg && cg->prg[cg->active_idx].mvp)
   {
      const math_matrix_4x4 *mat = (const math_matrix_4x4*)mat_data;
      cgGLSetMatrixParameterfc(cg->prg[cg->active_idx].mvp, mat->data);
      return true;
   }

   return false;
}

static bool gl_cg_set_coords(void *shader_data,
      const struct video_coords *coords)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)shader_data;

   if (!cg || !coords)
   {
      if (coords)
         return false;
      return true;
   }

   if (cg->prg[cg->active_idx].vertex)
      gl_cg_set_coord_array(cg->prg[cg->active_idx].vertex, cg, coords->vertex, 2);

   if (cg->prg[cg->active_idx].tex)
      gl_cg_set_coord_array(cg->prg[cg->active_idx].tex, cg, coords->tex_coord, 2);

   if (cg->prg[cg->active_idx].lut_tex)
      gl_cg_set_coord_array(cg->prg[cg->active_idx].lut_tex, cg, coords->lut_tex_coord, 2);

   if (cg->prg[cg->active_idx].color)
      gl_cg_set_coord_array(cg->prg[cg->active_idx].color, cg, coords->color, 4);

   return true;
}

static void gl_cg_set_texture_info(
      cg_shader_data_t *cg,
      const struct cg_fbo_params *params,
      const struct video_tex_info *info)
{
   CGparameter param                     = params->tex;

   cg_gl_set_texture_parameter(param, info->tex);

   set_param_2f(params->vid_size_v,
         info->input_size[0], info->input_size[1]);
   set_param_2f(params->vid_size_f,
         info->input_size[0], info->input_size[1]);
   set_param_2f(params->tex_size_v,
         info->tex_size[0],   info->tex_size[1]);
   set_param_2f(params->tex_size_f,
         info->tex_size[0],   info->tex_size[1]);

   if (params->coord)
      gl_cg_set_coord_array(params->coord, cg, info->coord, 2);
}

static void gl_cg_set_params(void *dat, void *shader_data)
{
   unsigned i;
   video_shader_ctx_params_t          *params =
      (video_shader_ctx_params_t*)dat;
   unsigned width                             = params->width;
   unsigned height                            = params->height;
   unsigned tex_width                         = params->tex_width;
   unsigned tex_height                        = params->tex_height;
   unsigned out_width                         = params->out_width;
   unsigned out_height                        = params->out_height;
   unsigned frame_count                       = params->frame_counter;
   const void *_info                          = params->info;
   const void *_prev_info                     = params->prev_info;
   const void *_feedback_info                 = params->feedback_info;
   const void *_fbo_info                      = params->fbo_info;
   unsigned fbo_info_cnt                      = params->fbo_info_cnt;
   const struct video_tex_info *info          = (const struct video_tex_info*)_info;
   const struct video_tex_info *prev_info     = (const struct video_tex_info*)_prev_info;
   const struct video_tex_info *feedback_info = (const struct video_tex_info*)_feedback_info;
   const struct video_tex_info *fbo_info      = (const struct video_tex_info*)_fbo_info;
   cg_shader_data_t *cg                       = (cg_shader_data_t*)shader_data;

   if (!cg || (cg->active_idx == 0))
         return;
   if (cg->active_idx == VIDEO_SHADER_STOCK_BLEND)
      return;

   /* Set frame. */
   set_param_2f(cg->prg[cg->active_idx].vid_size_f, width, height);
   set_param_2f(cg->prg[cg->active_idx].tex_size_f, tex_width, tex_height);
   set_param_2f(cg->prg[cg->active_idx].out_size_f, out_width, out_height);
   cg_gl_set_param_1f(cg->prg[cg->active_idx].frame_dir_f,
         state_manager_frame_is_reversed() ? -1.0 : 1.0);

   set_param_2f(cg->prg[cg->active_idx].vid_size_v, width, height);
   set_param_2f(cg->prg[cg->active_idx].tex_size_v, tex_width, tex_height);
   set_param_2f(cg->prg[cg->active_idx].out_size_v, out_width, out_height);
   cg_gl_set_param_1f(cg->prg[cg->active_idx].frame_dir_v,
         state_manager_frame_is_reversed() ? -1.0 : 1.0);

   if (cg->prg[cg->active_idx].frame_cnt_f || cg->prg[cg->active_idx].frame_cnt_v)
   {
      unsigned modulo = cg->shader->pass[cg->active_idx - 1].frame_count_mod;
      if (modulo)
         frame_count %= modulo;

      cg_gl_set_param_1f(cg->prg[cg->active_idx].frame_cnt_f, (float)frame_count);
      cg_gl_set_param_1f(cg->prg[cg->active_idx].frame_cnt_v, (float)frame_count);
   }

   /* Set lookup textures. */
   for (i = 0; i < cg->shader->luts; i++)
   {
      CGparameter fparam = cgGetNamedParameter(
            cg->prg[cg->active_idx].fprg, cg->shader->lut[i].id);
      CGparameter vparam = cgGetNamedParameter(cg->prg[cg->active_idx].vprg,
		  cg->shader->lut[i].id);

      cg_gl_set_texture_parameter(fparam, cg->lut_textures[i]);
      cg_gl_set_texture_parameter(vparam, cg->lut_textures[i]);
   }

   if (cg->active_idx)
   {
      /* Set original texture. */
      gl_cg_set_texture_info(cg, &cg->prg[cg->active_idx].orig, info);

      /* Set feedback texture. */
      gl_cg_set_texture_info(cg, &cg->prg[cg->active_idx].feedback, feedback_info);

      /* Bind FBO textures. */
      for (i = 0; i < fbo_info_cnt; i++)
         gl_cg_set_texture_info(cg, &cg->prg[cg->active_idx].fbo[i], &fbo_info[i]);
   }

   /* Set previous textures. */
   for (i = 0; i < PREV_TEXTURES; i++)
      gl_cg_set_texture_info(cg, &cg->prg[cg->active_idx].prev[i], &prev_info[i]);

   /* #pragma parameters. */
   for (i = 0; i < cg->shader->num_parameters; i++)
   {
      CGparameter param_v = cgGetNamedParameter(
            cg->prg[cg->active_idx].vprg, cg->shader->parameters[i].id);
      CGparameter param_f = cgGetNamedParameter(
            cg->prg[cg->active_idx].fprg, cg->shader->parameters[i].id);
      cg_gl_set_param_1f(param_v, cg->shader->parameters[i].current);
      cg_gl_set_param_1f(param_f, cg->shader->parameters[i].current);
   }
}

static void gl_cg_deinit_progs(void *data)
{
   unsigned i;
   cg_shader_data_t *cg = (cg_shader_data_t*)data;

   if (!cg)
      return;

   RARCH_LOG("[CG]: Destroying programs.\n");
   cgGLUnbindProgram(cg->cgFProf);
   cgGLUnbindProgram(cg->cgVProf);

   /* Programs may alias [0]. */
   for (i = 1; i < GFX_MAX_SHADERS; i++)
   {
      if (cg->prg[i].fprg && cg->prg[i].fprg != cg->prg[0].fprg)
         cgDestroyProgram(cg->prg[i].fprg);
      if (cg->prg[i].vprg && cg->prg[i].vprg != cg->prg[0].vprg)
         cgDestroyProgram(cg->prg[i].vprg);
   }

   if (cg->prg[0].fprg)
      cgDestroyProgram(cg->prg[0].fprg);
   if (cg->prg[0].vprg)
      cgDestroyProgram(cg->prg[0].vprg);

   memset(cg->prg, 0, sizeof(cg->prg));
}

static void gl_cg_destroy_resources(void *data)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;
   if (!cg)
      return;

   gl_cg_reset_attrib(data);

   gl_cg_deinit_progs(data);

   if (cg->shader && cg->shader->luts)
   {
      glDeleteTextures(cg->shader->luts, cg->lut_textures);
      memset(cg->lut_textures, 0, sizeof(cg->lut_textures));
   }

   free(cg->shader);
   cg->shader = NULL;
}

/* Final deinit. */
static void gl_cg_deinit_context_state(void *data)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;
   if (cg->cgCtx)
   {
      RARCH_LOG("[CG]: Destroying context.\n");
      cgDestroyContext(cg->cgCtx);
   }
   cg->cgCtx = NULL;
}

/* Full deinit. */
static void gl_cg_deinit(void *data)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;
   if (!cg)
      return;

   gl_cg_destroy_resources(cg);
   gl_cg_deinit_context_state(cg);

   free(cg);
}

static bool gl_cg_compile_program(
      void *data,
      unsigned idx,
      void *program_data,
      struct shader_program_info *program_info)
{
   const char *argv[2 + GFX_MAX_SHADERS];
   const char *list                  = NULL;
   bool ret                          = true;
   char *listing_f                   = NULL;
   char *listing_v                   = NULL;
   unsigned i, argc                  = 0;
   struct shader_program_cg *program = (struct shader_program_cg*)program_data;
   cg_shader_data_t              *cg = (cg_shader_data_t*)data;

   if (!program)
      program = &cg->prg[idx];

   argv[argc++] = "-DPARAMETER_UNIFORM";

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      if (*(cg->alias_define[i]))
         argv[argc++] = cg->alias_define[i];
   }

   argv[argc] = NULL;

   if (program_info->is_file)
      program->fprg = cgCreateProgramFromFile(
            cg->cgCtx, CG_SOURCE,
            program_info->combined, cg->cgFProf, "main_fragment", argv);
   else
      program->fprg = cgCreateProgram(cg->cgCtx, CG_SOURCE,
            program_info->combined, cg->cgFProf, "main_fragment", argv);

   list = cgGetLastListing(cg->cgCtx);

   if (list)
      listing_f = strdup(list);

   list = NULL;

   if (program_info->is_file)
      program->vprg = cgCreateProgramFromFile(
            cg->cgCtx, CG_SOURCE,
            program_info->combined, cg->cgVProf, "main_vertex", argv);
   else
      program->vprg = cgCreateProgram(cg->cgCtx, CG_SOURCE,
            program_info->combined, cg->cgVProf, "main_vertex", argv);

   list = cgGetLastListing(cg->cgCtx);

   if (list)
      listing_v = strdup(list);

   if (!program->fprg || !program->vprg)
   {
      RARCH_ERR("CG error: %s\n", cgGetErrorString(cgGetError()));
      if (listing_f)
         RARCH_ERR("Fragment:\n%s\n", listing_f);
      else if (listing_v)
         RARCH_ERR("Vertex:\n%s\n", listing_v);

      ret = false;
      goto end;
   }

   cgGLLoadProgram(program->fprg);
   cgGLLoadProgram(program->vprg);

end:
   free(listing_f);
   free(listing_v);
   return ret;
}

static void gl_cg_set_program_base_attrib(void *data, unsigned i)
{
   cg_shader_data_t      *cg = (cg_shader_data_t*)data;
   CGparameter         param = cgGetFirstParameter(
         cg->prg[i].vprg, CG_PROGRAM);

   for (; param; param = cgGetNextParameter(param))
   {
      const char *semantic = NULL;
      if (     (cgGetParameterDirection(param)   != CG_IN)
            || (cgGetParameterVariability(param) != CG_VARYING))
         continue;

      semantic = cgGetParameterSemantic(param);
      if (!semantic)
         continue;

      RARCH_LOG("[CG]: Found semantic \"%s\" in prog #%u.\n", semantic, i);

      if (
            string_is_equal(semantic, "TEXCOORD") ||
            string_is_equal(semantic, "TEXCOORD0")
         )
         cg->prg[i].tex     = param;
      else if (
            string_is_equal(semantic, "COLOR") ||
            string_is_equal(semantic, "COLOR0")
            )
            cg->prg[i].color   = param;
      else if (string_is_equal(semantic, "POSITION"))
         cg->prg[i].vertex  = param;
      else if (string_is_equal(semantic, "TEXCOORD1"))
         cg->prg[i].lut_tex = param;
   }

   if (!cg->prg[i].tex)
      cg->prg[i].tex     = cgGetNamedParameter(cg->prg[i].vprg, "IN.tex_coord");
   if (!cg->prg[i].color)
      cg->prg[i].color   = cgGetNamedParameter(cg->prg[i].vprg, "IN.color");
   if (!cg->prg[i].vertex)
      cg->prg[i].vertex  = cgGetNamedParameter(cg->prg[i].vprg, "IN.vertex_coord");
   if (!cg->prg[i].lut_tex)
      cg->prg[i].lut_tex = cgGetNamedParameter(cg->prg[i].vprg, "IN.lut_tex_coord");
}

static bool gl_cg_load_stock(void *data)
{
   struct shader_program_info program_info;
   cg_shader_data_t *cg  = (cg_shader_data_t*)data;

   program_info.combined = stock_cg_gl_program;
   program_info.is_file  = false;

   if (!gl_cg_compile_program(data, 0, &cg->prg[0], &program_info))
      goto error;

   gl_cg_set_program_base_attrib(data, 0);

   return true;

error:
   RARCH_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
   return false;
}

static bool gl_cg_load_plain(void *data, const char *path)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;

   if (!gl_cg_load_stock(cg))
      return false;

   cg->shader = (struct video_shader*)
      calloc(1, sizeof(*cg->shader));
   if (!cg->shader)
      return false;

   cg->shader->passes = 1;

   if (string_is_empty(path))
   {
      RARCH_LOG("[CG]: Loading stock Cg file.\n");
      cg->prg[1] = cg->prg[0];
   }
   else
   {
      struct shader_program_info program_info;

      program_info.combined = path;
      program_info.is_file  = true;

      RARCH_LOG("[CG]: Loading Cg file: %s\n", path);
      strlcpy(cg->shader->pass[0].source.path, path,
            sizeof(cg->shader->pass[0].source.path));
      if (!gl_cg_compile_program(data, 1, &cg->prg[1], &program_info))
         return false;
   }

   video_shader_resolve_parameters(NULL, cg->shader);
   return true;
}

static bool gl_cg_load_shader(void *data, unsigned i)
{
   struct shader_program_info program_info;
   cg_shader_data_t *cg  = (cg_shader_data_t*)data;

   program_info.combined = cg->shader->pass[i].source.path;
   program_info.is_file  = true;

   RARCH_LOG("[CG]: Loading Cg shader: \"%s\".\n",
         cg->shader->pass[i].source.path);

   if (!gl_cg_compile_program(data, i + 1, &cg->prg[i + 1],&program_info))
      return false;

   return true;
}

static bool gl_cg_load_preset(void *data, const char *path)
{
   unsigned i;
   config_file_t  *conf = NULL;
   cg_shader_data_t *cg = (cg_shader_data_t*)data;

   if (!gl_cg_load_stock(cg))
      return false;

   RARCH_LOG("[CG]: Loading Cg meta-shader: %s\n", path);
   conf = video_shader_read_preset(path);
   if (!conf)
   {
      RARCH_ERR("Failed to load preset.\n");
      return false;
   }

   cg->shader = (struct video_shader*)calloc(1, sizeof(*cg->shader));
   if (!cg->shader)
   {
      config_file_free(conf);
      return false;
   }

   if (!video_shader_read_conf_preset(conf, cg->shader))
   {
      RARCH_ERR("Failed to parse CGP file.\n");
      config_file_free(conf);
      return false;
   }

   video_shader_resolve_parameters(conf, cg->shader);
   config_file_free(conf);

   if (cg->shader->passes > GFX_MAX_SHADERS - 3)
   {
      RARCH_WARN("Too many shaders ... Capping shader amount to %d.\n",
            GFX_MAX_SHADERS - 3);
      cg->shader->passes = GFX_MAX_SHADERS - 3;
   }

   for (i = 0; i < cg->shader->passes; i++)
   {
      if (*cg->shader->pass[i].alias)
         snprintf(cg->alias_define[i],
               sizeof(cg->alias_define[i]),
               "-D%s_ALIAS",
               cg->shader->pass[i].alias);
   }

   for (i = 0; i < cg->shader->passes; i++)
   {
      if (!gl_cg_load_shader(cg, i))
      {
         RARCH_ERR("Failed to load shaders ...\n");
         return false;
      }
   }

   if (!gl_load_luts(cg->shader, cg->lut_textures))
   {
      RARCH_ERR("Failed to load lookup textures ...\n");
      return false;
   }

   return true;
}

static void gl_cg_set_pass_attrib(
      struct shader_program_cg *program,
      struct cg_fbo_params *fbo,
      const char *attr)
{
   char attr_buf[64];

   attr_buf[0] = '\0';

   snprintf(attr_buf, sizeof(attr_buf), "%s.texture", attr);
   if (!fbo->tex)
      fbo->tex = cgGetNamedParameter(program->fprg, attr_buf);

   snprintf(attr_buf, sizeof(attr_buf), "%s.video_size", attr);
   if (!fbo->vid_size_v)
      fbo->vid_size_v = cgGetNamedParameter(program->vprg, attr_buf);
   if (!fbo->vid_size_f)
      fbo->vid_size_f = cgGetNamedParameter(program->fprg, attr_buf);

   snprintf(attr_buf, sizeof(attr_buf), "%s.texture_size", attr);
   if (!fbo->tex_size_v)
      fbo->tex_size_v = cgGetNamedParameter(program->vprg, attr_buf);
   if (!fbo->tex_size_f)
      fbo->tex_size_f = cgGetNamedParameter(program->fprg, attr_buf);

   snprintf(attr_buf, sizeof(attr_buf), "%s.tex_coord", attr);
   if (!fbo->coord)
      fbo->coord = cgGetNamedParameter(program->vprg, attr_buf);
}

static INLINE void gl_cg_set_shaders(CGprogram frag, CGprogram vert)
{
   cgGLBindProgram(frag);
   cgGLBindProgram(vert);
}

static void gl_cg_set_program_attributes(void *data, unsigned i)
{
   unsigned j;
   cg_shader_data_t *cg = (cg_shader_data_t*)data;

   if (!cg)
      return;

   gl_cg_set_shaders(cg->prg[i].fprg, cg->prg[i].vprg);

   gl_cg_set_program_base_attrib(cg, i);

   cg->prg[i].vid_size_f = cgGetNamedParameter (cg->prg[i].fprg, "IN.video_size");
   cg->prg[i].tex_size_f = cgGetNamedParameter (cg->prg[i].fprg, "IN.texture_size");
   cg->prg[i].out_size_f = cgGetNamedParameter (cg->prg[i].fprg, "IN.output_size");
   cg->prg[i].frame_cnt_f = cgGetNamedParameter(cg->prg[i].fprg, "IN.frame_count");
   cg->prg[i].frame_dir_f = cgGetNamedParameter(cg->prg[i].fprg, "IN.frame_direction");
   cg->prg[i].vid_size_v = cgGetNamedParameter (cg->prg[i].vprg, "IN.video_size");
   cg->prg[i].tex_size_v = cgGetNamedParameter (cg->prg[i].vprg, "IN.texture_size");
   cg->prg[i].out_size_v = cgGetNamedParameter (cg->prg[i].vprg, "IN.output_size");
   cg->prg[i].frame_cnt_v = cgGetNamedParameter(cg->prg[i].vprg, "IN.frame_count");
   cg->prg[i].frame_dir_v = cgGetNamedParameter(cg->prg[i].vprg, "IN.frame_direction");

   cg->prg[i].mvp                 = cgGetNamedParameter(cg->prg[i].vprg, "modelViewProj");
   if (!cg->prg[i].mvp)
      cg->prg[i].mvp              = cgGetNamedParameter(cg->prg[i].vprg, "IN.mvp_matrix");

   cg->prg[i].orig.tex            = cgGetNamedParameter(cg->prg[i].fprg, "ORIG.texture");
   cg->prg[i].orig.vid_size_v     = cgGetNamedParameter(cg->prg[i].vprg, "ORIG.video_size");
   cg->prg[i].orig.vid_size_f     = cgGetNamedParameter(cg->prg[i].fprg, "ORIG.video_size");
   cg->prg[i].orig.tex_size_v     = cgGetNamedParameter(cg->prg[i].vprg, "ORIG.texture_size");
   cg->prg[i].orig.tex_size_f     = cgGetNamedParameter(cg->prg[i].fprg, "ORIG.texture_size");
   cg->prg[i].orig.coord          = cgGetNamedParameter(cg->prg[i].vprg, "ORIG.tex_coord");

   cg->prg[i].feedback.tex        = cgGetNamedParameter(cg->prg[i].fprg, "FEEDBACK.texture");
   cg->prg[i].feedback.vid_size_v = cgGetNamedParameter(cg->prg[i].vprg, "FEEDBACK.video_size");
   cg->prg[i].feedback.vid_size_f = cgGetNamedParameter(cg->prg[i].fprg, "FEEDBACK.video_size");
   cg->prg[i].feedback.tex_size_v = cgGetNamedParameter(cg->prg[i].vprg, "FEEDBACK.texture_size");
   cg->prg[i].feedback.tex_size_f = cgGetNamedParameter(cg->prg[i].fprg, "FEEDBACK.texture_size");
   cg->prg[i].feedback.coord      = cgGetNamedParameter(cg->prg[i].vprg, "FEEDBACK.tex_coord");

   if (i > 1)
   {
      char pass_str[64];

      pass_str[0] = '\0';

      snprintf(pass_str, sizeof(pass_str), "PASSPREV%u", i);
      gl_cg_set_pass_attrib(&cg->prg[i], &cg->prg[i].orig, pass_str);
   }

   for (j = 0; j < PREV_TEXTURES; j++)
   {
      char attr_buf_tex[64];
      char attr_buf_vid_size[64];
      char attr_buf_tex_size[64];
      char attr_buf_coord[64];
      static const char *prev_names[PREV_TEXTURES] = {
         "PREV",
         "PREV1",
         "PREV2",
         "PREV3",
         "PREV4",
         "PREV5",
         "PREV6",
      };

      attr_buf_tex[0] = attr_buf_vid_size[0] = attr_buf_tex_size[0] =
         attr_buf_coord[0] = '\0';

      snprintf(attr_buf_tex,      sizeof(attr_buf_tex),
            "%s.texture", prev_names[j]);
      snprintf(attr_buf_vid_size, sizeof(attr_buf_vid_size),
            "%s.video_size", prev_names[j]);
      snprintf(attr_buf_tex_size, sizeof(attr_buf_tex_size),
            "%s.texture_size", prev_names[j]);
      snprintf(attr_buf_coord,    sizeof(attr_buf_coord),
            "%s.tex_coord", prev_names[j]);

      cg->prg[i].prev[j].tex = cgGetNamedParameter(cg->prg[i].fprg,
            attr_buf_tex);

      cg->prg[i].prev[j].vid_size_v =
         cgGetNamedParameter(cg->prg[i].vprg, attr_buf_vid_size);
      cg->prg[i].prev[j].vid_size_f =
         cgGetNamedParameter(cg->prg[i].fprg, attr_buf_vid_size);

      cg->prg[i].prev[j].tex_size_v =
         cgGetNamedParameter(cg->prg[i].vprg, attr_buf_tex_size);
      cg->prg[i].prev[j].tex_size_f =
         cgGetNamedParameter(cg->prg[i].fprg, attr_buf_tex_size);

      cg->prg[i].prev[j].coord = cgGetNamedParameter(cg->prg[i].vprg,
            attr_buf_coord);
   }

   for (j = 0; j + 1 < i; j++)
   {
      char pass_str[64];

      pass_str[0] = '\0';

      snprintf(pass_str, sizeof(pass_str), "PASS%u", j + 1);
      gl_cg_set_pass_attrib(&cg->prg[i], &cg->prg[i].fbo[j], pass_str);
      snprintf(pass_str, sizeof(pass_str), "PASSPREV%u", i - (j + 1));
      gl_cg_set_pass_attrib(&cg->prg[i], &cg->prg[i].fbo[j], pass_str);

      if (*cg->shader->pass[j].alias)
         gl_cg_set_pass_attrib(&cg->prg[i], &cg->prg[i].fbo[j],
               cg->shader->pass[j].alias);
   }
}

static void gl_cg_init_menu_shaders(void *data)
{
   struct shader_program_info shader_prog_info;
   cg_shader_data_t *cg = (cg_shader_data_t*)data;

   if (!cg)
      return;

#ifdef HAVE_SHADERPIPELINE
   shader_prog_info.combined = stock_xmb_ribbon_simple;
   shader_prog_info.is_file  = false;

   gl_cg_compile_program(
         cg,
         VIDEO_SHADER_MENU,
         &cg->prg[VIDEO_SHADER_MENU],
         &shader_prog_info);
   gl_cg_set_program_base_attrib(cg, VIDEO_SHADER_MENU);

   shader_prog_info.combined = stock_xmb_ribbon_simple;
   shader_prog_info.is_file  = false;

   gl_cg_compile_program(
         cg,
         VIDEO_SHADER_MENU_2,
         &cg->prg[VIDEO_SHADER_MENU_2],
         &shader_prog_info);
   gl_cg_set_program_base_attrib(cg, VIDEO_SHADER_MENU_2);

   shader_prog_info.combined = stock_xmb_snow;
   shader_prog_info.is_file  = false;

   gl_cg_compile_program(
         cg,
         VIDEO_SHADER_MENU_3,
         &cg->prg[VIDEO_SHADER_MENU_3],
         &shader_prog_info);
   gl_cg_set_program_base_attrib(cg, VIDEO_SHADER_MENU_3);
#endif
}

static void *gl_cg_init(void *data, const char *path)
{
   unsigned i;
   cg_shader_data_t *cg = (cg_shader_data_t*)
      calloc(1, sizeof(cg_shader_data_t));

   if (!cg)
      return NULL;

#ifdef HAVE_CG_RUNTIME_COMPILER
   cgRTCgcInit();
#endif

   cg->cgCtx = cgCreateContext();

   if (!cg->cgCtx)
   {
      RARCH_ERR("Failed to create Cg context.\n");
      goto error;
   }

#ifdef RARCH_CG_DEBUG
   cgGLSetDebugMode(CG_TRUE);
   cgSetErrorHandler(cg_error_handler, NULL);
#endif

   cg->cgFProf = cgGLGetLatestProfile(CG_GL_FRAGMENT);
   cg->cgVProf = cgGLGetLatestProfile(CG_GL_VERTEX);

   if (
         cg->cgFProf == CG_PROFILE_UNKNOWN ||
         cg->cgVProf == CG_PROFILE_UNKNOWN)
   {
      RARCH_ERR("Invalid profile type\n");
      goto error;
   }

   RARCH_LOG("[CG]: Vertex profile: %s\n",   cgGetProfileString(cg->cgVProf));
   RARCH_LOG("[CG]: Fragment profile: %s\n", cgGetProfileString(cg->cgFProf));
   cgGLSetOptimalOptions(cg->cgFProf);
   cgGLSetOptimalOptions(cg->cgVProf);
   cgGLEnableProfile(cg->cgFProf);
   cgGLEnableProfile(cg->cgVProf);

   memset(cg->alias_define, 0, sizeof(cg->alias_define));

   {
      bool is_preset;
      enum rarch_shader_type type =
         video_shader_get_type_from_ext(path_get_extension(path), &is_preset);

      if (!string_is_empty(path) && type != RARCH_SHADER_CG)
      {
         RARCH_ERR("[CG]: Invalid shader type, falling back to stock.\n");
         path = NULL;
      }

      if (!string_is_empty(path) && is_preset)
      {
         if (!gl_cg_load_preset(cg, path))
            goto error;
      }
      else
      {
         if (!gl_cg_load_plain(cg, path))
            goto error;
      }
   }

   cg->prg[0].mvp = cgGetNamedParameter(cg->prg[0].vprg, "IN.mvp_matrix");

   for (i = 1; i <= cg->shader->passes; i++)
      gl_cg_set_program_attributes(cg, i);

   /* If we aren't using last pass non-FBO shader,
    * this shader will be assumed to be "fixed-function".
    *
    * Just use prg[0] for that pass, which will be
    * pass-through. */
   cg->prg[cg->shader->passes + 1] = cg->prg[0];

   /* No need to apply Android hack in Cg. */
   cg->prg[VIDEO_SHADER_STOCK_BLEND] = cg->prg[0];

   gl_cg_set_shaders(cg->prg[1].fprg, cg->prg[1].vprg);

   gl_cg_reset_attrib(cg);

   return cg;

error:
   gl_cg_destroy_resources(cg);
   if (!cg)
      free(cg);
   return NULL;
}

static void gl_cg_use(void *data, void *shader_data, unsigned idx, bool set_active)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)shader_data;
   if (cg && cg->prg[idx].vprg && cg->prg[idx].fprg)
   {
      if (set_active)
      {
         gl_cg_reset_attrib(cg);
         cg->active_idx = idx;
      }

      gl_cg_set_shaders(cg->prg[idx].fprg, cg->prg[idx].vprg);
   }
}

static unsigned gl_cg_num(void *data)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;
   if (!cg)
      return 0;
   return cg->shader->passes;
}

static bool gl_cg_filter_type(void *data, unsigned idx, bool *smooth)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;
   if (cg && idx &&
         (cg->shader->pass[idx - 1].filter != RARCH_FILTER_UNSPEC)
      )
   {
      *smooth = (cg->shader->pass[idx - 1].filter == RARCH_FILTER_LINEAR);
      return true;
   }

   return false;
}

static enum gfx_wrap_type gl_cg_wrap_type(void *data, unsigned idx)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;
   if (cg && idx)
      return cg->shader->pass[idx - 1].wrap;
   return RARCH_WRAP_BORDER;
}

static void gl_cg_shader_scale(void *data, unsigned idx, struct gfx_fbo_scale *scale)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;
   if (cg && idx)
      *scale = cg->shader->pass[idx - 1].fbo;
   else
      scale->valid = false;
}

static unsigned gl_cg_get_prev_textures(void *data)
{
   unsigned i, j;
   unsigned max_prev = 0;
   cg_shader_data_t *cg = (cg_shader_data_t*)data;

   if (!cg)
      return 0;

   for (i = 1; i <= cg->shader->passes; i++)
      for (j = 0; j < PREV_TEXTURES; j++)
         if (cg->prg[i].prev[j].tex)
            max_prev = MAX(j + 1, max_prev);

   return max_prev;
}

static bool gl_cg_get_feedback_pass(void *data, unsigned *pass)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;
   if (!cg || cg->shader->feedback_pass < 0)
      return false;

   *pass = cg->shader->feedback_pass;
   return true;
}

static bool gl_cg_mipmap_input(void *data, unsigned idx)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;
   if (cg && idx)
      return cg->shader->pass[idx - 1].mipmap;
   return false;
}

static struct video_shader *gl_cg_get_current_shader(void *data)
{
   cg_shader_data_t *cg = (cg_shader_data_t*)data;
   if (!cg)
      return NULL;
   return cg->shader;
}

static void gl_cg_get_flags(uint32_t *flags)
{
   BIT32_SET(*flags, GFX_CTX_FLAGS_SHADERS_CG);
}

const shader_backend_t gl_cg_backend = {
   gl_cg_init,
   gl_cg_init_menu_shaders,
   gl_cg_deinit,
   gl_cg_set_params,
   gl_cg_set_uniform_parameter,
   gl_cg_compile_program,
   gl_cg_use,
   gl_cg_num,
   gl_cg_filter_type,
   gl_cg_wrap_type,
   gl_cg_shader_scale,
   gl_cg_set_coords,
   gl_cg_set_mvp,
   gl_cg_get_prev_textures,
   gl_cg_get_feedback_pass,
   gl_cg_mipmap_input,
   gl_cg_get_current_shader,
   gl_cg_get_flags,

   RARCH_SHADER_CG,
   "cg"
};
