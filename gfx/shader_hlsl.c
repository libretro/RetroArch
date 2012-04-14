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
   D3DXHANDLE	vid_size_v;
   D3DXHANDLE	tex_size_v;
   D3DXHANDLE	out_size_v;
   XMMATRIX mvp;
};

static struct hlsl_program prg[SSNES_HLSL_MAX_SHADERS];
static bool hlsl_active = false;

static const char *stock_hlsl_program =
      "void main_vertex"
      "("
      "	float2 position : POSITION,"
      "	float2 texCoord : TEXCOORD0,"
      " uniform float4x4 modelViewProj : register(c0),"
      "	out float4 oPosition : POSITION,"
      "	out float2 otexCoord : TEXCOORD"
      ")"
      "{"
      "	oPosition = mul(modelViewProj, float4(position, 0.0, 1.0));"
      "	otexCoord = texCoord;"
      "}"
      ""
      "struct output"
      "{"
      " float4 color: COLOR;"
      "}"
      ""
      "struct input"
      "{"
      " float2 video_size;"
      " float2 texture_size;"
      " float2 output_size;"
      "}"
      ""
      "output main_fragment(float2 tex : TEXCOORD0, uniform sampler2D decal : register(s0), uniform input IN)"
      "{"
      "   output OUT;"
      "   OUT.color = tex2D(decal, tex);"
      "   return OUT;"
      "}";

void hlsl_set_proj_matrix(XMMATRIX rotation_value)
{
   if (hlsl_active && prg[active_index].mvp)
      prg[active_index].mvp = rotation_value;
}

void hlsl_set_params(IDirect3DDevice9 * device)
{
   if (!hlsl_active)
      return;

   if (prg[active_index].mvp)
      device->SetVertexShaderConstantF(0, (FLOAT*)&prg[active_index].mvp, 4);
}

static bool load_program(unsigned index, const char *prog, bool path_is_file)
{
   bool ret, ret_fp, ret_vp;
   ID3DXBuffer *listing_f = NULL;
   ID3DXBuffer *listing_v = NULL;
   ID3DXBuffer *code_f = NULL;
   ID3DXBuffer *code_v = NULL;

   ret = true;

   if (path_is_file)
   {
      ret_fp = D3DXCompileShaderFromFile(prog, NULL, NULL, "main_fragment", "ps_2_0", 0, &code_f, &listing_f, NULL); 
      ret_vp = D3DXCompileShaderFromFile(prog, NULL, NULL, "main_vertex", "vs_2_0", 0, &code_v, &listing_v, NULL); 
   }
   else
   {
      ret_fp = D3DXCompileShader(prog, sizeof(prog), NULL, NULL, "main_fragment", "ps_2_0", 0, &code_f, &listing_f, NULL );
      ret_vp = D3DXCompileShader(prog, sizeof(prog), NULL, NULL, "main_vertex", "vs_2_0", 0, &code_v, &listing_v, NULL );
   }

   if (FAILED(ret_fp) || FAILED(ret_vp))
   {
      SSNES_ERR("HLSL error:\n");
      if(listing_f)
         SSNES_ERR("Fragment:\n%s\n", (char*)listing_f->GetBufferPointer());
      if(listing_v)
         SSNES_ERR("Vertex:\n%s\n", (char*)listing_v->GetBufferPointer());

      ret = false;
      goto end;
   }

   prg[index].fprg = D3DDevice_CreatePixelShader((const DWORD*)code_f->GetBufferPointer());
   prg[index].vprg = D3DDevice_CreateVertexShader((const DWORD*)code_v->GetBufferPointer());
   code_f->Release();
   code_v->Release();

end:
   free(listing_f);
   free(listing_v);
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
   if (!load_stock())
      return false;

#if 0
   SSNES_LOG("Loading HLSL file: %s\n", path);

   if (!load_program(1, path, true))
      return false;
#endif

   return true;
}

static void hlsl_deinit_progs(void)
{
   D3DResource_Release((D3DResource *)vid->pPixelShader);
   D3DResource_Release((D3DResource *)vid->pVertexShader);
}

static void hlsl_deinit_state(void)
{
   hlsl_active = false;

   hlsl_deinit_progs();
}

bool hlsl_init(const char *path)
{
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

   hlsl_active = true;
   return true;
}

void hlsl_use(IDirect3DDevice9 * device, unsigned index)
{
   if (hlsl_active && prg[index].vprg && prg[index].fprg)
   {
      active_index = index;
      D3DDevice_SetVertexShader(device, prg[index].vprg);
      D3DDevice_SetPixelShader(device, prg[index].fprg);
   }
}

// Full deinit.
void hlsl_deinit(void)
{
   if (!hlsl_active)
      return;

   hlsl_deinit_state();
}
