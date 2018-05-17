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

#define CINTERFACE

#include <compat/strl.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <string.h>
#include <retro_inline.h>
#include <retro_math.h>

#include <d3d9.h>
#include <d3dx9shader.h>

#include "../drivers/d3d_shaders/opaque.hlsl.d3d9.h"

#include "../../defines/d3d_defines.h"
#include "../common/d3d_common.h"
#include "../common/d3d9_common.h"

#include "../video_driver.h"

#include "../video_shader_parse.h"
#include "../../managers/state_manager.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#define RARCH_HLSL_MAX_SHADERS 16

struct shader_program_hlsl_data
{
   LPDIRECT3DVERTEXSHADER9 vprg;
   LPDIRECT3DPIXELSHADER9 fprg;

   D3DXHANDLE     mvp;

   LPD3DXCONSTANTTABLE v_ctable;
   LPD3DXCONSTANTTABLE f_ctable;
   D3DXMATRIX mvp_val;
};

typedef struct hlsl_shader_data
{
   LPDIRECT3DDEVICE9 dev;
   struct shader_program_hlsl_data prg[RARCH_HLSL_MAX_SHADERS];
   unsigned active_idx;
   struct video_shader *cg_shader;
} hlsl_shader_data_t;

#include "d3d9_renderchain.h"

typedef struct hlsl_d3d9_renderchain
{
   struct d3d9_renderchain chain;
   hlsl_shader_data_t *shader_pipeline;
} hlsl_d3d9_renderchain_t;

static void *d3d9_hlsl_get_constant_by_name(void *data, const char *name)
{
   LPD3DXCONSTANTTABLE prog = (LPD3DXCONSTANTTABLE)data;
   char lbl[64];
   lbl[0] = '\0';
   snprintf(lbl, sizeof(lbl), "$%s", name);
   return d3d9x_constant_table_get_constant_by_name(prog, NULL, lbl);
}

static INLINE void d3d9_hlsl_set_param_2f(void *data, void *userdata, const char *name, const void *values)
{
   LPD3DXCONSTANTTABLE prog = (LPD3DXCONSTANTTABLE)data;
   D3DXHANDLE param         = d3d9_hlsl_get_constant_by_name(prog, name);
   if (param)
      d3d9x_constant_table_set_float_array((LPDIRECT3DDEVICE9)userdata, prog, (void*)param, values, 2);
}

static INLINE void d3d9_hlsl_set_param_1f(void *data, void *userdata, const char *name, const void *value)
{
   LPD3DXCONSTANTTABLE prog = (LPD3DXCONSTANTTABLE)data;
   D3DXHANDLE param         = d3d9_hlsl_get_constant_by_name(prog, name);
   float *val               = (float*)value;
   if (param)
      d3d9x_constant_table_set_float(prog, (LPDIRECT3DDEVICE9)userdata, (void*)param, *val);
}

static void hlsl_use(hlsl_shader_data_t *hlsl,
      LPDIRECT3DDEVICE9 d3dr,
      unsigned idx, bool set_active)
{
   struct shader_program_hlsl_data *program = &hlsl->prg[idx];

   if (!program || !program->vprg || !program->fprg)
      return;

   if (set_active)
      hlsl->active_idx           = idx;

   d3d9_set_vertex_shader(d3dr, idx, program->vprg);
   d3d9_set_pixel_shader(d3dr, program->fprg);
}

static bool hlsl_set_mvp(hlsl_shader_data_t *hlsl,
      d3d9_video_t *d3d,
      LPDIRECT3DDEVICE9 d3dr,
      const D3DMATRIX *mat)
{
   struct shader_program_hlsl_data *program = &hlsl->prg[hlsl->active_idx];

   if (!program || !program->mvp)
      return false;

   d3d9x_constant_table_set_matrix(d3dr, program->v_ctable, 
         (void*)program->mvp, &program->mvp_val);
   return true;
}

#if 0
void hlsl_set_proj_matrix(hlsl_shader_data_t *hlsl, void *matrix_data)
{
   const D3DMATRIX *matrix  = (const D3DMATRIX*)matrix_data;
   if (hlsl && matrix)
      hlsl->prg[hlsl->active_idx].mvp_val = *matrix;
}
#endif

static bool d3d9_hlsl_load_program(
      hlsl_shader_data_t *hlsl,
      unsigned idx,
      struct shader_program_hlsl_data *program,
      struct shader_program_info *program_info)
{
   LPDIRECT3DDEVICE9 d3dr                    = (LPDIRECT3DDEVICE9)hlsl->dev;
   ID3DXBuffer *listing_f                    = NULL;
   ID3DXBuffer *listing_v                    = NULL;
   ID3DXBuffer *code_f                       = NULL;
   ID3DXBuffer *code_v                       = NULL;

   if (!program)
      program = &hlsl->prg[idx];

   if (program_info->is_file)
   {
      if (!d3d9x_compile_shader_from_file(program_info->combined, NULL, NULL,
               "main_fragment", "ps_3_0", 0, &code_f, &listing_f, &program->f_ctable))
         goto error;
      if (!d3d9x_compile_shader_from_file(program_info->combined, NULL, NULL,
               "main_vertex", "vs_3_0", 0, &code_v, &listing_v, &program->v_ctable))
         goto error;
   }
   else
   {
      if (!d3d9x_compile_shader(program_info->combined,
               strlen(program_info->combined), NULL, NULL,
               "main_fragment", "ps_3_0", 0, &code_f, &listing_f,
               &program->f_ctable ))
      {
         RARCH_ERR("Failure building stock fragment shader..\n");
         goto error;
      }
      if (!d3d9x_compile_shader(program_info->combined,
               strlen(program_info->combined), NULL, NULL,
               "main_vertex", "vs_3_0", 0, &code_v, &listing_v,
               &program->v_ctable ))
      {
         RARCH_ERR("Failure building stock vertex shader..\n");
         goto error;
      }
   }

   d3d9_create_pixel_shader(d3dr,  (const DWORD*)d3d9x_get_buffer_ptr(code_f),  (void**)&program->fprg);
   d3d9_create_vertex_shader(d3dr, (const DWORD*)d3d9x_get_buffer_ptr(code_v), (void**)&program->vprg);
   d3d9x_buffer_release((void*)code_f);
   d3d9x_buffer_release((void*)code_v);

   return true;

error:
   RARCH_ERR("Cg/HLSL error:\n");
   if (listing_f)
      RARCH_ERR("Fragment:\n%s\n", (char*)d3d9x_get_buffer_ptr(listing_f));
   if (listing_v)
      RARCH_ERR("Vertex:\n%s\n", (char*)d3d9x_get_buffer_ptr(listing_v));
   d3d9x_buffer_release((void*)listing_f);
   d3d9x_buffer_release((void*)listing_v);

   return false;
}

static void hlsl_set_program_attributes(hlsl_shader_data_t *hlsl,
      unsigned i)
{
   struct shader_program_hlsl_data *program = &hlsl->prg[i];
   void *fprg              = NULL;
   void *vprg              = NULL;

   if (!program)
      return;

   vprg                    = program->v_ctable;

   if (vprg)
      program->mvp         = (D3DXHANDLE)d3d9_hlsl_get_constant_by_name(vprg, "modelViewProj");

   d3d_matrix_identity(&program->mvp_val);
}

static bool hlsl_load_shader(hlsl_shader_data_t *hlsl,
      const char *cgp_path, unsigned i)
{
   struct shader_program_info program_info;
   char path_buf[PATH_MAX_LENGTH];

   path_buf[0]           = '\0';

   program_info.combined = path_buf;
   program_info.is_file  = true;

   fill_pathname_resolve_relative(path_buf, cgp_path,
         hlsl->cg_shader->pass[i].source.path, sizeof(path_buf));

   RARCH_LOG("Loading Cg/HLSL shader: \"%s\".\n", path_buf);

   if (!d3d9_hlsl_load_program(hlsl, i + 1, &hlsl->prg[i + 1], &program_info))
      return false;

   return true;
}

static bool hlsl_load_plain(hlsl_shader_data_t *hlsl, const char *path)
{
   struct video_shader *cg_shader = (struct video_shader*)
      calloc(1, sizeof(*cg_shader));

   if (!cg_shader)
      return false;

   hlsl->cg_shader         = cg_shader;
   hlsl->cg_shader->passes = 1;

   if (!string_is_empty(path))
   {
      struct shader_program_info program_info;

      program_info.combined = path;
      program_info.is_file  = true;

      RARCH_LOG("Loading Cg/HLSL file: %s\n", path);

      strlcpy(hlsl->cg_shader->pass[0].source.path,
            path, sizeof(hlsl->cg_shader->pass[0].source.path));

      if (!d3d9_hlsl_load_program(hlsl, 1, &hlsl->prg[1], &program_info))
         return false;
   }
   else
   {
      RARCH_LOG("Loading stock Cg/HLSL file.\n");
      hlsl->prg[1] = hlsl->prg[0];
   }

   return true;
}

static bool hlsl_load_preset(hlsl_shader_data_t *hlsl, const char *path)
{
   unsigned i;
   config_file_t *conf = config_file_new(path);

   if (!conf)
      goto error;

   RARCH_LOG("Loaded Cg meta-shader: %s\n", path);

   if (!hlsl->cg_shader)
      hlsl->cg_shader = (struct video_shader*)calloc
         (1, sizeof(*hlsl->cg_shader));
   if (!hlsl->cg_shader)
      goto error;

   if (!video_shader_read_conf_cgp(conf, hlsl->cg_shader))
   {
      RARCH_ERR("Failed to parse CGP file.\n");
      goto error;
   }

   config_file_free(conf);

   if (hlsl->cg_shader->passes > RARCH_HLSL_MAX_SHADERS - 3)
   {
      RARCH_WARN("Too many shaders ... "
            "Capping shader amount to %d.\n", RARCH_HLSL_MAX_SHADERS - 3);
      hlsl->cg_shader->passes = RARCH_HLSL_MAX_SHADERS - 3;
   }

   for (i = 0; i < hlsl->cg_shader->passes; i++)
   {
      if (!hlsl_load_shader(hlsl, path, i))
         goto error;
   }

   /* TODO - textures / imports */
   return true;

error:
   RARCH_ERR("Failed to load preset.\n");
   if (conf)
      config_file_free(conf);
   conf = NULL;

   return false;
}

static hlsl_shader_data_t *hlsl_init(d3d9_video_t *d3d, const char *path)
{
   unsigned i;
   struct shader_program_info program_info;
   hlsl_shader_data_t *hlsl  = (hlsl_shader_data_t*)
      calloc(1, sizeof(hlsl_shader_data_t));

   if (!hlsl)
      goto error;

   hlsl->dev         = d3d->dev;

   if (!hlsl->dev)
      goto error;

   program_info.combined     = stock_hlsl_program;
   program_info.is_file      = false;

   /* Load stock shader */
   if (!d3d9_hlsl_load_program(hlsl, 0, &hlsl->prg[0], &program_info))
   {
      RARCH_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
      goto error;
   }

   if (path && (string_is_equal(path_get_extension(path), ".cgp")))
   {
      if (!hlsl_load_preset(hlsl, path))
         goto error;
   }
   else
   {
      if (!hlsl_load_plain(hlsl, path))
         goto error;
   }

   RARCH_LOG("Setting up program attributes...\n");
   RARCH_LOG("Shader passes: %d\n", hlsl->cg_shader->passes);

   for(i = 1; i <= hlsl->cg_shader->passes; i++)
      hlsl_set_program_attributes(hlsl, i);

   RARCH_LOG("Setting up vertex shader...\n");
   d3d9_set_vertex_shader(hlsl->dev, 1, hlsl->prg[1].vprg);
   RARCH_LOG("Setting up pixel shader...\n");
   d3d9_set_pixel_shader(hlsl->dev, hlsl->prg[1].fprg);

   return hlsl;

error:
   if (hlsl)
      free(hlsl);
   return NULL;
}

static void hlsl_set_params(hlsl_shader_data_t *hlsl,
      LPDIRECT3DDEVICE9 d3dr,
      video_shader_ctx_params_t *params)
{
   float ori_size[2], tex_size[2], out_size[2];
   void *data                               = params->data;
   unsigned width                           = params->width;
   unsigned height                          = params->height;
   unsigned tex_width                       = params->tex_width;
   unsigned tex_height                      = params->tex_height;
   unsigned out_width                       = params->out_width;
   unsigned out_height                      = params->out_height;
   unsigned frame_count                     = params->frame_counter;
   const void *_info                        = params->info;
   const void *_prev_info                   = params->prev_info;
   const void *_feedback_info               = params->feedback_info;
   const void *_fbo_info                    = params->fbo_info;
   unsigned fbo_info_cnt                    = params->fbo_info_cnt;
   float frame_cnt                          = frame_count;
   const struct video_tex_info *info        = (const struct video_tex_info*)_info;
   const struct video_tex_info *prev_info   = (const struct video_tex_info*)_prev_info;
   const struct video_tex_info *fbo_info    = (const struct video_tex_info*)_fbo_info;
   struct shader_program_hlsl_data *program = &hlsl->prg[hlsl->active_idx];
   void *fprg                               = NULL;
   void *vprg                               = NULL;
   float frame_dir                          = 1.0f;

   if (!program)
      return;

   fprg                                     = program->f_ctable;
   vprg                                     = program->v_ctable;

   ori_size[0]                              = (float)width;
   ori_size[1]                              = (float)height;
   tex_size[0]                              = (float)tex_width;
   tex_size[1]                              = (float)tex_height;
   out_size[0]                              = (float)out_width;
   out_size[1]                              = (float)out_height;

   d3d9x_constant_table_set_defaults(d3dr, fprg);
   d3d9x_constant_table_set_defaults(d3dr, vprg);

   if (state_manager_frame_is_reversed())
      frame_dir = -1.0f;

   d3d9_hlsl_set_param_2f(fprg, d3dr, "IN.video_size",      &ori_size);
   d3d9_hlsl_set_param_2f(fprg, d3dr, "IN.texture_size",    &tex_size);
   d3d9_hlsl_set_param_2f(fprg, d3dr, "IN.output_size",     &out_size);
   d3d9_hlsl_set_param_1f(fprg, d3dr, "IN.frame_count",     &frame_cnt);
   d3d9_hlsl_set_param_1f(fprg, d3dr, "IN.frame_direction", &frame_dir);

   d3d9_hlsl_set_param_2f(vprg, d3dr, "IN.video_size",      &ori_size);
   d3d9_hlsl_set_param_2f(vprg, d3dr, "IN.texture_size",    &tex_size);
   d3d9_hlsl_set_param_2f(vprg, d3dr, "IN.output_size",     &out_size);
   d3d9_hlsl_set_param_1f(vprg, d3dr, "IN.frame_count",     &frame_cnt);
   d3d9_hlsl_set_param_1f(vprg, d3dr, "IN.frame_direction", &frame_dir);

   /* TODO - set lookup textures/FBO textures/state parameters/etc */
}

static void hlsl_deinit_progs(hlsl_shader_data_t *hlsl)
{
   unsigned i;

   for (i = 1; i < RARCH_HLSL_MAX_SHADERS; i++)
   {
      if (hlsl->prg[i].fprg && hlsl->prg[i].fprg != hlsl->prg[0].fprg)
         d3d9_free_pixel_shader(hlsl->dev, hlsl->prg[i].fprg);
      if (hlsl->prg[i].vprg && hlsl->prg[i].vprg != hlsl->prg[0].vprg)
         d3d9_free_vertex_shader(hlsl->dev, hlsl->prg[i].vprg);

      hlsl->prg[i].fprg = NULL;
      hlsl->prg[i].vprg = NULL;
   }

   if (hlsl->prg[0].fprg)
      d3d9_free_pixel_shader(hlsl->dev, hlsl->prg[0].fprg);
   if (hlsl->prg[0].vprg)
      d3d9_free_vertex_shader(hlsl->dev, hlsl->prg[0].vprg);

   hlsl->prg[0].fprg = NULL;
   hlsl->prg[0].vprg = NULL;
}

static void hlsl_deinit(hlsl_shader_data_t *hlsl)
{
   if (!hlsl)
      return;

   hlsl_deinit_progs(hlsl);
   memset(hlsl->prg, 0, sizeof(hlsl->prg));

   if (hlsl->cg_shader)
      free(hlsl->cg_shader);
   hlsl->cg_shader = NULL;

   if (hlsl)
      free(hlsl);
}

static bool hlsl_d3d9_renderchain_init_shader_fvf(
      d3d9_renderchain_t *chain,
      struct shader_pass *pass)
{
   static const D3DVERTEXELEMENT9 decl[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      D3D9_DECL_FVF_TEXCOORD(0, 2, 0),
      D3DDECL_END()
   };

   return d3d9_vertex_declaration_new(chain->dev,
         decl, (void**)&pass->vertex_decl);
}

static bool hlsl_d3d9_renderchain_create_first_pass(
      LPDIRECT3DDEVICE9 dev,
      d3d9_renderchain_t *chain,
      const struct LinkInfo *info,
      unsigned _fmt)
{
   unsigned i;
   struct shader_pass pass;
   unsigned fmt = 
      (_fmt == RETRO_PIXEL_FORMAT_RGB565) ? 
      d3d9_get_rgb565_format() : d3d9_get_xrgb8888_format();

   pass.info        = *info;
   pass.last_width  = 0;
   pass.last_height = 0;
   pass.attrib_map  = (struct unsigned_vector_list*)
      unsigned_vector_list_new();

   chain->prev.ptr  = 0;

   for (i = 0; i < TEXTURES; i++)
   {
      chain->prev.last_width[i]  = 0;
      chain->prev.last_height[i] = 0;
      chain->prev.vertex_buf[i]  = (LPDIRECT3DVERTEXBUFFER9)
         d3d9_vertex_buffer_new(
            chain->dev, 4 * sizeof(struct D3D9Vertex),
            D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, NULL);

      if (!chain->prev.vertex_buf[i])
         return false;

      chain->prev.tex[i] = (LPDIRECT3DTEXTURE9)
         d3d9_texture_new(chain->dev, NULL,
            info->tex_w, info->tex_h, 1, 0, fmt,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!chain->prev.tex[i])
         return false;

      d3d9_set_texture(chain->dev, 0, chain->prev.tex[i]);
      d3d9_set_sampler_minfilter(dev, 0,
            d3d_translate_filter(info->pass->filter));
      d3d9_set_sampler_magfilter(dev, 0,
            d3d_translate_filter(info->pass->filter));
      d3d9_set_sampler_address_u(dev, 0, D3DTADDRESS_BORDER);
      d3d9_set_sampler_address_v(dev, 0, D3DTADDRESS_BORDER);
      d3d9_set_texture(chain->dev, 0, NULL);
   }

   pass.vertex_buf        = d3d9_vertex_buffer_new(
         dev, 4 * sizeof(struct D3D9Vertex),
         D3DUSAGE_WRITEONLY,
         0,
         D3DPOOL_MANAGED,
         NULL);

   if (!pass.vertex_buf)
      return false;

   pass.tex = d3d9_texture_new(dev, NULL,
         pass.info.tex_w, pass.info.tex_h, 1, 0, fmt,
         0, 0, 0, 0, NULL, NULL, false);

   if (!pass.tex)
      return false;

   d3d9_set_sampler_address_u(dev, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d9_set_sampler_address_v(dev, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   d3d9_set_render_state(dev, D3DRS_CULLMODE, D3DCULL_NONE);
   d3d9_set_render_state(dev, D3DRS_ZENABLE, FALSE);

   if (!hlsl_d3d9_renderchain_init_shader_fvf(chain, &pass))
      return false;
   shader_pass_vector_list_append(chain->passes, pass);

   return true;
}

static void d3d9_hlsl_renderchain_calc_and_set_shader_mvp(
      hlsl_d3d9_renderchain_t *chain,
      d3d9_video_t *d3d,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   struct d3d_matrix proj, ortho, rot, matrix;

   d3d_matrix_ortho_off_center_lh(&ortho, 0, vp_width, 0, vp_height, 0, 1);
   d3d_matrix_identity(&rot);
   d3d_matrix_rotation_z(&rot, rotation * (D3D_PI / 2.0));

   d3d_matrix_multiply(&proj, &ortho, &rot);
   d3d_matrix_transpose(&matrix, &proj);

   if (chain->shader_pipeline)
      hlsl_set_mvp(chain->shader_pipeline, d3d,
            chain->chain.dev,
            (const D3DMATRIX*)&matrix);
}


static void hlsl_d3d9_renderchain_set_vertices(
      d3d9_video_t *d3d,
      hlsl_d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned pass_count,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      uint64_t frame_count,
      unsigned rotation)
{
   video_shader_ctx_params_t params;

   if (pass->last_width != width || pass->last_height != height)
   {
      unsigned i;
      struct D3D9Vertex vert[4];
      void *verts        = NULL;
      float _u           = (width)  / pass->info.tex_w;
      float _v           = (height) / pass->info.tex_h;

      pass->last_width   = width;
      pass->last_height  = height;

      vert[0].x          = 0.0f;
      vert[0].y          = out_height;
      vert[0].z          = 0.5f;
      vert[0].u          = 0.0f;
      vert[0].v          = 0.0f;
      vert[0].lut_u      = 0.0f;
      vert[0].lut_v      = 0.0f;
      vert[0].r          = 1.0f;
      vert[0].g          = 1.0f;
      vert[0].b          = 1.0f;
      vert[0].a          = 1.0f;

      vert[1].x         = out_width;
      vert[1].y         = out_height;
      vert[1].z         = 0.5f;
      vert[1].u         = _u;
      vert[1].v         = 0.0f;
      vert[1].lut_u     = 1.0f;
      vert[1].lut_v     = 0.0f;
      vert[1].r         = 1.0f;
      vert[1].g         = 1.0f;
      vert[1].b         = 1.0f;
      vert[1].a         = 1.0f;

      vert[2].x         = 0.0f;
      vert[2].y         = 0.0f;
      vert[2].z         = 0.5f;
      vert[2].u         = 0.0f;
      vert[2].v         = _v;
      vert[2].lut_u     = 0.0f;
      vert[2].lut_v     = 1.0f;
      vert[2].r         = 1.0f;
      vert[2].g         = 1.0f;
      vert[2].b         = 1.0f;
      vert[2].a         = 1.0f;

      vert[3].x         = out_width;
      vert[3].y         = 0.0f;
      vert[3].z         = 0.5f;
      vert[3].u         = _u;
      vert[3].v         = _v;
      vert[3].lut_u     = 1.0f;
      vert[3].lut_v     = 1.0f;
      vert[3].r         = 1.0f;
      vert[3].g         = 1.0f;
      vert[3].b         = 1.0f;
      vert[3].a         = 1.0f;

#if 0
      /* Align texels and vertices. */
      for (i = 0; i < 4; i++)
      {
         vert[i].x      -= 0.5f;
         vert[i].y      += 0.5f;
      }
#endif

      verts = d3d9_vertex_buffer_lock(pass->vertex_buf);
      memcpy(verts, vert, sizeof(vert));
      d3d9_vertex_buffer_unlock(pass->vertex_buf);
   }

   if (chain->shader_pipeline)
      hlsl_use(chain->shader_pipeline, chain->chain.dev, pass_count, true);

   params.data          = d3d;
   params.width         = width;
   params.height        = height;
   params.tex_width     = pass->info.tex_w;
   params.tex_height    = pass->info.tex_h;
   params.out_width     = out_width;
   params.out_height    = out_height;
   params.frame_counter = (unsigned int)frame_count;
   params.info          = NULL;
   params.prev_info     = NULL;
   params.feedback_info = NULL;
   params.fbo_info      = NULL;
   params.fbo_info_cnt  = 0;

   d3d9_hlsl_renderchain_calc_and_set_shader_mvp(chain, d3d,
         /*pass->vPrg, */vp_width, vp_height, rotation);
   if (chain->shader_pipeline)
      hlsl_set_params(chain->shader_pipeline, chain->chain.dev, &params);
}

static void d3d9_hlsl_deinit_progs(hlsl_d3d9_renderchain_t *chain)
{
   RARCH_LOG("[D3D9 HLSL]: Destroying programs.\n");

   if (chain->chain.passes->count >= 1)
   {
      unsigned i;

      d3d9_vertex_buffer_free(NULL, chain->chain.passes->data[0].vertex_decl);

      for (i = 1; i < chain->chain.passes->count; i++)
      {
         if (chain->chain.passes->data[i].tex)
            d3d9_texture_free(chain->chain.passes->data[i].tex);
         chain->chain.passes->data[i].tex = NULL;
         d3d9_vertex_buffer_free(
               chain->chain.passes->data[i].vertex_buf,
               chain->chain.passes->data[i].vertex_decl);
      }
   }
}

static void d3d9_hlsl_destroy_resources(hlsl_d3d9_renderchain_t *chain)
{
   unsigned i;

   for (i = 0; i < TEXTURES; i++)
   {
      if (chain->chain.prev.tex[i])
         d3d9_texture_free(chain->chain.prev.tex[i]);
      if (chain->chain.prev.vertex_buf[i])
         d3d9_vertex_buffer_free(chain->chain.prev.vertex_buf[i], NULL);
   }

   d3d9_hlsl_deinit_progs(chain);

   for (i = 0; i < chain->chain.luts->count; i++)
   {
      if (chain->chain.luts->data[i].tex)
         d3d9_texture_free(chain->chain.luts->data[i].tex);
   }
}

static void hlsl_d3d9_renderchain_free(void *data)
{
   unsigned i;
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)data;

   if (!chain)
      return;

   hlsl_deinit(chain->shader_pipeline);
   d3d9_hlsl_destroy_resources(chain);
   d3d9_renderchain_destroy_passes_and_luts(&chain->chain);
   free(chain);
}

void *hlsl_d3d9_renderchain_new(void)
{
   hlsl_d3d9_renderchain_t *renderchain =
      (hlsl_d3d9_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   d3d9_init_renderchain(&renderchain->chain);

   return renderchain;
}

static bool hlsl_d3d9_renderchain_init_shader(d3d9_video_t *d3d,
      hlsl_d3d9_renderchain_t *chain)
{
   hlsl_shader_data_t *shader = hlsl_init(d3d, retroarch_get_shader_preset());
   if (!shader)
      return false;

   RARCH_LOG("[D3D9]: Using HLSL shader backend.\n");

   chain->shader_pipeline           = shader;

   return true;
}

static bool hlsl_d3d9_renderchain_init(
      d3d9_video_t *d3d,
      const video_info_t *video_info,
      LPDIRECT3DDEVICE9 dev,
      const D3DVIEWPORT9 *final_viewport,
      const struct LinkInfo *info,
      bool rgb32
      )
{
   hlsl_d3d9_renderchain_t *chain     = (hlsl_d3d9_renderchain_t*)
      d3d->renderchain_data;
   unsigned fmt                       = (rgb32)
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;

   if (!chain)
      return false;
   if (!hlsl_d3d9_renderchain_init_shader(d3d, chain))
   {
      RARCH_ERR("[D3D9 HLSL]: Failed to initialize shader subsystem.\n");
      return false;
   }

   chain->chain.dev                         = dev;
   chain->chain.final_viewport              = (D3DVIEWPORT9*)final_viewport;
   chain->chain.frame_count                 = 0;
   chain->chain.pixel_size                  = (fmt == RETRO_PIXEL_FORMAT_RGB565) ? 2 : 4;

   if (!hlsl_d3d9_renderchain_create_first_pass(dev, &chain->chain, info, fmt))
      return false;

   return true;
}

static void hlsl_d3d9_renderchain_set_final_viewport(
      d3d9_video_t *d3d,
      void *renderchain_data,
      const D3DVIEWPORT9 *final_viewport)
{
   hlsl_d3d9_renderchain_t *_chain = (hlsl_d3d9_renderchain_t*)renderchain_data;
   d3d9_renderchain_t      *chain  = (d3d9_renderchain_t*)&_chain->chain;

   if (chain && final_viewport)
      chain->final_viewport = (D3DVIEWPORT9*)final_viewport;

   d3d9_recompute_pass_sizes(chain->dev, chain, d3d);
}

static void hlsl_d3d9_renderchain_render_pass(
      hlsl_d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      state_tracker_t *tracker,
      unsigned pass_index)
{
   unsigned i;

#if 0
   cgD3D9BindProgram(pass->fPrg);
   cgD3D9BindProgram(pass->vPrg);
#endif

   d3d9_set_texture(chain->chain.dev, 0, pass->tex);
   d3d9_set_sampler_minfilter(chain->chain.dev, 0,
         d3d_translate_filter(pass->info.pass->filter));
   d3d9_set_sampler_magfilter(chain->chain.dev, 0,
         d3d_translate_filter(pass->info.pass->filter));

   d3d9_set_vertex_declaration(chain->chain.dev, pass->vertex_decl);
   for (i = 0; i < 4; i++)
      d3d9_set_stream_source(chain->chain.dev, i,
            pass->vertex_buf, 0,
            sizeof(struct D3D9Vertex));

#if 0
   /* Set orig texture. */
   d3d9_cg_renderchain_bind_orig(chain, pass);

   /* Set prev textures. */
   d3d9_cg_renderchain_bind_prev(chain, pass);

   /* Set lookup textures */
   for (i = 0; i < chain->chain.luts->count; i++)
   {
      CGparameter vparam;
      CGparameter fparam = cgGetNamedParameter(
            pass->fPrg, chain->chain.luts->data[i].id);
      int bound_index    = -1;

      if (fparam)
      {
         unsigned index  = cgGetParameterResourceIndex(fparam);
         bound_index     = index;

         d3d9_renderchain_add_lut_internal(&chain->chain, index, i);
      }

      vparam = cgGetNamedParameter(pass->vPrg, chain->chain.luts->data[i].id);

      if (vparam)
      {
         unsigned index = cgGetParameterResourceIndex(vparam);
         if (index != (unsigned)bound_index)
            d3d9_renderchain_add_lut_internal(&chain->chain, index, i);
      }
   }

   if (pass_index >= 3)
      d3d9_cg_renderchain_bind_pass(chain, pass, pass_index);

   if (tracker)
      cg_d3d9_renderchain_set_params(chain, pass, tracker, pass_index);
#endif

   d3d9_draw_primitive(chain->chain.dev, D3DPT_TRIANGLESTRIP, 0, 2);

   /* So we don't render with linear filter into render targets,
    * which apparently looked odd (too blurry). */
   d3d9_set_sampler_minfilter(chain->chain.dev, 0, D3DTEXF_POINT);
   d3d9_set_sampler_magfilter(chain->chain.dev, 0, D3DTEXF_POINT);

   d3d9_renderchain_unbind_all(&chain->chain);
}

static bool hlsl_d3d9_renderchain_render(
      d3d9_video_t *d3d,
      const video_frame_info_t *video_info,
      state_tracker_t *tracker,
      const void *frame,
      unsigned width, unsigned height,
      unsigned pitch, unsigned rotation)
{
   LPDIRECT3DSURFACE9 back_buffer, target;
   unsigned i, current_width, current_height, out_width = 0, out_height = 0;
   struct shader_pass *last_pass    = NULL;
   struct shader_pass *first_pass   = NULL;
   settings_t *settings           = config_get_ptr();
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)
      d3d->renderchain_data;

   d3d9_renderchain_start_render(&chain->chain);

   current_width                  = width;
   current_height                 = height;

   first_pass                     = (struct shader_pass*)
      &chain->chain.passes->data[0];

   d3d9_convert_geometry(
         &first_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->chain.final_viewport);

   d3d9_renderchain_blit_to_texture(first_pass->tex,
         frame,
         first_pass->info.tex_w,
         first_pass->info.tex_h,
         width,
         height,
         first_pass->last_width,
         first_pass->last_height,
         pitch,
         chain->chain.pixel_size);

   /* Grab back buffer. */
   d3d9_device_get_render_target(chain->chain.dev, 0, (void**)&back_buffer);

   /* In-between render target passes. */
   for (i = 0; i < chain->chain.passes->count - 1; i++)
   {
      D3DVIEWPORT9   viewport = {0};
      struct shader_pass *from_pass  = (struct shader_pass*)
         &chain->chain.passes->data[i];
      struct shader_pass *to_pass    = (struct shader_pass*)
         &chain->chain.passes->data[i + 1];

      d3d9_texture_get_surface_level(to_pass->tex, 0, (void**)&target);

      d3d9_device_set_render_target(chain->chain.dev, 0, (void*)target);

      d3d9_convert_geometry(&from_pass->info,
            &out_width, &out_height,
            current_width, current_height, chain->chain.final_viewport);

      /* Clear out whole FBO. */
      viewport.Width  = to_pass->info.tex_w;
      viewport.Height = to_pass->info.tex_h;
      viewport.MinZ   = 0.0f;
      viewport.MaxZ   = 1.0f;

      d3d9_set_viewports(chain->chain.dev, &viewport);
      d3d9_clear(chain->chain.dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);

      viewport.Width  = out_width;
      viewport.Height = out_height;

      d3d9_set_viewports(chain->chain.dev, &viewport);

      hlsl_d3d9_renderchain_set_vertices(d3d,
            chain, from_pass, i,
            current_width, current_height,
            out_width, out_height,
            out_width, out_height,
            chain->chain.frame_count, 0);

      hlsl_d3d9_renderchain_render_pass(chain, from_pass,
            tracker,
            i + 1);

      current_width = out_width;
      current_height = out_height;
      d3d9_surface_free(target);
   }

   /* Final pass */
   d3d9_device_set_render_target(chain->chain.dev, 0, (void*)back_buffer);

   last_pass = (struct shader_pass*)&chain->chain.passes->
      data[chain->chain.passes->count - 1];

   d3d9_convert_geometry(&last_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->chain.final_viewport);

   d3d9_set_viewports(chain->chain.dev, chain->chain.final_viewport);

   hlsl_d3d9_renderchain_set_vertices(d3d,
         chain, last_pass, chain->chain.passes->count - 1,
         current_width, current_height,
         out_width, out_height,
         chain->chain.final_viewport->Width,
         chain->chain.final_viewport->Height,
         chain->chain.frame_count, rotation);

   hlsl_d3d9_renderchain_render_pass(chain, last_pass,
         tracker,
         chain->chain.passes->count);

   chain->chain.frame_count++;

   d3d9_surface_free(back_buffer);

   d3d9_renderchain_end_render(&chain->chain);
#if 0
   cgD3D9BindProgram(chain->fStock);
   cgD3D9BindProgram(chain->vStock);
#endif
   d3d9_hlsl_renderchain_calc_and_set_shader_mvp(chain, d3d,
         /* chain->vStock, */ chain->chain.final_viewport->Width,
         chain->chain.final_viewport->Height, 0);

   return true;
}

static bool hlsl_d3d9_renderchain_add_pass(
      void *data, const struct LinkInfo *info)
{
   (void)data;

   /* stub */
   return true;
}

static bool hlsl_d3d9_renderchain_add_lut(void *data,
      const char *id, const char *path, bool smooth)
{
   hlsl_d3d9_renderchain_t *_chain  = (hlsl_d3d9_renderchain_t*)data;
   d3d9_renderchain_t *chain        = (d3d9_renderchain_t*)&_chain->chain;

   return d3d9_renderchain_add_lut(chain, id, path, smooth);
}

d3d9_renderchain_driver_t hlsl_d3d9_renderchain = {
   hlsl_d3d9_renderchain_free,
   hlsl_d3d9_renderchain_new,
   hlsl_d3d9_renderchain_init,
   hlsl_d3d9_renderchain_set_final_viewport,
   hlsl_d3d9_renderchain_add_pass,
   hlsl_d3d9_renderchain_add_lut,
   hlsl_d3d9_renderchain_render,
   "hlsl_d3d9",
};
