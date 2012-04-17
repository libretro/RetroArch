/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "shader_hlsl.h"
#ifdef _XBOX
#include <xtl.h>
#endif

struct hlsl_program
{
   IDirect3DVertexShader9 *vprg;
   IDirect3DPixelShader9 *fprg;
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

static IDirect3DDevice9 *d3d_device_ptr;
static struct hlsl_program prg[SSNES_HLSL_MAX_SHADERS] = {0};
static bool hlsl_active = false;
static unsigned active_index = 0;

static const char *stock_hlsl_program =
      "void main_vertex                                                "
      "(                                                               "
      "   float2 position : POSITION,                                  "
      "   float2 texCoord : TEXCOORD0,                                 "
      "   uniform float4x4 modelViewProj : register(c0),               "
      "   out float4 oPosition : POSITION,                             "
      "   out float2 otexCoord : TEXCOORD                              "
      ")                                                               "
      "{                                                               "
      "   oPosition = mul(modelViewProj, float4(position, 0.0, 1.0));  "
      "   otexCoord = texCoord;                                        "
      "}                                                               "
      "                                                                "
      "struct output                                                   "
      "{                                                               "
      "   float4 color: COLOR;                                         "
      "};                                                              "
      "                                                                "
      "struct input                                                    "
      "{                                                               "
      "   float2 video_size;                                           "
      "   float2 texture_size;                                         "
      "   float2 output_size;                                          "
      "};                                                              "
      "                                                                "
      "output main_fragment(float2 texCoord : TEXCOORD0,               " 
      "uniform sampler2D decal : register(s0), uniform input IN)       "
      "{                                                               "
      "   output OUT;                                                  "
      "   OUT.color = tex2D(decal, tex);                               "
      "   return OUT;                                                  "
      "}                                                               ";

void hlsl_set_proj_matrix(XMMATRIX rotation_value)
{
   if (hlsl_active)
      prg[active_index].mvp_val = rotation_value;
}

#define set_param_2f(param, xy, constanttable) \
   if (param) constanttable->SetFloatArray(d3d_device_ptr, param, xy, 2)
#define set_param_1f(param, x, constanttable) \
   if (param) constanttable->SetFloat(d3d_device_ptr, param, x)

void hlsl_set_params(unsigned width, unsigned height,
      unsigned tex_width, unsigned tex_height,
      unsigned out_width, unsigned out_height,
      unsigned frame_count)
{
   if (!hlsl_active)
      return;

   const float ori_size[2] = { (float)width,     (float)height     };
   const float tex_size[2] = { (float)tex_width, (float)tex_height };
   const float out_size[2] = { (float)out_width, (float)out_height };

   set_param_2f(prg[active_index].vid_size_f, ori_size, prg[active_index].f_ctable);
   set_param_2f(prg[active_index].tex_size_f, tex_size, prg[active_index].f_ctable);
   set_param_2f(prg[active_index].out_size_f, out_size, prg[active_index].f_ctable);
   set_param_1f(prg[active_index].frame_cnt_f, (float)frame_count, prg[active_index].f_ctable);
   set_param_1f(prg[active_index].frame_dir_f, g_extern.frame_is_reverse ? -1.0 : 1.0,prg[active_index].f_ctable);

   set_param_2f(prg[active_index].vid_size_v, ori_size, prg[active_index].v_ctable);
   set_param_2f(prg[active_index].tex_size_v, tex_size, prg[active_index].v_ctable);
   set_param_2f(prg[active_index].out_size_v, out_size, prg[active_index].v_ctable);
   set_param_1f(prg[active_index].frame_cnt_v, (float)frame_count, prg[active_index].v_ctable);
   set_param_1f(prg[active_index].frame_dir_v, g_extern.frame_is_reverse ? -1.0 : 1.0,prg[active_index].v_ctable);

   /* TODO: Move to D3DXMATRIX here */
   prg[active_index].v_ctable->SetMatrix(d3d_device_ptr, prg[active_index].mvp, (D3DXMATRIX*)&prg[active_index].mvp_val);
}

static bool load_program(unsigned index, const char *prog, bool path_is_file)
{
   bool ret, ret_fp, ret_vp;
   ID3DXBuffer *listing_f = NULL;
   ID3DXBuffer *listing_v = NULL;
   ID3DXBuffer *code_f = NULL;
   ID3DXBuffer *code_v = NULL;

   ret = true;
   ret_fp = false;
   ret_vp = false;

   if (prg[index].f_ctable)
      prg[index].f_ctable->Release();
   if (prg[index].v_ctable)
      prg[index].v_ctable->Release();

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
      ret_fp = D3DXCompileShader(prog, (UINT)strlen(prog), NULL, NULL,
            "main_fragment", "ps_3_0", 0, &code_f, &listing_f, &prg[index].f_ctable );
      ret_vp = D3DXCompileShader(prog, (UINT)strlen(prog), NULL, NULL,
            "main_vertex", "vs_3_0", 0, &code_v, &listing_v, &prg[index].v_ctable );
   }

   if (FAILED(ret_fp) || FAILED(ret_vp) || listing_v || listing_f)
   {
      SSNES_ERR("HLSL error:\n");
      if(listing_f)
         SSNES_ERR("Fragment:\n%s\n", (char*)listing_f->GetBufferPointer());
      if(listing_v)
         SSNES_ERR("Vertex:\n%s\n", (char*)listing_v->GetBufferPointer());

      ret = false;
      goto end;
   }

   if (prg[index].fprg)
	   prg[index].fprg->Release();
   if (prg[index].vprg)
	   prg[index].vprg->Release();

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
      SSNES_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
      return false;
   }

   return true;
}

static bool load_plain(const char *path)
{
#if 0
   if (!load_stock())
      return false;
#endif

   SSNES_LOG("Loading HLSL file: %s\n", path);

   if (!load_program(0, path, true))
      return false;

   return true;
}

static void hlsl_deinit_progs(void)
{
   for(int i = 0; i < SSNES_HLSL_MAX_SHADERS; i++)
   {
      if (prg[i].fprg)
         prg[i].fprg->Release();
      if (prg[i].vprg)
         prg[i].vprg->Release();
   }
}

static void hlsl_deinit_state(void)
{
   hlsl_active = false;

   d3d_device_ptr = NULL;

   hlsl_deinit_progs();
}

static bool load_preset(const char *path)
{
   return false;
}

static void set_program_attributes(unsigned i)
{
   SSNES_LOG("Fragment: IN.video_size.\n");
   prg[i].vid_size_f  = prg[i].f_ctable->GetConstantByName(NULL, "$IN.video_size");
   SSNES_LOG("Fragment: IN.texture_size.\n");
   prg[i].tex_size_f  = prg[i].f_ctable->GetConstantByName(NULL, "$IN.texture_size");
   SSNES_LOG("Fragment: IN.output_size.\n");
   prg[i].out_size_f  = prg[i].f_ctable->GetConstantByName(NULL, "$IN.output_size");
   SSNES_LOG("Fragment: IN.frame_count.\n");
   prg[i].frame_cnt_f = prg[i].f_ctable->GetConstantByName(NULL, "$IN.frame_count");
   SSNES_LOG("Fragment: IN.frame_direction.\n");
   prg[i].frame_dir_f = prg[i].f_ctable->GetConstantByName(NULL, "$IN.frame_direction");
   SSNES_LOG("Vertex: IN.video_size.\n");
   prg[i].vid_size_v  = prg[i].v_ctable->GetConstantByName(NULL, "$IN.video_size");
   SSNES_LOG("Vertex: IN.texture_size.\n");
   prg[i].tex_size_v  = prg[i].v_ctable->GetConstantByName(NULL, "$IN.texture_size");
   SSNES_LOG("Vertex: IN.output_size.\n");
   prg[i].out_size_v  = prg[i].v_ctable->GetConstantByName(NULL, "$IN.output_size");
   SSNES_LOG("Vertex: IN.frame_count.\n");
   prg[i].frame_cnt_v = prg[i].v_ctable->GetConstantByName(NULL, "$IN.frame_count");
   SSNES_LOG("Vertex: IN.frame_direction.\n");
   prg[i].frame_dir_v = prg[i].v_ctable->GetConstantByName(NULL, "$IN.frame_direction");
   SSNES_LOG("Vertex: modelViewProj.\n");
   prg[i].mvp         = prg[i].v_ctable->GetConstantByName(NULL, "$modelViewProj");
}

bool hlsl_init(const char *path, IDirect3DDevice9 * device_ptr)
{
   if (!device_ptr)
      return false;

   d3d_device_ptr = device_ptr;

   if (strstr(path, ".cgp"))
   {
      if (!load_preset(path))
         return false;
   }
   else
   {
      if (!load_plain(path))
         return false;
   }

   set_program_attributes(0);

   active_index = 0;
   hlsl_active = true;
   return true;
}

void hlsl_use(unsigned index)
{
   if (hlsl_active && prg[index].vprg && prg[index].fprg)
   {
      active_index = index;
	  d3d_device_ptr->SetVertexShader(prg[index].vprg);
	  d3d_device_ptr->SetPixelShader(prg[index].fprg);
   }
}

// Full deinit.
void hlsl_deinit(void)
{
   if (!hlsl_active)
      return;

   hlsl_deinit_state();
}

