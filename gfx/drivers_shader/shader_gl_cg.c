/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifdef _MSC_VER
#pragma comment(lib, "cg")
#pragma comment(lib, "cggl")
#endif

#include <stdint.h>
#include <string.h>

#include <Cg/cg.h>
#ifdef HAVE_OPENGL
#include "../common/gl_common.h"
#include <Cg/cgGL.h>
#endif

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <rhash.h>
#include <string/stdstring.h>

#include "../video_shader_driver.h"
#include "../video_shader_parse.h"
#include "../../libretro_version_1.h"
#include "../../dynamic.h"
#include "../../rewind.h"
#include "../video_state_tracker.h"

#define SEMANTIC_TEXCOORD     0x92ee91cdU
#define SEMANTIC_TEXCOORD0    0xf0c0cb9dU
#define SEMANTIC_TEXCOORD1    0xf0c0cb9eU
#define SEMANTIC_COLOR        0x0ce809a4U
#define SEMANTIC_COLOR0       0xa9e93e54U
#define SEMANTIC_POSITION     0xd87309baU

enum uniform_type
{
   UNIFORM_1F = 0,
   UNIFORM_2F
};

struct uniform_info
{
   CGparameter data;
   enum uniform_type type;

   struct
   {
      struct
      {
         float a;
         float b;
         float c;
      } f;
   } result;
};

static void cg_uniform_set_parameter(void *data)
{
   struct uniform_info *param = (struct uniform_info*)data;

   if (!param)
      return;

   switch (param->type)
   {
      case UNIFORM_1F:
         cgGLSetParameter1f(param->data, param->result.f.a);
         break;
      case UNIFORM_2F:
         cgGLSetParameter2f(param->data, param->result.f.a, param->result.f.b);
         break;
   }
}

#if 0
#define RARCH_CG_DEBUG
#endif

/* Used when we call deactivate() since just unbinding 
 * the program didn't seem to work... */
static const char *stock_cg_gl_program =
      "struct input"
      "{"
      "  float2 tex_coord;"
      "  float4 color;"
      "  float4 vertex_coord;"
      "  uniform float4x4 mvp_matrix;"
      "  uniform sampler2D texture;"
      "};"
      "struct vertex_data"
      "{"
      "  float2 tex;"
      "  float4 color;"
      "};"
      "void main_vertex"
      "("
      "	out float4 oPosition : POSITION,"
      "  input IN,"
      "  out vertex_data vert"
      ")"
      "{"
      "	oPosition = mul(IN.mvp_matrix, IN.vertex_coord);"
      "  vert = vertex_data(IN.tex_coord, IN.color);"
      "}"
      ""
      "float4 main_fragment(input IN, vertex_data vert, uniform sampler2D s0 : TEXUNIT0) : COLOR"
      "{"
      "  return vert.color * tex2D(s0, vert.tex);"
      "}";

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


struct cg_fbo_params
{
   CGparameter vid_size_f;
   CGparameter tex_size_f;
   CGparameter vid_size_v;
   CGparameter tex_size_v;
   CGparameter tex;
   CGparameter coord;
};

#define PREV_TEXTURES (GFX_MAX_TEXTURES - 1)

struct cg_program
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
   struct cg_program prg[GFX_MAX_SHADERS];
   unsigned active_idx;
   unsigned cg_attrib_idx;
   CGprofile cgVProf;
   CGprofile cgFProf;
   struct video_shader *shader;
   state_tracker_t *state_tracker;
   GLuint lut_textures[GFX_MAX_TEXTURES];
   CGparameter cg_attribs[PREV_TEXTURES + 2 + 4 + GFX_MAX_SHADERS];
   char cg_alias_define[GFX_MAX_SHADERS][128];
   CGcontext cgCtx;
} cg_shader_data_t;

static void gl_cg_reset_attrib(void *data)
{
   unsigned i;
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;

   /* Add sanity check that we did not overflow. */
   retro_assert(cg_data->cg_attrib_idx <= ARRAY_SIZE(cg_data->cg_attribs));

   for (i = 0; i < cg_data->cg_attrib_idx; i++)
      cgGLDisableClientState(cg_data->cg_attribs[i]);
   cg_data->cg_attrib_idx = 0;
}

static bool gl_cg_set_mvp(void *data, void *shader_data, const math_matrix_4x4 *mat)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)shader_data;
   if (cg_data && cg_data->prg[cg_data->active_idx].mvp)
   {
      cgGLSetMatrixParameterfc(cg_data->prg[cg_data->active_idx].mvp, mat->data);
      return true;
   }

   gl_ff_matrix(mat);
   return false;
}

#define SET_COORD(cg_data, name, coords_name, len) do { \
   if (cg_data->prg[cg_data->active_idx].name) \
   { \
      cgGLSetParameterPointer(cg_data->prg[cg_data->active_idx].name, len, GL_FLOAT, 0, coords->coords_name); \
      cgGLEnableClientState(cg_data->prg[cg_data->active_idx].name); \
      cg_data->cg_attribs[cg_data->cg_attrib_idx++] = cg_data->prg[cg_data->active_idx].name; \
   } \
} while(0)

static bool gl_cg_set_coords(void *handle_data, void *shader_data, const void *data)
{
   const struct gfx_coords *coords = (const struct gfx_coords*)data;
   cg_shader_data_t *cg_data = (cg_shader_data_t*)shader_data;

   if (!cg_data || !coords)
      goto fallback;

   SET_COORD(cg_data, vertex, vertex, 2);
   SET_COORD(cg_data, tex, tex_coord, 2);
   SET_COORD(cg_data, lut_tex, lut_tex_coord, 2);
   SET_COORD(cg_data, color, color, 4);

   return true;
fallback:
   gl_ff_vertex(coords);
   return false;
}


static void gl_cg_set_texture_info(
      cg_shader_data_t *cg_data, 
      const struct cg_fbo_params *params,
      const struct gfx_tex_info *info)
{
   unsigned i;
   struct uniform_info uniform_params[4];
   CGparameter param = params->tex;

   if (param)
   {
      cgGLSetTextureParameter(param, info->tex);
      cgGLEnableTextureParameter(param);
   }

   uniform_params[0].data          = params->vid_size_v;
   uniform_params[0].type          = UNIFORM_2F;
   uniform_params[0].result.f.a    = info->input_size[0];
   uniform_params[0].result.f.b    = info->input_size[1];

   uniform_params[1].data          = params->vid_size_f;
   uniform_params[1].type          = UNIFORM_2F;
   uniform_params[1].result.f.a    = info->input_size[0];
   uniform_params[1].result.f.b    = info->input_size[1];

   uniform_params[2].data          = params->tex_size_v;
   uniform_params[2].type          = UNIFORM_2F;
   uniform_params[2].result.f.a    = info->tex_size[0];
   uniform_params[2].result.f.b    = info->tex_size[1];

   uniform_params[3].data          = params->tex_size_f;
   uniform_params[3].type          = UNIFORM_2F;
   uniform_params[3].result.f.a    = info->tex_size[0];
   uniform_params[3].result.f.b    = info->tex_size[1];

   for (i = 0; i < 4; i++)
      cg_uniform_set_parameter(&uniform_params[i]);

   if (params->coord)
   {
      cgGLSetParameterPointer(params->coord, 2,
            GL_FLOAT, 0, info->coord);
      cgGLEnableClientState(params->coord);
      cg_data->cg_attribs[cg_data->cg_attrib_idx++] = params->coord;
   }
}

static void gl_cg_set_params(void *data, void *shader_data,
      unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height,
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const void *_info,
      const void *_prev_info,
      const void *_feedback_info,
      const void *_fbo_info,
      unsigned fbo_info_cnt)
{
   unsigned i;
   unsigned uniform_count = 0;
   const struct gfx_tex_info *info = (const struct gfx_tex_info*)_info;
   const struct gfx_tex_info *prev_info = (const struct gfx_tex_info*)_prev_info;
   const struct gfx_tex_info *feedback_info = (const struct gfx_tex_info*)_feedback_info;
   const struct gfx_tex_info *fbo_info = (const struct gfx_tex_info*)_fbo_info;
   cg_shader_data_t *cg_data = (cg_shader_data_t*)shader_data;
   struct uniform_info uniform_params[10];

   if (!cg_data || (cg_data->active_idx == 0) ||
         (cg_data->active_idx == GL_SHADER_STOCK_BLEND))
      return;

   /* Set frame. */
   uniform_params[0].data          = cg_data->prg[cg_data->active_idx].vid_size_f;
   uniform_params[0].type          = UNIFORM_2F;
   uniform_params[0].result.f.a    = width;
   uniform_params[0].result.f.b    = height;
   uniform_count++;
   
   uniform_params[1].data          = cg_data->prg[cg_data->active_idx].tex_size_f;
   uniform_params[1].type          = UNIFORM_2F;
   uniform_params[1].result.f.a    = tex_width;
   uniform_params[1].result.f.b    = tex_height;
   uniform_count++;

   uniform_params[2].data          = cg_data->prg[cg_data->active_idx].out_size_f;
   uniform_params[2].type          = UNIFORM_2F;
   uniform_params[2].result.f.a    = out_width;
   uniform_params[2].result.f.b    = out_height;
   uniform_count++;

   uniform_params[3].data          = cg_data->prg[cg_data->active_idx].frame_dir_f;
   uniform_params[3].type          = UNIFORM_1F;
   uniform_params[3].result.f.a    = state_manager_frame_is_reversed() ? -1.0 : 1.0;
   uniform_count++;

   uniform_params[4].data          = cg_data->prg[cg_data->active_idx].vid_size_v;
   uniform_params[4].type          = UNIFORM_2F;
   uniform_params[4].result.f.a    = width;
   uniform_params[4].result.f.b    = height;
   uniform_count++;

   uniform_params[5].data          = cg_data->prg[cg_data->active_idx].tex_size_v;
   uniform_params[5].type          = UNIFORM_2F;
   uniform_params[5].result.f.a    = tex_width;
   uniform_params[5].result.f.b    = tex_height;
   uniform_count++;

   uniform_params[6].data          = cg_data->prg[cg_data->active_idx].out_size_v;
   uniform_params[6].type          = UNIFORM_2F;
   uniform_params[6].result.f.a    = out_width;
   uniform_params[6].result.f.b    = out_height;
   uniform_count++;

   uniform_params[7].data          = cg_data->prg[cg_data->active_idx].frame_dir_v;
   uniform_params[7].type          = UNIFORM_1F;
   uniform_params[7].result.f.a    = state_manager_frame_is_reversed() ? -1.0 : 1.0;
   uniform_count++;

   if (cg_data->prg[cg_data->active_idx].frame_cnt_f || cg_data->prg[cg_data->active_idx].frame_cnt_v)
   {
      unsigned modulo = cg_data->shader->pass[cg_data->active_idx - 1].frame_count_mod;
      if (modulo)
         frame_count %= modulo;

      uniform_params[8].data          = cg_data->prg[cg_data->active_idx].frame_cnt_f;
      uniform_params[8].type          = UNIFORM_1F;
      uniform_params[8].result.f.a    = (float)frame_count;
      uniform_count++;

      uniform_params[9].data          = cg_data->prg[cg_data->active_idx].frame_cnt_v;
      uniform_params[9].type          = UNIFORM_1F;
      uniform_params[9].result.f.a    = (float)frame_count;
      uniform_count++;
   }

   for (i = 0; i < (uniform_count+1); i++)
      cg_uniform_set_parameter(&uniform_params[i]);

   /* Set orig texture. */
   gl_cg_set_texture_info(cg_data, &cg_data->prg[cg_data->active_idx].orig, info);

   /* Set feedback texture. */
   gl_cg_set_texture_info(cg_data, &cg_data->prg[cg_data->active_idx].feedback, feedback_info);

   /* Set previous textures. */
   for (i = 0; i < PREV_TEXTURES; i++)
      gl_cg_set_texture_info(cg_data, &cg_data->prg[cg_data->active_idx].prev[i], &prev_info[i]);

   /* Set lookup textures. */
   for (i = 0; i < cg_data->shader->luts; i++)
   {
      CGparameter vparam;
      CGparameter fparam = cgGetNamedParameter(
            cg_data->prg[cg_data->active_idx].fprg, cg_data->shader->lut[i].id);

      if (fparam)
      {
         cgGLSetTextureParameter(fparam, cg_data->lut_textures[i]);
         cgGLEnableTextureParameter(fparam);
      }

      vparam = cgGetNamedParameter(cg_data->prg[cg_data->active_idx].vprg,
		  cg_data->shader->lut[i].id);

      if (vparam)
      {
         cgGLSetTextureParameter(vparam, cg_data->lut_textures[i]);
         cgGLEnableTextureParameter(vparam);
      }
   }

   /* Set FBO textures. */
   if (cg_data->active_idx)
   {
      for (i = 0; i < fbo_info_cnt; i++)
         gl_cg_set_texture_info(cg_data, &cg_data->prg[cg_data->active_idx].fbo[i], &fbo_info[i]);
   }

   /* #pragma parameters. */
   for (i = 0; i < cg_data->shader->num_parameters; i++)
   {
      struct uniform_info pragma_params[2];

      CGparameter param_v = cgGetNamedParameter(
            cg_data->prg[cg_data->active_idx].vprg, cg_data->shader->parameters[i].id);
      CGparameter param_f = cgGetNamedParameter(
            cg_data->prg[cg_data->active_idx].fprg, cg_data->shader->parameters[i].id);

      uniform_params[0].data          = param_v;
      uniform_params[0].type          = UNIFORM_1F;
      uniform_params[0].result.f.a    = cg_data->shader->parameters[i].current;

      uniform_params[1].data          = param_f;
      uniform_params[1].type          = UNIFORM_1F;
      uniform_params[1].result.f.a    = cg_data->shader->parameters[i].current;

      cg_uniform_set_parameter(&pragma_params[i]);
   }

   /* Set state parameters. */
   if (cg_data->state_tracker)
   {
      /* Only query uniforms in first pass. */
      static struct state_tracker_uniform tracker_info[GFX_MAX_VARIABLES];
      static unsigned cnt = 0;

      if (cg_data->active_idx == 1)
         cnt = state_tracker_get_uniform(cg_data->state_tracker, tracker_info,
               GFX_MAX_VARIABLES, frame_count);

      for (i = 0; i < cnt; i++)
      {
         struct uniform_info state_params[2];

         CGparameter param_v = cgGetNamedParameter(
               cg_data->prg[cg_data->active_idx].vprg, tracker_info[i].id);
         CGparameter param_f = cgGetNamedParameter(
               cg_data->prg[cg_data->active_idx].fprg, tracker_info[i].id);

         uniform_params[0].data          = param_v;
         uniform_params[0].type          = UNIFORM_1F;
         uniform_params[0].result.f.a    = tracker_info[i].value;

         uniform_params[1].data          = param_f;
         uniform_params[1].type          = UNIFORM_1F;
         uniform_params[1].result.f.a    = tracker_info[i].value;

         cg_uniform_set_parameter(&state_params[i]);
      }
   }
}

static void gl_cg_deinit_progs(void *data)
{
   unsigned i;
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;

   if (!cg_data)
      return;

   RARCH_LOG("CG: Destroying programs.\n");
   cgGLUnbindProgram(cg_data->cgFProf);
   cgGLUnbindProgram(cg_data->cgVProf);

   /* Programs may alias [0]. */
   for (i = 1; i < GFX_MAX_SHADERS; i++)
   {
      if (cg_data->prg[i].fprg && cg_data->prg[i].fprg != cg_data->prg[0].fprg)
         cgDestroyProgram(cg_data->prg[i].fprg);
      if (cg_data->prg[i].vprg && cg_data->prg[i].vprg != cg_data->prg[0].vprg)
         cgDestroyProgram(cg_data->prg[i].vprg);
   }

   if (cg_data->prg[0].fprg)
      cgDestroyProgram(cg_data->prg[0].fprg);
   if (cg_data->prg[0].vprg)
      cgDestroyProgram(cg_data->prg[0].vprg);

   memset(cg_data->prg, 0, sizeof(cg_data->prg));
}

static void gl_cg_destroy_resources(void *data)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (!cg_data)
      return;

   gl_cg_reset_attrib(data);

   gl_cg_deinit_progs(data);

   if (cg_data->shader && cg_data->shader->luts)
   {
      glDeleteTextures(cg_data->shader->luts, cg_data->lut_textures);
      memset(cg_data->lut_textures, 0, sizeof(cg_data->lut_textures));
   }

   if (cg_data->state_tracker)
   {
      state_tracker_free(cg_data->state_tracker);
      cg_data->state_tracker = NULL;
   }

   free(cg_data->shader);
   cg_data->shader = NULL;
}

/* Final deinit. */
static void gl_cg_deinit_context_state(void *data)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (cg_data->cgCtx)
   {
      RARCH_LOG("CG: Destroying context.\n");
      cgDestroyContext(cg_data->cgCtx);
   }
   cg_data->cgCtx = NULL;
}

/* Full deinit. */
static void gl_cg_deinit(void *data)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (!cg_data)
      return;

   gl_cg_destroy_resources(cg_data);
   gl_cg_deinit_context_state(cg_data);

   free(cg_data);
}

#define CG_GL_SET_LISTING(cg_data, type) \
{ \
   const char *list = cgGetLastListing(cg_data->cgCtx); \
   if (list) \
      listing_##type = strdup(list); \
}

static bool gl_cg_load_program(void *data, unsigned idx,
      const char *prog, bool path_is_file)
{
   const char *argv[2 + GFX_MAX_SHADERS];
   bool ret         = true;
   char *listing_f  = NULL;
   char *listing_v  = NULL;
   unsigned i, argc = 0;
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;

   argv[argc++] = "-DPARAMETER_UNIFORM";
   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      if (*(cg_data->cg_alias_define[i]))
         argv[argc++] = cg_data->cg_alias_define[i];
   }
   argv[argc] = NULL;

   if (path_is_file)
   {
      cg_data->prg[idx].fprg = cgCreateProgramFromFile(
            cg_data->cgCtx, CG_SOURCE,
            prog, cg_data->cgFProf, "main_fragment", argv);
      CG_GL_SET_LISTING(cg_data, f);
      cg_data->prg[idx].vprg = cgCreateProgramFromFile(
            cg_data->cgCtx, CG_SOURCE,
            prog, cg_data->cgVProf, "main_vertex", argv);
      CG_GL_SET_LISTING(cg_data, v);
   }
   else
   {
      cg_data->prg[idx].fprg = cgCreateProgram(cg_data->cgCtx, CG_SOURCE,
            prog, cg_data->cgFProf, "main_fragment", argv);
      CG_GL_SET_LISTING(cg_data, f);
      cg_data->prg[idx].vprg = cgCreateProgram(cg_data->cgCtx, CG_SOURCE,
            prog, cg_data->cgVProf, "main_vertex", argv);
      CG_GL_SET_LISTING(cg_data, v);
   }

   if (!cg_data->prg[idx].fprg || !cg_data->prg[idx].vprg)
   {
      RARCH_ERR("CG error: %s\n", cgGetErrorString(cgGetError()));
      if (listing_f)
         RARCH_ERR("Fragment:\n%s\n", listing_f);
      else if (listing_v)
         RARCH_ERR("Vertex:\n%s\n", listing_v);

      ret = false;
      goto end;
   }

   cgGLLoadProgram(cg_data->prg[idx].fprg);
   cgGLLoadProgram(cg_data->prg[idx].vprg);

end:
   free(listing_f);
   free(listing_v);
   return ret;
}

static void gl_cg_set_program_base_attrib(void *data, unsigned i)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   CGparameter         param = cgGetFirstParameter(
         cg_data->prg[i].vprg, CG_PROGRAM);

   for (; param; param = cgGetNextParameter(param))
   {
      uint32_t semantic_hash;
      const char *semantic = NULL;
      if (cgGetParameterDirection(param) != CG_IN 
            || cgGetParameterVariability(param) != CG_VARYING)
         continue;

      semantic = cgGetParameterSemantic(param);
      if (!semantic)
         continue;

      RARCH_LOG("CG: Found semantic \"%s\" in prog #%u.\n", semantic, i);

      semantic_hash = djb2_calculate(semantic);

      switch (semantic_hash)
      {
         case SEMANTIC_TEXCOORD:
         case SEMANTIC_TEXCOORD0:
            cg_data->prg[i].tex     = param;
            break;
         case SEMANTIC_COLOR:
         case SEMANTIC_COLOR0:
            cg_data->prg[i].color   = param;
            break;
         case SEMANTIC_POSITION:
            cg_data->prg[i].vertex  = param;
            break;
         case SEMANTIC_TEXCOORD1:
            cg_data->prg[i].lut_tex = param;
            break;
      }
   }

   if (!cg_data->prg[i].tex)
      cg_data->prg[i].tex = cgGetNamedParameter  (cg_data->prg[i].vprg, "IN.tex_coord");
   if (!cg_data->prg[i].color)
      cg_data->prg[i].color = cgGetNamedParameter(cg_data->prg[i].vprg, "IN.color");
   if (!cg_data->prg[i].vertex)
      cg_data->prg[i].vertex = cgGetNamedParameter   (cg_data->prg[i].vprg, "IN.vertex_coord");
   if (!cg_data->prg[i].lut_tex)
      cg_data->prg[i].lut_tex = cgGetNamedParameter  (cg_data->prg[i].vprg, "IN.lut_tex_coord");
}

static bool gl_cg_load_stock(void *data)
{
   if (!gl_cg_load_program(data, 0, stock_cg_gl_program, false))
   {
      RARCH_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
      return false;
   }

   gl_cg_set_program_base_attrib(data, 0);

   return true;
}

static bool gl_cg_load_plain(void *data, const char *path)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (!gl_cg_load_stock(cg_data))
      return false;

   cg_data->shader = (struct video_shader*)calloc(1, sizeof(*cg_data->shader));
   if (!cg_data->shader)
      return false;

   cg_data->shader->passes = 1;

   if (path)
   {
      RARCH_LOG("Loading Cg file: %s\n", path);
      strlcpy(cg_data->shader->pass[0].source.path, path,
            sizeof(cg_data->shader->pass[0].source.path));
      if (!gl_cg_load_program(data, 1, path, true))
         return false;
   }
   else
   {
      RARCH_LOG("Loading stock Cg file.\n");
      cg_data->prg[1] = cg_data->prg[0];
   }

   video_shader_resolve_parameters(NULL, cg_data->shader);
   return true;
}

static bool gl_cg_load_imports(void *data)
{
   unsigned i;
   retro_ctx_memory_info_t mem_info;
   struct state_tracker_info tracker_info = {0};
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;

   if (!cg_data->shader->variables)
      return true;

   for (i = 0; i < cg_data->shader->variables; i++)
   {
      unsigned memtype;

      switch (cg_data->shader->variable[i].ram_type)
      {
         case RARCH_STATE_WRAM:
            memtype = RETRO_MEMORY_SYSTEM_RAM;
            break;

         default:
            memtype = -1u;
      }

      mem_info.id = memtype;

      core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

      if ((memtype != -1u) && 
            (cg_data->shader->variable[i].addr >= mem_info.size))
      {
         RARCH_ERR("Address out of bounds.\n");
         return false;
      }
   }

   mem_info.data = NULL;
   mem_info.size = 0;
   mem_info.id   = RETRO_MEMORY_SYSTEM_RAM;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   tracker_info.wram      = (uint8_t*)mem_info.data;
   tracker_info.info      = cg_data->shader->variable;
   tracker_info.info_elem = cg_data->shader->variables;

#ifdef HAVE_PYTHON
   if (*cg_data->shader->script_path)
   {
      tracker_info.script = cg_data->shader->script_path;
      tracker_info.script_is_file = true;
   }

   tracker_info.script_class = 
      *cg_data->shader->script_class ? cg_data->shader->script_class : NULL;
#endif

   cg_data->state_tracker = state_tracker_init(&tracker_info);
   if (!cg_data->state_tracker)
      RARCH_WARN("Failed to initialize state tracker.\n");

   return true;
}

static bool gl_cg_load_shader(void *data, unsigned i)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;

   RARCH_LOG("Loading Cg shader: \"%s\".\n",
         cg_data->shader->pass[i].source.path);

   if (!gl_cg_load_program(data, i + 1,
            cg_data->shader->pass[i].source.path, true))
      return false;

   return true;
}

static bool gl_cg_load_preset(void *data, const char *path)
{
   unsigned i;
   config_file_t *conf = NULL;
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;

   if (!gl_cg_load_stock(cg_data))
      return false;

   RARCH_LOG("Loading Cg meta-shader: %s\n", path);
   conf = config_file_new(path);
   if (!conf)
   {
      RARCH_ERR("Failed to load preset.\n");
      return false;
   }

   cg_data->shader = (struct video_shader*)calloc(1, sizeof(*cg_data->shader));
   if (!cg_data->shader)
   {
      config_file_free(conf);
      return false;
   }

   if (!video_shader_read_conf_cgp(conf, cg_data->shader))
   {
      RARCH_ERR("Failed to parse CGP file.\n");
      config_file_free(conf);
      return false;
   }

   video_shader_resolve_relative(cg_data->shader, path);
   video_shader_resolve_parameters(conf, cg_data->shader);
   config_file_free(conf);

   if (cg_data->shader->passes > GFX_MAX_SHADERS - 3)
   {
      RARCH_WARN("Too many shaders ... Capping shader amount to %d.\n",
            GFX_MAX_SHADERS - 3);
      cg_data->shader->passes = GFX_MAX_SHADERS - 3;
   }

   for (i = 0; i < cg_data->shader->passes; i++)
      if (*cg_data->shader->pass[i].alias)
         snprintf(cg_data->cg_alias_define[i],
               sizeof(cg_data->cg_alias_define[i]),
               "-D%s_ALIAS",
               cg_data->shader->pass[i].alias);

   for (i = 0; i < cg_data->shader->passes; i++)
   {
      if (!gl_cg_load_shader(cg_data, i))
      {
         RARCH_ERR("Failed to load shaders ...\n");
         return false;
      }
   }

   if (!gl_load_luts(cg_data->shader, cg_data->lut_textures))
   {
      RARCH_ERR("Failed to load lookup textures ...\n");
      return false;
   }

   if (!gl_cg_load_imports(cg_data))
   {
      RARCH_ERR("Failed to load imports ...\n");
      return false;
   }

   return true;
}

static void gl_cg_set_pass_attrib(
      struct cg_program *program,
      struct cg_fbo_params *fbo,
      const char *attr)
{
   char attr_buf[64] = {0};

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

static void gl_cg_set_program_attributes(void *data, unsigned i)
{
   unsigned j;
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;

   if (!cg_data)
      return;

   cgGLBindProgram(cg_data->prg[i].fprg);
   cgGLBindProgram(cg_data->prg[i].vprg);

   gl_cg_set_program_base_attrib(cg_data, i);

   cg_data->prg[i].vid_size_f = cgGetNamedParameter (cg_data->prg[i].fprg, "IN.video_size");
   cg_data->prg[i].tex_size_f = cgGetNamedParameter (cg_data->prg[i].fprg, "IN.texture_size");
   cg_data->prg[i].out_size_f = cgGetNamedParameter (cg_data->prg[i].fprg, "IN.output_size");
   cg_data->prg[i].frame_cnt_f = cgGetNamedParameter(cg_data->prg[i].fprg, "IN.frame_count");
   cg_data->prg[i].frame_dir_f = cgGetNamedParameter(cg_data->prg[i].fprg, "IN.frame_direction");
   cg_data->prg[i].vid_size_v = cgGetNamedParameter (cg_data->prg[i].vprg, "IN.video_size");
   cg_data->prg[i].tex_size_v = cgGetNamedParameter (cg_data->prg[i].vprg, "IN.texture_size");
   cg_data->prg[i].out_size_v = cgGetNamedParameter (cg_data->prg[i].vprg, "IN.output_size");
   cg_data->prg[i].frame_cnt_v = cgGetNamedParameter(cg_data->prg[i].vprg, "IN.frame_count");
   cg_data->prg[i].frame_dir_v = cgGetNamedParameter(cg_data->prg[i].vprg, "IN.frame_direction");

   cg_data->prg[i].mvp = cgGetNamedParameter(cg_data->prg[i].vprg, "modelViewProj");
   if (!cg_data->prg[i].mvp)
      cg_data->prg[i].mvp = cgGetNamedParameter(cg_data->prg[i].vprg, "IN.mvp_matrix");

   cg_data->prg[i].orig.tex = cgGetNamedParameter(cg_data->prg[i].fprg, "ORIG.texture");
   cg_data->prg[i].orig.vid_size_v = cgGetNamedParameter(cg_data->prg[i].vprg, "ORIG.video_size");
   cg_data->prg[i].orig.vid_size_f = cgGetNamedParameter(cg_data->prg[i].fprg, "ORIG.video_size");
   cg_data->prg[i].orig.tex_size_v = cgGetNamedParameter(cg_data->prg[i].vprg, "ORIG.texture_size");
   cg_data->prg[i].orig.tex_size_f = cgGetNamedParameter(cg_data->prg[i].fprg, "ORIG.texture_size");
   cg_data->prg[i].orig.coord = cgGetNamedParameter(cg_data->prg[i].vprg, "ORIG.tex_coord");

   cg_data->prg[i].feedback.tex = cgGetNamedParameter(cg_data->prg[i].fprg, "FEEDBACK.texture");
   cg_data->prg[i].feedback.vid_size_v = cgGetNamedParameter(cg_data->prg[i].vprg, "FEEDBACK.video_size");
   cg_data->prg[i].feedback.vid_size_f = cgGetNamedParameter(cg_data->prg[i].fprg, "FEEDBACK.video_size");
   cg_data->prg[i].feedback.tex_size_v = cgGetNamedParameter(cg_data->prg[i].vprg, "FEEDBACK.texture_size");
   cg_data->prg[i].feedback.tex_size_f = cgGetNamedParameter(cg_data->prg[i].fprg, "FEEDBACK.texture_size");
   cg_data->prg[i].feedback.coord = cgGetNamedParameter(cg_data->prg[i].vprg, "FEEDBACK.tex_coord");

   if (i > 1)
   {
      char pass_str[64] = {0};

      snprintf(pass_str, sizeof(pass_str), "PASSPREV%u", i);
      gl_cg_set_pass_attrib(&cg_data->prg[i], &cg_data->prg[i].orig, pass_str);
   }

   for (j = 0; j < PREV_TEXTURES; j++)
   {
      char attr_buf_tex[64]      = {0};
      char attr_buf_vid_size[64] = {0};
      char attr_buf_tex_size[64] = {0};
      char attr_buf_coord[64]    = {0};
      static const char *prev_names[PREV_TEXTURES] = {
         "PREV",
         "PREV1",
         "PREV2",
         "PREV3",
         "PREV4",
         "PREV5",
         "PREV6",
      };

      snprintf(attr_buf_tex,      sizeof(attr_buf_tex),     
            "%s.texture", prev_names[j]);
      snprintf(attr_buf_vid_size, sizeof(attr_buf_vid_size),
            "%s.video_size", prev_names[j]);
      snprintf(attr_buf_tex_size, sizeof(attr_buf_tex_size),
            "%s.texture_size", prev_names[j]);
      snprintf(attr_buf_coord,    sizeof(attr_buf_coord),
            "%s.tex_coord", prev_names[j]);

      cg_data->prg[i].prev[j].tex = cgGetNamedParameter(cg_data->prg[i].fprg,
            attr_buf_tex);

      cg_data->prg[i].prev[j].vid_size_v = 
         cgGetNamedParameter(cg_data->prg[i].vprg, attr_buf_vid_size);
      cg_data->prg[i].prev[j].vid_size_f = 
         cgGetNamedParameter(cg_data->prg[i].fprg, attr_buf_vid_size);

      cg_data->prg[i].prev[j].tex_size_v = 
         cgGetNamedParameter(cg_data->prg[i].vprg, attr_buf_tex_size);
      cg_data->prg[i].prev[j].tex_size_f = 
         cgGetNamedParameter(cg_data->prg[i].fprg, attr_buf_tex_size);

      cg_data->prg[i].prev[j].coord = cgGetNamedParameter(cg_data->prg[i].vprg,
            attr_buf_coord);
   }

   for (j = 0; j + 1 < i; j++)
   {
      char pass_str[64] = {0};

      snprintf(pass_str, sizeof(pass_str), "PASS%u", j + 1);
      gl_cg_set_pass_attrib(&cg_data->prg[i], &cg_data->prg[i].fbo[j], pass_str);
      snprintf(pass_str, sizeof(pass_str), "PASSPREV%u", i - (j + 1));
      gl_cg_set_pass_attrib(&cg_data->prg[i], &cg_data->prg[i].fbo[j], pass_str);

      if (*cg_data->shader->pass[j].alias)
         gl_cg_set_pass_attrib(&cg_data->prg[i], &cg_data->prg[i].fbo[j],
               cg_data->shader->pass[j].alias);
   }
}

static void *gl_cg_init(void *data, const char *path)
{
   unsigned i;
   cg_shader_data_t *cg_data = (cg_shader_data_t*)
      calloc(1, sizeof(cg_shader_data_t));

   if (!cg_data)
      return NULL;

#ifdef HAVE_CG_RUNTIME_COMPILER
   cgRTCgcInit();
#endif

   cg_data->cgCtx = cgCreateContext();

   if (!cg_data->cgCtx)
   {
      RARCH_ERR("Failed to create Cg context.\n");
      goto error;
   }

#ifdef RARCH_CG_DEBUG
   cgGLSetDebugMode(CG_TRUE);
   cgSetErrorHandler(cg_error_handler, NULL);
#endif

   cg_data->cgFProf = cgGLGetLatestProfile(CG_GL_FRAGMENT);
   cg_data->cgVProf = cgGLGetLatestProfile(CG_GL_VERTEX);

   if (
         cg_data->cgFProf == CG_PROFILE_UNKNOWN ||
         cg_data->cgVProf == CG_PROFILE_UNKNOWN)
   {
      RARCH_ERR("Invalid profile type\n");
      free(cg_data);
      cg_data = NULL;
      goto error;
   }

   RARCH_LOG("[Cg]: Vertex profile: %s\n",   cgGetProfileString(cg_data->cgVProf));
   RARCH_LOG("[Cg]: Fragment profile: %s\n", cgGetProfileString(cg_data->cgFProf));
   cgGLSetOptimalOptions(cg_data->cgFProf);
   cgGLSetOptimalOptions(cg_data->cgVProf);
   cgGLEnableProfile(cg_data->cgFProf);
   cgGLEnableProfile(cg_data->cgVProf);

   memset(cg_data->cg_alias_define, 0, sizeof(cg_data->cg_alias_define));

   if (path && string_is_equal(path_get_extension(path), "cgp"))
   {
      if (!gl_cg_load_preset(cg_data, path))
         goto error;
   }
   else
   {
      if (!gl_cg_load_plain(cg_data, path))
         goto error;
   }

   cg_data->prg[0].mvp = cgGetNamedParameter(
         cg_data->prg[0].vprg, "IN.mvp_matrix");

   for (i = 1; i <= cg_data->shader->passes; i++)
      gl_cg_set_program_attributes(cg_data, i);

   /* If we aren't using last pass non-FBO shader, 
    * this shader will be assumed to be "fixed-function".
    *
    * Just use prg[0] for that pass, which will be
    * pass-through. */
   cg_data->prg[cg_data->shader->passes + 1] = cg_data->prg[0]; 

   /* No need to apply Android hack in Cg. */
   cg_data->prg[GL_SHADER_STOCK_BLEND] = cg_data->prg[0];

   cgGLBindProgram(cg_data->prg[1].fprg);
   cgGLBindProgram(cg_data->prg[1].vprg);

   return cg_data;

error:
   gl_cg_destroy_resources(cg_data);
   if (!cg_data)
      free(cg_data);
   return NULL;
}

static void gl_cg_use(void *data, void *shader_data, unsigned idx)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)shader_data;
   if (cg_data && cg_data->prg[idx].vprg && cg_data->prg[idx].fprg)
   {
      gl_cg_reset_attrib(cg_data);

      cg_data->active_idx = idx;
      cgGLBindProgram(cg_data->prg[idx].vprg);
      cgGLBindProgram(cg_data->prg[idx].fprg);
   }
}

static unsigned gl_cg_num(void *data)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (!cg_data)
      return 0;
   return cg_data->shader->passes;
}

static bool gl_cg_filter_type(void *data, unsigned idx, bool *smooth)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (cg_data && idx &&
         (cg_data->shader->pass[idx - 1].filter != RARCH_FILTER_UNSPEC)
      )
   {
      *smooth = (cg_data->shader->pass[idx - 1].filter == RARCH_FILTER_LINEAR);
      return true;
   }

   return false;
}

static enum gfx_wrap_type gl_cg_wrap_type(void *data, unsigned idx)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (cg_data && idx)
      return cg_data->shader->pass[idx - 1].wrap;
   return RARCH_WRAP_BORDER;
}

static void gl_cg_shader_scale(void *data, unsigned idx, struct gfx_fbo_scale *scale)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (cg_data && idx)
      *scale = cg_data->shader->pass[idx - 1].fbo;
   else
      scale->valid = false;
}

static unsigned gl_cg_get_prev_textures(void *data)
{
   unsigned i, j;
   unsigned max_prev = 0;
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;

   if (!cg_data)
      return 0;

   for (i = 1; i <= cg_data->shader->passes; i++)
      for (j = 0; j < PREV_TEXTURES; j++)
         if (cg_data->prg[i].prev[j].tex)
            max_prev = MAX(j + 1, max_prev);

   return max_prev;
}

static bool gl_cg_get_feedback_pass(void *data, unsigned *pass)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (!cg_data || cg_data->shader->feedback_pass < 0)
      return false;

   *pass = cg_data->shader->feedback_pass;
   return true;
}

static bool gl_cg_mipmap_input(void *data, unsigned idx)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (cg_data && idx)
      return cg_data->shader->pass[idx - 1].mipmap;
   return false;
}

static struct video_shader *gl_cg_get_current_shader(void *data)
{
   cg_shader_data_t *cg_data = (cg_shader_data_t*)data;
   if (!cg_data)
      return NULL;
   return cg_data->shader;
}

const shader_backend_t gl_cg_backend = {
   gl_cg_init,
   gl_cg_deinit,
   gl_cg_set_params,
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

   RARCH_SHADER_CG,
   "gl_cg"
};

