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
static struct hlsl_program prg[RARCH_HLSL_MAX_SHADERS] = {0};
static bool hlsl_active = false;
static unsigned active_index = 0;
static unsigned hlsl_shader_num = 0;

static const char *stock_hlsl_program =
      "void main_vertex                                                "
      "(                                                               "
      "   float4 position : POSITION,                                  "
	  "   float4 color    : COLOR,                                     "
      "   float4 texCoord : TEXCOORD0,                                 "
      "   uniform float4x4 modelViewProj,                              "
      "   out float4 oPosition : POSITION,                             "
	  "   out float4 oColor : COLOR,                                   "
      "   out float2 otexCoord : TEXCOORD                              "
      ")                                                               "
      "{                                                               "
      "   oPosition = mul(modelViewProj, position);                    "
	  "   oColor = color;                                              "
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
	  "   float frame_count;                                           "
	  "   float frame_direction;                                       "
	  "   float frame_rotation;                                        "
      "};                                                              "
      "                                                                "
      "output main_fragment(float2 texCoord : TEXCOORD0,               " 
      "uniform sampler2D decal : TEXUNIT0, uniform input IN)           "
      "{                                                               "
      "   output OUT;                                                  "
      "   OUT.color = tex2D(decal, texCoord);                          "
      "   return OUT;                                                  "
      "}                                                               ";

void hlsl_set_proj_matrix(XMMATRIX rotation_value)
{
   if (hlsl_active)
      prg[active_index].mvp_val = rotation_value;
}

unsigned d3d_hlsl_num(void)
{
   if (hlsl_active)
      return hlsl_shader_num;
   else
      return 0;
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
   float frame_cnt = frame_count;

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

   /* TODO: Move to D3DXMATRIX here */
   if(prg[active_index].mvp)
      prg[active_index].v_ctable->SetMatrix(d3d_device_ptr, prg[active_index].mvp, (D3DXMATRIX*)&prg[active_index].mvp_val);
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
      ret_fp = D3DXCompileShader(prog, (UINT)strlen(prog), NULL, NULL,
            "main_fragment", "ps_3_0", 0, &code_f, &listing_f, &prg[index].f_ctable );
      ret_vp = D3DXCompileShader(prog, (UINT)strlen(prog), NULL, NULL,
            "main_vertex", "vs_3_0", 0, &code_v, &listing_v, &prg[index].v_ctable );
   }

   if (ret_fp < 0 || ret_vp < 0 || listing_v || listing_f)
   {
      RARCH_ERR("HLSL error:\n");
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

bool hlsl_load_shader(unsigned index, const char *path)
{
   bool retval = true;

   if (!hlsl_active || index == 0)
      retval = false;

   // FIXME: This could cause corruption issues if prg[index] == prg[0]
   // (Set earlier if path == NULL).
   if(retval)
   {
      if (path)
      {
         if (load_program(index, path, true))
         {
            set_program_attributes(index);
         }
         else
         {
            // Always make sure we have a valid shader.
            prg[index] = prg[0];
		    retval = false;
         }
      }
      else
         prg[index] = prg[0];
   }
   else
      goto end; // if retval is false, skip to end label

   hlsl_active = true;
   active_index = index;
   d3d_device_ptr->SetVertexShader(prg[index].vprg);
   d3d_device_ptr->SetPixelShader(prg[index].fprg);
end:
   return retval;
}

static bool load_plain(const char *path)
{
   if (!load_stock())
      return false;

   RARCH_LOG("Loading HLSL file: %s\n", path);

   if (!load_program(1, path, true))
   {
      RARCH_ERR("Failed to load HLSL shader %s into first-pass.\n", path);
      return false;
   }

   if (*g_settings.video.second_pass_shader && g_settings.video.render_to_texture)
   {
      if (!load_program(2, g_settings.video.second_pass_shader, true))
      {
         RARCH_ERR("Failed to load HLSL shader %s into secondpass.\n", path);
         return false;
      }

      hlsl_shader_num = 2;
   }
   else
   {
      prg[2] = prg[0];
      hlsl_shader_num = 1;
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
}

static bool load_preset(const char *path)
{
   return false;
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
      {
         RARCH_ERR("Loading of HLSL shader %s failed.\n", path);
         return false;
      }
   }
   for(unsigned i = 1; i <= hlsl_shader_num; i++)
      set_program_attributes(i);

   active_index = 1;
   d3d_device_ptr->SetVertexShader(prg[active_index].vprg);
   d3d_device_ptr->SetPixelShader(prg[active_index].fprg);

   hlsl_active = true;
   return true;
}

void hlsl_use(unsigned index)
{
   if (hlsl_active)
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