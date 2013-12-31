/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "shader_hlsl.h"
#include "shader_parse.h"
#ifdef _XBOX
#include <xtl.h>
#endif

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

struct hlsl_program
{
   LPDIRECT3DVERTEXSHADER vprg;
   LPDIRECT3DPIXELSHADER fprg;
   D3DXHANDLE	vid_size_f;
   D3DXHANDLE	tex_size_f;
   D3DXHANDLE	out_size_f;
   D3DXHANDLE   frame_cnt_f;
   D3DXHANDLE   frame_dir_f;
   D3DXHANDLE	vid_size_v;
   D3DXHANDLE	tex_size_v;
   D3DXHANDLE	out_size_v;
   D3DXHANDLE   frame_cnt_v;
   D3DXHANDLE   frame_dir_v;
   D3DXHANDLE   mvp;
   LPD3DXCONSTANTTABLE v_ctable;
   LPD3DXCONSTANTTABLE f_ctable;
   XMMATRIX mvp_val;   /* TODO: Move to D3DXMATRIX here */
};

static LPDIRECT3DDEVICE d3d_device_ptr;
static struct hlsl_program prg[RARCH_HLSL_MAX_SHADERS] = {0};
static bool hlsl_active = false;
static unsigned active_index = 0;

static struct gfx_shader *cg_shader;

void hlsl_set_proj_matrix(XMMATRIX rotation_value)
{
   if (hlsl_active)
      prg[active_index].mvp_val = rotation_value;
}

#define set_param_2f(param, xy, constanttable) \
   if (param) constanttable->SetFloatArray(d3d_device_ptr, param, xy, 2)
#define set_param_1f(param, x, constanttable) \
   if (param) constanttable->SetFloat(d3d_device_ptr, param, x)

static void hlsl_set_params(unsigned width, unsigned height,
      unsigned tex_width, unsigned tex_height,
      unsigned out_width, unsigned out_height,
      unsigned frame_counter,
      const struct gl_tex_info *info,
      const struct gl_tex_info *prev_info,
      const struct gl_tex_info *fbo_info, unsigned fbo_info_cnt)
{
   if (!hlsl_active)
      return;

   const float ori_size[2] = { (float)width,     (float)height     };
   const float tex_size[2] = { (float)tex_width, (float)tex_height };
   const float out_size[2] = { (float)out_width, (float)out_height };
   float frame_cnt = frame_counter;

   prg[active_index].f_ctable->SetDefaults(d3d_device_ptr);
   prg[active_index].v_ctable->SetDefaults(d3d_device_ptr);

   set_param_2f(prg[active_index].vid_size_f, ori_size, prg[active_index].f_ctable);
   set_param_2f(prg[active_index].tex_size_f, tex_size, prg[active_index].f_ctable);
   set_param_2f(prg[active_index].out_size_f, out_size, prg[active_index].f_ctable);
   set_param_1f(prg[active_index].frame_cnt_f, frame_cnt, prg[active_index].f_ctable);
   set_param_1f(prg[active_index].frame_dir_f, g_extern.frame_is_reverse ? -1.0 : 1.0,prg[active_index].f_ctable);

   set_param_2f(prg[active_index].vid_size_v, ori_size, prg[active_index].v_ctable);
   set_param_2f(prg[active_index].tex_size_v, tex_size, prg[active_index].v_ctable);
   set_param_2f(prg[active_index].out_size_v, out_size, prg[active_index].v_ctable);
   set_param_1f(prg[active_index].frame_cnt_v, frame_cnt, prg[active_index].v_ctable);
   set_param_1f(prg[active_index].frame_dir_v, g_extern.frame_is_reverse ? -1.0 : 1.0,prg[active_index].v_ctable);


   /* TODO - set lookup textures/FBO textures/state parameters/etc */
}

static bool load_program(unsigned index, const char *prog, bool path_is_file)
{
   HRESULT ret, ret_fp, ret_vp;
   ID3DXBuffer *listing_f = NULL;
   ID3DXBuffer *listing_v = NULL;
   ID3DXBuffer *code_f = NULL;
   ID3DXBuffer *code_v = NULL;

   if (path_is_file)
   {
      ret_fp = D3DXCompileShaderFromFile(prog, NULL, NULL,
            "main_fragment", "ps_3_0", 0, &code_f, &listing_f, &prg[index].f_ctable); 
      ret_vp = D3DXCompileShaderFromFile(prog, NULL, NULL,
            "main_vertex", "vs_3_0", 0, &code_v, &listing_v, &prg[index].v_ctable); 
   }
   else
   {
      /* TODO - crashes currently - to do with 'end of line' of stock shader */
      ret_fp = D3DXCompileShader(prog, strlen(prog), NULL, NULL,
            "main_fragment", "ps_3_0", 0, &code_f, &listing_f, &prg[index].f_ctable );
      ret_vp = D3DXCompileShader(prog, strlen(prog), NULL, NULL,
            "main_vertex", "vs_3_0", 0, &code_v, &listing_v, &prg[index].v_ctable );
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

   d3d_device_ptr->CreatePixelShader((const DWORD*)code_f->GetBufferPointer(), &prg[index].fprg);
   d3d_device_ptr->CreateVertexShader((const DWORD*)code_v->GetBufferPointer(), &prg[index].vprg);
   code_f->Release();
   code_v->Release();

end:
   if (listing_f)
      listing_f->Release();
   if (listing_v)
      listing_v->Release();
   return ret;
}

static bool load_stock(void)
{
   if (!load_program(0, stock_hlsl_program, false))
   {
      RARCH_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
      return false;
   }

   return true;
}

static void set_program_attributes(unsigned i)
{
   prg[i].vid_size_f  = prg[i].f_ctable->GetConstantByName(NULL, "$IN.video_size");
   prg[i].tex_size_f  = prg[i].f_ctable->GetConstantByName(NULL, "$IN.texture_size");
   prg[i].out_size_f  = prg[i].f_ctable->GetConstantByName(NULL, "$IN.output_size");
   prg[i].frame_cnt_f = prg[i].f_ctable->GetConstantByName(NULL, "$IN.frame_count");
   prg[i].frame_dir_f = prg[i].f_ctable->GetConstantByName(NULL, "$IN.frame_direction");
   prg[i].vid_size_v  = prg[i].v_ctable->GetConstantByName(NULL, "$IN.video_size");
   prg[i].tex_size_v  = prg[i].v_ctable->GetConstantByName(NULL, "$IN.texture_size");
   prg[i].out_size_v  = prg[i].v_ctable->GetConstantByName(NULL, "$IN.output_size");
   prg[i].frame_cnt_v = prg[i].v_ctable->GetConstantByName(NULL, "$IN.frame_count");
   prg[i].frame_dir_v = prg[i].v_ctable->GetConstantByName(NULL, "$IN.frame_direction");
   prg[i].mvp         = prg[i].v_ctable->GetConstantByName(NULL, "$modelViewProj");
   prg[i].mvp_val     = XMMatrixIdentity();
}

static bool load_shader(const char *cgp_path, unsigned i)
{
   char path_buf[PATH_MAX];
   fill_pathname_resolve_relative(path_buf, cgp_path,
      cg_shader->pass[i].source.cg, sizeof(path_buf));

   RARCH_LOG("Loading Cg/HLSL shader: \"%s\".\n", path_buf);

   if (!load_program(i + 1, path_buf, true))
      return false;

   return true;
}

static bool load_plain(const char *path)
{
   if (!load_stock())
      return false;

   cg_shader = (struct gfx_shader*)calloc(1, sizeof(*cg_shader));
   if (!cg_shader)
      return false;

   cg_shader->passes = 1;

   if (path && path[0] != '\0')
   {
      RARCH_LOG("Loading Cg/HLSL file: %s\n", path);
      strlcpy(cg_shader->pass[0].source.cg, path, sizeof(cg_shader->pass[0].source.cg));
      if (!load_program(1, path, true))
         return false;
   }
   else
   {
      RARCH_LOG("Loading stock Cg/HLSL file.\n");
      prg[1] = prg[0];
   }

   return true;
}

static void hlsl_deinit_progs(void)
{
   for (unsigned i = 1; i < RARCH_HLSL_MAX_SHADERS; i++)
   {
      if (prg[i].fprg && prg[i].fprg != prg[0].fprg)
         prg[i].fprg->Release();
      if (prg[i].vprg && prg[i].vprg != prg[0].vprg)
         prg[i].vprg->Release();

      prg[i].fprg = NULL;
      prg[i].vprg = NULL;
   }

   if (prg[0].fprg)
      prg[0].fprg->Release();
   if (prg[0].vprg)
      prg[0].vprg->Release();

   prg[0].fprg = NULL;
   prg[0].vprg = NULL;
}

static void hlsl_deinit_state(void)
{
   hlsl_active = false;
   hlsl_deinit_progs();
   memset(prg, 0, sizeof(prg));

   d3d_device_ptr = NULL;

   free(cg_shader);
   cg_shader = NULL;
}

static bool load_preset(const char *path)
{
   if (!load_stock())
      return false;

   RARCH_LOG("Loading Cg meta-shader: %s\n", path);
   config_file_t *conf = config_file_new(path);

   if (!conf)
   {
      RARCH_ERR("Failed to load preset.\n");
      return false;
   }

   if (!cg_shader)
      cg_shader = (struct gfx_shader*)calloc(1, sizeof(*cg_shader));
   if (!cg_shader)
      return false;

   if (!gfx_shader_read_conf_cgp(conf, cg_shader))
   {
      RARCH_ERR("Failed to parse CGP file.\n");
      config_file_free(conf);
      return false;
   }

   config_file_free(conf);

   if (cg_shader->passes > RARCH_HLSL_MAX_SHADERS - 3)
   {
      RARCH_WARN("Too many shaders ... Capping shader amount to %d.\n", RARCH_HLSL_MAX_SHADERS - 3);
      cg_shader->passes = RARCH_HLSL_MAX_SHADERS - 3;
   }
   for (unsigned i = 0; i < cg_shader->passes; i++)
   {
      if (!load_shader(path, i))
      {
         RARCH_ERR("Failed to load shaders ...\n");
         return false;
      }
   }

   /* TODO - textures / imports */

   return true;
}

static bool hlsl_init(const char *path)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if (path && strcmp(path_get_extension(path), ".cgp") == 0)
   {
      if (!load_preset(path))
         return false;
   }
   else
   {
      if (!load_plain(path))
         return false;
   }

   for(unsigned i = 1; i <= cg_shader->passes; i++)
      set_program_attributes(i);

   d3d_device_ptr = d3d->d3d_render_device;
   d3d->d3d_render_device->SetVertexShader(prg[1].vprg);
   d3d->d3d_render_device->SetPixelShader(prg[1].fprg);

   hlsl_active = true;
   return true;
}

// Full deinit.
static void hlsl_deinit(void)
{
   if (!hlsl_active)
      return;

   hlsl_deinit_state();
}

static void hlsl_use(unsigned index)
{
   if (hlsl_active && prg[index].vprg && prg[index].fprg)
   {
      active_index = index;
#ifdef _XBOX
      D3DDevice_SetVertexShader(d3d_device_ptr, prg[index].vprg);
      D3DDevice_SetPixelShader(d3d_device_ptr, prg[index].fprg);
#else
      d3d_device_ptr->SetVertexShader(prg[index].vprg);
      d3d_device_ptr->SetPixelShader(prg[index].fprg);
#endif
   }
}

static unsigned hlsl_num(void)
{
   if (hlsl_active)
      return cg_shader->passes;
   else
      return 0;
}

static bool hlsl_filter_type(unsigned index, bool *smooth)
{
   if (hlsl_active && index)
   {
      if (cg_shader->pass[index - 1].filter == RARCH_FILTER_UNSPEC)
         return false;
      *smooth = cg_shader->pass[index - 1].filter = RARCH_FILTER_LINEAR;
      return true;
   }
   else
      return false;
}

static void hlsl_shader_scale(unsigned index, struct gfx_fbo_scale *scale)
{
   if (hlsl_active && index)
      *scale = cg_shader->pass[index - 1].fbo;
   else
      scale->valid = false;
}

static bool hlsl_set_mvp(const math_matrix *mat)
{
   /* TODO: Move to D3DXMATRIX here */
   if(hlsl_active && prg[active_index].mvp)
   {
      prg[active_index].v_ctable->SetMatrix(d3d_device_ptr, prg[active_index].mvp, (D3DXMATRIX*)&prg[active_index].mvp_val);
      return true;
   }
   else
      return false;
}

const gl_shader_backend_t hlsl_backend = {
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

   RARCH_SHADER_HLSL,
};
