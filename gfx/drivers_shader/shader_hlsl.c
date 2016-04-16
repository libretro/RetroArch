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

#include <string/stdstring.h>

#include "shader_hlsl.h"

#include "../video_shader_parse.h"
#include "../d3d/d3d.h"
#include "../../rewind.h"

static const char *stock_hlsl_program =
      "void main_vertex\n"
      "(\n"
      "  float4 position : POSITION,\n"
      "  float4 color    : COLOR,\n"
      "\n"
      "  uniform float4x4 modelViewProj,\n"
      "\n"
      "  float4 texCoord : TEXCOORD0,\n"
      "  out float4 oPosition : POSITION,\n"
      "  out float4 oColor : COLOR,\n"
      "  out float2 otexCoord : TEXCOORD\n"
      ")\n"
      "{\n"
      "  oPosition = mul(modelViewProj, position);\n"
	   "  oColor = color;\n"
      "  otexCoord = texCoord;\n"
      "}\n"
      "\n"
      "struct output\n"
      "{\n"
      "  float4 color: COLOR;\n"
      "};\n"
      "\n"
      "struct input\n"
      "{\n"
      "  float2 video_size;\n"
      "  float2 texture_size;\n"
      "  float2 output_size;\n"
	   "  float frame_count;\n"
	   "  float frame_direction;\n"
	   "  float frame_rotation;\n"
      "};\n"
      "\n"
      "output main_fragment(float2 texCoord : TEXCOORD0,\n" 
      "uniform sampler2D decal : TEXUNIT0, uniform input IN)\n"
      "{\n"
      "  output OUT;\n"
      "  OUT.color = tex2D(decal, texCoord);\n"
      "  return OUT;\n"
      "}\n";

struct shader_program_data 
{
   LPDIRECT3DVERTEXSHADER vprg;
   LPDIRECT3DPIXELSHADER fprg;

   D3DXHANDLE	   vid_size_f;
   D3DXHANDLE	   tex_size_f;
   D3DXHANDLE	   out_size_f;
   D3DXHANDLE     frame_cnt_f;
   D3DXHANDLE     frame_dir_f;
   D3DXHANDLE	   vid_size_v;
   D3DXHANDLE	   tex_size_v;
   D3DXHANDLE	   out_size_v;
   D3DXHANDLE     frame_cnt_v;
   D3DXHANDLE     frame_dir_v;
   D3DXHANDLE     mvp;

   LPD3DXCONSTANTTABLE v_ctable;
   LPD3DXCONSTANTTABLE f_ctable;
   XMMATRIX mvp_val;   /* TODO: Move to D3DXMATRIX here */
};

typedef struct hlsl_shader_data hlsl_shader_data_t;

struct hlsl_shader_data
{
   d3d_video_t *d3d;
   shader_program_data_t prg[RARCH_HLSL_MAX_SHADERS];
   unsigned active_idx;
   struct video_shader *cg_shader;
};

void hlsl_set_proj_matrix(void *data, XMMATRIX rotation_value)
{
   hlsl_shader_data_t *hlsl = (hlsl_shader_data_t*)data;
   if (hlsl_data)
      hlsl_data->prg[hlsl_data->active_idx].mvp_val = rotation_value;
}

static void hlsl_uniform_set_parameter(void *data, shader_program_data_t *shader_data, void *uniform_data)
{
   struct uniform_info *param = (struct uniform_info*)data;

   if (!param || !param->enabled)
      return;

   switch (param->type)
   {
      case UNIFORM_1F:
         /* Unimplemented - Cg limitation */
         break;
      case UNIFORM_2F:
         /* Unimplemented - Cg limitation */
         break;
      case UNIFORM_3F:
         /* Unimplemented - Cg limitation */
         break;
      case UNIFORM_4F:
         /* Unimplemented - Cg limitation */
         break;
      case UNIFORM_1FV:
         /* Unimplemented - Cg limitation */
         break;
      case UNIFORM_2FV:
         /* Unimplemented - Cg limitation */
         break;
      case UNIFORM_3FV:
         /* Unimplemented - Cg limitation */
         break;
      case UNIFORM_4FV:
         /* Unimplemented - Cg limitation */
         break;
      case UNIFORM_1I:
         /* Unimplemented - Cg limitation */
         break;
   }
}

#define set_param_2f(param, xy, constanttable) \
   if (param) constanttable->SetFloatArray(d3d_device_ptr, param, xy, 2)
#define set_param_1f(param, x, constanttable) \
   if (param) constanttable->SetFloat(d3d_device_ptr, param, x)

static void hlsl_set_params(void *data, void *shader_data,
      unsigned width, unsigned height,
      unsigned tex_width, unsigned tex_height,
      unsigned out_width, unsigned out_height,
      unsigned frame_counter,
      const void *_info,
      const void *_prev_info,
      const void *_feedback_info,
      const void *_fbo_info, unsigned fbo_info_cnt)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3d_device_ptr = (LPDIRECT3DDEVICE)d3d->dev;
   const struct gfx_tex_info *info = (const struct gfx_tex_info*)_info;
   const struct gfx_tex_info *prev_info = (const struct gfx_tex_info*)_prev_info;
   (void)_feedback_info;
   const struct gfx_tex_info *fbo_info = (const struct gfx_tex_info*)_fbo_info;
   hlsl_shader_data_t *hlsl = (hlsl_shader_data_t*)shader_data;

   if (!hlsl)
      return;

   const float ori_size[2] = { (float)width,     (float)height     };
   const float tex_size[2] = { (float)tex_width, (float)tex_height };
   const float out_size[2] = { (float)out_width, (float)out_height };
   float frame_cnt = frame_counter;

   hlsl->prg[hlsl->active_idx].f_ctable->SetDefaults(d3d_device_ptr);
   hlsl->prg[hlsl->active_idx].v_ctable->SetDefaults(d3d_device_ptr);

   set_param_2f(hlsl->prg[hlsl->active_idx].vid_size_f, ori_size, hlsl->prg[hlsl->active_idx].f_ctable);
   set_param_2f(hlsl->prg[hlsl->active_idx].tex_size_f, tex_size, hlsl->prg[hlsl->active_idx].f_ctable);
   set_param_2f(hlsl->prg[hlsl->active_idx].out_size_f, out_size, hlsl->prg[hlsl->active_idx].f_ctable);
   set_param_1f(hlsl->prg[hlsl->active_idx].frame_cnt_f, frame_cnt, hlsl->prg[hlsl->active_idx].f_ctable);
   set_param_1f(hlsl->prg[hlsl->active_idx].frame_dir_f, state_manager_frame_is_reversed() ? -1.0 : 1.0, hlsl->prg[hlsl->active_idx].f_ctable);

   set_param_2f(hlsl->prg[hlsl->active_idx].vid_size_v, ori_size, hlsl->prg[hlsl->active_idx].v_ctable);
   set_param_2f(hlsl->prg[hlsl->active_idx].tex_size_v, tex_size, hlsl->prg[hlsl->active_idx].v_ctable);
   set_param_2f(hlsl->prg[hlsl->active_idx].out_size_v, out_size, hlsl->prg[hlsl->active_idx].v_ctable);
   set_param_1f(hlsl->prg[hlsl->active_idx].frame_cnt_v, frame_cnt, hlsl->prg[hlsl->active_idx].v_ctable);
   set_param_1f(hlsl->prg[hlsl->active_idx].frame_dir_v, state_manager_frame_is_reversed() ? -1.0 : 1.0, hlsl->prg[hlsl->active_idx].v_ctable);

   /* TODO - set lookup textures/FBO textures/state parameters/etc */
}

static bool hlsl_compile_program(
      void *data,
      unsigned idx,
      void *program_data,
      struct shader_program_info *program_info)
{
   hlsl_shader_data_t *hlsl = (hlsl_shader_data_t*)data;
   d3d_video_t *d3d = (d3d_video_t*)hlsl->d3d;
   shader_program_data_t *program  = (shader_program_data_t*)program_data;
   LPDIRECT3DDEVICE d3d_device_ptr = (LPDIRECT3DDEVICE)d3d->dev;
   HRESULT ret, ret_fp, ret_vp;
   ID3DXBuffer *listing_f = NULL;
   ID3DXBuffer *listing_v = NULL;
   ID3DXBuffer *code_f = NULL;
   ID3DXBuffer *code_v = NULL;

   if (!program)
      program = &hlsl->prg[idx];

   if (program_info->is_file)
   {
      ret_fp = D3DXCompileShaderFromFile(program_info->combined, NULL, NULL,
            "main_fragment", "ps_3_0", 0, &code_f, &listing_f, &program->f_ctable); 
      ret_vp = D3DXCompileShaderFromFile(program_info->combined, NULL, NULL,
            "main_vertex", "vs_3_0", 0, &code_v, &listing_v, &program->v_ctable); 
   }
   else
   {
      /* TODO - crashes currently - to do with 'end of line' of stock shader */
      ret_fp = D3DXCompileShader(program_info->combined, strlen(program_info->combined), NULL, NULL,
            "main_fragment", "ps_3_0", 0, &code_f, &listing_f, &program->f_ctable );
      ret_vp = D3DXCompileShader(program_info->combined, strlen(program_info->combined), NULL, NULL,
            "main_vertex", "vs_3_0", 0, &code_v, &listing_v, &program->v_ctable );
   }

   if (ret_fp < 0 || ret_vp < 0 || listing_v || listing_f)
   {
      RARCH_ERR("Cg/HLSL error:\n");
      if(listing_f)
         RARCH_ERR("Fragment:\n%s\n", (char*)listing_f->GetBufferPointer());
      if(listing_v)
         RARCH_ERR("Vertex:\n%s\n", (char*)listing_v->GetBufferPointer());

      ret = false;
      goto end;
   }

   d3d_device_ptr->CreatePixelShader((const DWORD*)code_f->GetBufferPointer(),  &program->fprg);
   d3d_device_ptr->CreateVertexShader((const DWORD*)code_v->GetBufferPointer(), &program->vprg);
   code_f->Release();
   code_v->Release();

end:
   if (listing_f)
      listing_f->Release();
   if (listing_v)
      listing_v->Release();
   return ret;
}

static bool hlsl_load_stock(hlsl_shader_data_t *hlsl, void *data)
{
   struct shader_program_info program_info;

   program_info.combined = stock_hlsl_program;
   program_info.is_file  = false;

   hlsl->d3d = (d3d_video_t*)data;

   if (!hlsl_compile_program(hlsl, 0, &hlsl->prg[0], &program_info))
   {
      RARCH_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
      return false;
   }

   return true;
}

static void hlsl_set_program_attributes(hlsl_shader_data_t *hlsl, unsigned i)
{
   if (!hlsl)
      return;

   hlsl->prg[i].vid_size_f  = hlsl->prg[i].f_ctable->GetConstantByName(NULL, "$IN.video_size");
   hlsl->prg[i].tex_size_f  = hlsl->prg[i].f_ctable->GetConstantByName(NULL, "$IN.texture_size");
   hlsl->prg[i].out_size_f  = hlsl->prg[i].f_ctable->GetConstantByName(NULL, "$IN.output_size");
   hlsl->prg[i].frame_cnt_f = hlsl->prg[i].f_ctable->GetConstantByName(NULL, "$IN.frame_count");
   hlsl->prg[i].frame_dir_f = hlsl->prg[i].f_ctable->GetConstantByName(NULL, "$IN.frame_direction");
   hlsl->prg[i].vid_size_v  = hlsl->prg[i].v_ctable->GetConstantByName(NULL, "$IN.video_size");
   hlsl->prg[i].tex_size_v  = hlsl->prg[i].v_ctable->GetConstantByName(NULL, "$IN.texture_size");
   hlsl->prg[i].out_size_v  = hlsl->prg[i].v_ctable->GetConstantByName(NULL, "$IN.output_size");
   hlsl->prg[i].frame_cnt_v = hlsl->prg[i].v_ctable->GetConstantByName(NULL, "$IN.frame_count");
   hlsl->prg[i].frame_dir_v = hlsl->prg[i].v_ctable->GetConstantByName(NULL, "$IN.frame_direction");
   hlsl->prg[i].mvp         = hlsl->prg[i].v_ctable->GetConstantByName(NULL, "$modelViewProj");
   hlsl->prg[i].mvp_val     = XMMatrixIdentity();
}

static bool hlsl_load_shader(hlsl_shader_data_t *hlsl,
	void *data, const char *cgp_path, unsigned i)
{
   struct shader_program_info program_info;
   char path_buf[PATH_MAX_LENGTH] = {0};

   program_info.combined = path_buf;
   program_info.is_file  = true;

   fill_pathname_resolve_relative(path_buf, cgp_path,
      hlsl->cg_shader->pass[i].source.path, sizeof(path_buf));

   RARCH_LOG("Loading Cg/HLSL shader: \"%s\".\n", path_buf);

   hlsl->d3d = (d3d_video_t*)data;

   if (!hlsl_compile_program(hlsl, data, i + 1, &hlsl->prg[i + 1], &program_info))
      return false;

   return true;
}

static bool hlsl_load_plain(hlsl_shader_data_t *hlsl, void *data, const char *path)
{
   if (!hlsl_load_stock(hlsl, data))
      return false;

   hlsl->cg_shader = (struct video_shader*)calloc(1, sizeof(*hlsl->cg_shader));
   if (!hlsl->cg_shader)
      return false;

   hlsl->cg_shader->passes = 1;

   if (!string_is_empty(path))
   {
      struct shader_program_info program_info;

      program_info.combined = path;
      program_info.is_file  = true;

      RARCH_LOG("Loading Cg/HLSL file: %s\n", path);

      strlcpy(hlsl->cg_shader->pass[0].source.path,
		  path, sizeof(hlsl->cg_shader->pass[0].source.path));

      hlsl->d3d = (d3d_video_t*)data;
      if (!hlsl_compile_program(hlsl, data, 1, &hlsl->prg[1], &progarm_info))
         return false;
   }
   else
   {
      RARCH_LOG("Loading stock Cg/HLSL file.\n");
      hlsl->prg[1] = hlsl->prg[0];
   }

   return true;
}

static void hlsl_deinit_progs(hlsl_shader_data_t *hlsl)
{
   unsigned i;
   for (i = 1; i < RARCH_HLSL_MAX_SHADERS; i++)
   {
      if (hlsl->prg[i].fprg && hlsl->prg[i].fprg != hlsl->prg[0].fprg)
         hlsl->prg[i].fprg->Release();
      if (hlsl->prg[i].vprg && hlsl->prg[i].vprg != hlsl->prg[0].vprg)
         hlsl->prg[i].vprg->Release();

	  hlsl->prg[i].fprg = NULL;
	  hlsl->prg[i].vprg = NULL;
   }

   if (hlsl->prg[0].fprg)
      hlsl->prg[0].fprg->Release();
   if (hlsl->prg[0].vprg)
      hlsl->prg[0].vprg->Release();

   hlsl->prg[0].fprg = NULL;
   hlsl->prg[0].vprg = NULL;
}

static void hlsl_deinit_state(hlsl_shader_data_t *hlsl)
{
   hlsl_deinit_progs(hlsl);
   memset(hlsl->prg, 0, sizeof(hlsl->prg));

   if (hlsl->cg_shader)
      free(hlsl->cg_shader);
   hlsl->cg_shader = NULL;
}

static bool hlsl_load_preset(hlsl_shader_data_t *hlsl, void *data, const char *path)
{
   unsigned i;
   config_file_t *conf = NULL;
   if (!hlsl_load_stock(hlsl, data))
      return false;

   RARCH_LOG("Loading Cg meta-shader: %s\n", path);

   conf = config_file_new(path);

   if (!conf)
      goto error;

   if (!hlsl->cg_shader)
      hlsl->cg_shader = (struct video_shader*)calloc(1, sizeof(*hlsl->cg_shader));
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
      RARCH_WARN("Too many shaders ... Capping shader amount to %d.\n", RARCH_HLSL_MAX_SHADERS - 3);
      hlsl->cg_shader->passes = RARCH_HLSL_MAX_SHADERS - 3;
   }

   for (i = 0; i < hlsl->cg_shader->passes; i++)
   {
      if (!hlsl_load_shader(hlsl, data, path, i))
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

static void *hlsl_init(void *data, const char *path)
{
   unsigned i;
   d3d_video_t *d3d = (d3d_video_t*)data;
   hlsl_shader_data_t *hlsl_data = (hlsl_shader_data_t*)
      calloc(1, sizeof(hlsl_shader_data_t));

   if (!hlsl_data)
	   return NULL;

   if (path && string_is_equal(path_get_extension(path), ".cgp"))
   {
      if (!hlsl_load_preset(hlsl_data, d3d, path))
         goto error;
   }
   else
   {
      if (!hlsl_load_plain(hlsl_data, d3d, path))
         goto error;
   }

   for(i = 1; i <= hlsl_data->cg_shader->passes; i++)
      hlsl_set_program_attributes(hlsl_data, i);

   d3d_set_vertex_shader(d3d->dev, 1, hlsl_data->prg[1].vprg);
   d3d->dev->SetPixelShader(hlsl_data->prg[1].fprg);

   return hlsl_data;

error:
   if (hlsl_data)
	   free(hlsl_data);
   return NULL;
}

static void hlsl_deinit(void *data)
{
   hlsl_shader_data_t *hlsl_data = (hlsl_shader_data_t*)data;

   hlsl_deinit_state(hlsl_data);

   if (hlsl_data)
      free(hlsl_data);
}

static void hlsl_use(void *data, void *shader_data, unsigned idx, bool set_active)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   hlsl_shader_data_t *hlsl_data = (hlsl_shader_data_t*)shader_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;

   if (hlsl_data && hlsl_data->prg[idx].vprg && hlsl_data->prg[idx].fprg)
   {
      if (set_active)
      {
         hlsl_data->active_idx = idx;
      }

      d3d_set_vertex_shader(d3dr, idx, hlsl_data->prg[idx].vprg);
#ifdef _XBOX
      D3DDevice_SetPixelShader(d3dr, hlsl_data->prg[idx].fprg);
#else
      d3dr->SetPixelShader(hlsl_data->prg[idx].fprg);
#endif
   }
}

static unsigned hlsl_num(void *data)
{
   hlsl_shader_data_t *hlsl_data = (hlsl_shader_data_t*)data;
   if (hlsl_data)
      return hlsl_data->cg_shader->passes;
   return 0;
}

static bool hlsl_filter_type(void *data, unsigned idx, bool *smooth)
{
   hlsl_shader_data_t *hlsl_data = (hlsl_shader_data_t*)data;
   if (hlsl_data && idx
         && (hlsl_data->cg_shader->pass[idx - 1].filter != RARCH_FILTER_UNSPEC))
   {
      *smooth = hlsl_data->cg_shader->pass[idx - 1].filter = RARCH_FILTER_LINEAR;
      return true;
   }
   return false;
}

static void hlsl_shader_scale(void *data, unsigned idx, struct gfx_fbo_scale *scale)
{
   hlsl_shader_data_t *hlsl_data = (hlsl_shader_data_t*)data;
   if (hlsl_data && idx)
      *scale = hlsl_data->cg_shader->pass[idx - 1].fbo;
   else
      scale->valid = false;
}

static bool hlsl_set_mvp(void *data, void *shader_data, const math_matrix_4x4 *mat)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3d_device_ptr = (LPDIRECT3DDEVICE)d3d->dev;
   hlsl_shader_data_t *hlsl_data = (hlsl_shader_data_t*)shader_data;

   if(hlsl_data && hlsl_data->prg[hlsl_data->active_idx].mvp)
   {
      hlsl_data->prg[hlsl_data->active_idx].v_ctable->SetMatrix(d3d_device_ptr,
		  hlsl_data->prg[hlsl_data->active_idx].mvp,
		  (D3DXMATRIX*)&hlsl_data->prg[hlsl_data->active_idx].mvp_val);
      return true;
   }
   return false;
}

static bool hlsl_mipmap_input(void *data, unsigned idx)
{
   (void)idx;
   return false;
}

static bool hlsl_get_feedback_pass(void *data, unsigned *idx)
{
   (void)idx;
   return false;
}

static struct video_shader *hlsl_get_current_shader(void *data)
{
   return NULL;
}

const shader_backend_t hlsl_backend = {
   hlsl_init,
   hlsl_deinit,
   hlsl_set_params,
   hlsl_use,
   hlsl_num,
   hlsl_filter_type,
   NULL,              /* hlsl_wrap_type  */
   hlsl_shader_scale,
   NULL,              /* hlsl_set_coords */
   hlsl_set_mvp,
   NULL,              /* hlsl_get_prev_textures */
   hlsl_get_feedback_pass,
   hlsl_mipmap_input,
   hlsl_get_current_shader,

   RARCH_SHADER_HLSL,
   "hlsl"
};
