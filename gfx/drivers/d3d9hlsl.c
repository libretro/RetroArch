/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - OV2
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

/* Direct3D 9 driver with HLSL runtime backend.
 *
 * Minimum version : Direct3D 9.0 (2002)
 * Minimum OS      : Windows 98, Windows 2000, Windows ME
 * Recommended OS  : Windows XP
 * Requirements    : HLSL or fixed function backend
 */

#define CINTERFACE

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include <formats/image.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_math.h>

#include <string.h>
#include <retro_inline.h>
#include <retro_math.h>

#include <d3d9.h>
#include <d3dx9shader.h>

#include "d3d_shaders/opaque.hlsl.d3d9.h"
#include "d3d9_renderchain.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <defines/d3d_defines.h>
#include "../common/d3d_common.h"
#include "../common/d3d9_common.h"
#include "../video_coord_array.h"
#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../frontend/frontend_driver.h"

#include "../common/win32_common.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"

#include "../../core.h"
#include "../../verbosity.h"
#include "../../retroarch.h"

#ifdef __WINRT__
#error "UWP does not support D3D9"
#endif

/* TODO/FIXME - Temporary workaround for D3D9 not being able to poll flags during init */
static gfx_ctx_driver_t d3d9_hlsl_fake_context;

/* BEGIN HLSL RENDERCHAIN */

#define RARCH_HLSL_MAX_SHADERS 16

typedef struct hlsl_renderchain
{
   struct d3d9_renderchain chain;
   struct shader_pass stock_shader;
} hlsl_renderchain_t;

static void *d3d9_hlsl_get_constant_by_name(LPD3DXCONSTANTTABLE prog, const char *name)
{
   char lbl[64];
   lbl[0] = '\0';
   snprintf(lbl, sizeof(lbl), "$%s", name);
   return d3d9x_constant_table_get_constant_by_name(prog, NULL, lbl);
}

static INLINE void d3d9_hlsl_set_param_2f(LPD3DXCONSTANTTABLE prog, LPDIRECT3DDEVICE9 userdata, const char *name, const void *values)
{
   D3DXHANDLE param         = (D3DXHANDLE)d3d9_hlsl_get_constant_by_name(prog, name);
   if (param)
      d3d9x_constant_table_set_float_array(userdata, prog, (void*)param, values, 2);
}

static INLINE void d3d9_hlsl_set_param_1f(LPD3DXCONSTANTTABLE prog, LPDIRECT3DDEVICE9 userdata, const char *name, const void *value)
{
   D3DXHANDLE param         = (D3DXHANDLE)d3d9_hlsl_get_constant_by_name(prog, name);
   float *val               = (float*)value;
   if (param)
      d3d9x_constant_table_set_float(prog, userdata, (void*)param, *val);
}

static INLINE void d3d9_hlsl_bind_program(LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass)
{
   IDirect3DDevice9_SetVertexShader(dev, (LPDIRECT3DVERTEXSHADER9)pass->vprg);
   IDirect3DDevice9_SetPixelShader(dev, (LPDIRECT3DPIXELSHADER9)pass->fprg);
}

static INLINE void d3d9_hlsl_set_param_matrix(LPD3DXCONSTANTTABLE prog, LPDIRECT3DDEVICE9 userdata,
      const char *name, const void *values)
{
   D3DXHANDLE param         = (D3DXHANDLE)d3d9_hlsl_get_constant_by_name(prog, name);
   if (param)
      d3d9x_constant_table_set_matrix(userdata, prog, (void*)param, (D3DMATRIX*)values);
}

static bool d3d9_hlsl_load_program_from_file(
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      const char *prog)
{
   ID3DXBuffer *listing_f                    = NULL;
   ID3DXBuffer *listing_v                    = NULL;
   ID3DXBuffer *code_f                       = NULL;
   ID3DXBuffer *code_v                       = NULL;

   if (string_is_empty(prog))
      return false;

   if (!d3d9x_compile_shader_from_file(prog, NULL, NULL,
            "main_fragment", "ps_3_0", 0, &code_f, &listing_f, &pass->ftable))
   {
      RARCH_ERR("Could not compile fragment shader program (%s)..\n", prog);
      goto error;
   }
   if (!d3d9x_compile_shader_from_file(prog, NULL, NULL,
            "main_vertex", "vs_3_0", 0, &code_v, &listing_v, &pass->vtable))
   {
      RARCH_ERR("Could not compile vertex shader program (%s)..\n", prog);
      goto error;
   }

   IDirect3DDevice9_CreatePixelShader(dev,
         (const DWORD*)code_f->lpVtbl->GetBufferPointer(code_f),
         (LPDIRECT3DPIXELSHADER9*)&pass->fprg);
   IDirect3DDevice9_CreateVertexShader(dev,
         (const DWORD*)code_v->lpVtbl->GetBufferPointer(code_v),
         (LPDIRECT3DVERTEXSHADER9*)&pass->vprg);
   code_f->lpVtbl->Release(code_f);
   code_v->lpVtbl->Release(code_v);

   return true;

error:
   RARCH_ERR("Cg/HLSL error:\n");
   if (listing_f)
      RARCH_ERR("Fragment:\n%s\n", (char*)listing_f->lpVtbl->GetBufferPointer(listing_f));
   if (listing_v)
      RARCH_ERR("Vertex:\n%s\n", (char*)listing_v->lpVtbl->GetBufferPointer(listing_v));
   listing_f->lpVtbl->Release(listing_f);
   listing_v->lpVtbl->Release(listing_v);

   return false;
}

static bool d3d9_hlsl_load_program(
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      const char *prog)
{
   ID3DXBuffer *listing_f                    = NULL;
   ID3DXBuffer *listing_v                    = NULL;
   ID3DXBuffer *code_f                       = NULL;
   ID3DXBuffer *code_v                       = NULL;

   if (!d3d9x_compile_shader(prog, strlen(prog), NULL, NULL,
            "main_fragment", "ps_3_0", 0, &code_f, &listing_f,
            &pass->ftable ))
   {
      RARCH_ERR("Could not compile stock fragment shader..\n");
      goto error;
   }
   if (!d3d9x_compile_shader(prog, strlen(prog), NULL, NULL,
            "main_vertex", "vs_3_0", 0, &code_v, &listing_v,
            &pass->vtable ))
   {
      RARCH_ERR("Could not compile stock vertex shader..\n");
      goto error;
   }

   IDirect3DDevice9_CreatePixelShader(dev,
         (const DWORD*)code_f->lpVtbl->GetBufferPointer(code_f),
         (LPDIRECT3DPIXELSHADER9*)&pass->fprg);
   IDirect3DDevice9_CreateVertexShader(dev,
         (const DWORD*)code_v->lpVtbl->GetBufferPointer(code_v),
         (LPDIRECT3DVERTEXSHADER9*)&pass->vprg);
   code_f->lpVtbl->Release(code_f);
   code_v->lpVtbl->Release(code_v);

   return true;

error:
   RARCH_ERR("Cg/HLSL error:\n");
   if (listing_f)
      RARCH_ERR("Fragment:\n%s\n", (char*)listing_f->lpVtbl->GetBufferPointer(listing_f));
   if (listing_v)
      RARCH_ERR("Vertex:\n%s\n", (char*)listing_v->lpVtbl->GetBufferPointer(listing_v));
   listing_f->lpVtbl->Release(listing_f);
   listing_v->lpVtbl->Release(listing_v);

   return false;
}

static void hlsl_d3d9_renderchain_set_shader_params(
      d3d9_renderchain_t *chain,
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      unsigned video_w, unsigned video_h,
      unsigned tex_w, unsigned tex_h,
      unsigned viewport_w, unsigned viewport_h)
{
   float frame_cnt;
   float video_size[2];
   float texture_size[2];
   float output_size[2];
   LPD3DXCONSTANTTABLE fprg                 = (LPD3DXCONSTANTTABLE)pass->ftable;
   LPD3DXCONSTANTTABLE vprg                 = (LPD3DXCONSTANTTABLE)pass->vtable;

   video_size[0]                            = video_w;
   video_size[1]                            = video_h;
   texture_size[0]                          = tex_w;
   texture_size[1]                          = tex_h;
   output_size[0]                           = viewport_w;
   output_size[1]                           = viewport_h;

   d3d9x_constant_table_set_defaults(dev, fprg);
   d3d9x_constant_table_set_defaults(dev, vprg);

   d3d9_hlsl_set_param_2f(vprg, dev, "IN.video_size",      &video_size);
   d3d9_hlsl_set_param_2f(fprg, dev, "IN.video_size",      &video_size);
   d3d9_hlsl_set_param_2f(vprg, dev, "IN.texture_size",    &texture_size);
   d3d9_hlsl_set_param_2f(fprg, dev, "IN.texture_size",    &texture_size);
   d3d9_hlsl_set_param_2f(vprg, dev, "IN.output_size",     &output_size);
   d3d9_hlsl_set_param_2f(fprg, dev, "IN.output_size",     &output_size);

   frame_cnt = chain->frame_count;

   if (pass->info.pass->frame_count_mod)
      frame_cnt         = chain->frame_count
         % pass->info.pass->frame_count_mod;

   d3d9_hlsl_set_param_1f(fprg, dev, "IN.frame_count",     &frame_cnt);
   d3d9_hlsl_set_param_1f(vprg, dev, "IN.frame_count",     &frame_cnt);
}

static bool hlsl_d3d9_renderchain_init_shader_fvf(
      d3d9_renderchain_t *chain,
      struct shader_pass *pass)
{
   static const D3DVERTEXELEMENT9 decl[4] =
   {
      {0, offsetof(Vertex, x),  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_POSITION, 0},
      {0, offsetof(Vertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_COLOR, 0},
      {0, offsetof(Vertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_TEXCOORD, 0},
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
   struct shader_pass pass       = { 0 };
   unsigned fmt                  =
        (_fmt == RETRO_PIXEL_FORMAT_RGB565) 
      ? D3D9_RGB565_FORMAT
      : D3D9_XRGB8888_FORMAT;

   pass.info                     = *info;
   pass.last_width               = 0;
   pass.last_height              = 0;
   pass.attrib_map               = (struct unsigned_vector_list*)
      unsigned_vector_list_new();

   chain->prev.ptr               = 0;

   for (i = 0; i < TEXTURES; i++)
   {
      int32_t filter             = d3d_translate_filter(info->pass->filter);
      chain->prev.last_width[i]  = 0;
      chain->prev.last_height[i] = 0;
      chain->prev.vertex_buf[i]  = (LPDIRECT3DVERTEXBUFFER9)
         d3d9_vertex_buffer_new(
            chain->dev, 4 * sizeof(struct Vertex),
            D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, NULL);

      if (!chain->prev.vertex_buf[i])
         return false;

      chain->prev.tex[i] = (LPDIRECT3DTEXTURE9)
         d3d9_texture_new(chain->dev,
            info->tex_w, info->tex_h, 1, 0, fmt,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!chain->prev.tex[i])
         return false;

      IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)chain->prev.tex[i]);
      IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_MINFILTER, filter);
      IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_MAGFILTER, filter);
      IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_ADDRESSU,  D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_ADDRESSV,  D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)NULL);
   }

   d3d9_hlsl_load_program_from_file(chain->dev,
         &pass, info->pass->source.path);

   if (!hlsl_d3d9_renderchain_init_shader_fvf(chain, &pass))
      return false;
   shader_pass_vector_list_append(chain->passes, pass);
   return true;
}

static void hlsl_d3d9_renderchain_calc_and_set_shader_mvp(
      hlsl_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   struct d3d_matrix proj, ortho, rot, matrix;

   d3d_matrix_identity(&ortho);
   d3d_matrix_ortho_off_center_lh(&ortho, 0,
         vp_width, 0, vp_height, 0, 1);
   d3d_matrix_identity(&rot);
   d3d_matrix_rotation_z(&rot, rotation * (D3D_PI / 2.0));
   d3d_matrix_multiply(&proj, &ortho, &rot);
   d3d_matrix_transpose(&matrix, &proj);

   d3d9_hlsl_set_param_matrix((LPD3DXCONSTANTTABLE)pass->vtable,
         chain->chain.dev, "modelViewProj", (const void*)&matrix);
}

static INLINE void d3d9_hlsl_renderchain_set_vertices_on_change(
      d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation
      )
{
   struct Vertex vert[4];
   void *verts       = NULL;
   const struct
      LinkInfo *info = (const struct LinkInfo*)&pass->info;
   float _u          = (float)(width)  / info->tex_w;
   float _v          = (float)(height) / info->tex_h;

   pass->last_width  = width;
   pass->last_height = height;

   /* Copied from D3D8 driver */
   vert[0].x        =  0.0f;
   vert[0].y        =  1.0f;
   vert[0].z        =  1.0f;

   vert[1].x        =  1.0f;
   vert[1].y        =  1.0f;
   vert[1].z        =  1.0f;

   vert[2].x        =  0.0f;
   vert[2].y        =  0.0f;
   vert[2].z        =  1.0f;

   vert[3].x        =  1.0f;
   vert[3].y        =  0.0f;
   vert[3].z        =  1.0f;

   vert[0].u        = 0.0f;
   vert[0].v        = 0.0f;
   vert[1].v        = 0.0f;
   vert[2].u        = 0.0f;
   vert[1].u        = _u;
   vert[2].v        = _v;
   vert[3].u        = _u;
   vert[3].v        = _v;

   vert[0].color    = 0xFFFFFFFF;
   vert[1].color    = 0xFFFFFFFF;
   vert[2].color    = 0xFFFFFFFF;
   vert[3].color    = 0xFFFFFFFF;

   /* Align texels and vertices.
    *
    * Fixes infamous 'half-texel offset' issue of D3D9
    *	http://msdn.microsoft.com/en-us/library/bb219690%28VS.85%29.aspx.
    */
   /* Maybe we do need something like this left out for now */
#if 0
   for (i = 0; i < 4; i++)
   {
      vert[i].x    -= 0.5f;
      vert[i].y    += 0.5f;
   }
#endif

   IDirect3DVertexBuffer9_Lock(pass->vertex_buf, 0, 0, &verts, 0);
   memcpy(verts, vert, sizeof(vert));
   IDirect3DVertexBuffer9_Unlock(pass->vertex_buf);
}


static void hlsl_d3d9_renderchain_set_vertices(
      d3d9_video_t *d3d,
      hlsl_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      uint64_t frame_count,
      unsigned rotation)
{
   if (pass->last_width != width || pass->last_height != height)
      d3d9_hlsl_renderchain_set_vertices_on_change(&chain->chain,
            pass, width, height, out_width, out_height,
            vp_width, vp_height, rotation);

   hlsl_d3d9_renderchain_calc_and_set_shader_mvp(chain, pass,
         vp_width, vp_height, rotation);
   hlsl_d3d9_renderchain_set_shader_params(&chain->chain,
         chain->chain.dev,
         pass,
         width, height,
         pass->info.tex_w, pass->info.tex_h,
         vp_width, vp_height);
}

static void d3d9_hlsl_deinit_progs(hlsl_renderchain_t *chain)
{
   if (chain->chain.passes->count >= 1)
   {
      unsigned i;

      d3d9_vertex_buffer_free(NULL,
            chain->chain.passes->data[0].vertex_decl);

      for (i = 1; i < chain->chain.passes->count; i++)
      {
         if (chain->chain.passes->data[i].tex)
            IDirect3DTexture9_Release(chain->chain.passes->data[i].tex);
         chain->chain.passes->data[i].tex = NULL;
         d3d9_vertex_buffer_free(
               chain->chain.passes->data[i].vertex_buf,
               chain->chain.passes->data[i].vertex_decl);
      }
   }
}

static void d3d9_hlsl_destroy_resources(hlsl_renderchain_t *chain)
{
   unsigned i;

   for (i = 0; i < TEXTURES; i++)
   {
      if (chain->chain.prev.tex[i])
         IDirect3DTexture9_Release(chain->chain.prev.tex[i]);
      if (chain->chain.prev.vertex_buf[i])
         d3d9_vertex_buffer_free(chain->chain.prev.vertex_buf[i], NULL);
   }

   d3d9_hlsl_deinit_progs(chain);

   for (i = 0; i < chain->chain.luts->count; i++)
   {
      if (chain->chain.luts->data[i].tex)
         IDirect3DTexture9_Release(chain->chain.luts->data[i].tex);
   }
}

static void hlsl_d3d9_renderchain_free(void *data)
{
   hlsl_renderchain_t *chain = (hlsl_renderchain_t*)data;

   if (!chain)
      return;

   d3d9_hlsl_destroy_resources(chain);
   d3d9_renderchain_destroy_passes_and_luts(&chain->chain);
   free(chain);
}

static bool hlsl_d3d9_renderchain_init(
      d3d9_video_t *d3d,
      hlsl_renderchain_t *chain,
      LPDIRECT3DDEVICE9 dev,
      const D3DVIEWPORT9 *final_viewport,
      const struct LinkInfo *info,
      unsigned fmt
      )
{
   chain->chain.dev                         = dev;
   chain->chain.final_viewport              = (D3DVIEWPORT9*)final_viewport;
   chain->chain.frame_count                 = 0;
   chain->chain.pixel_size                  = (fmt == RETRO_PIXEL_FORMAT_RGB565) ? 2 : 4;

   if (!hlsl_d3d9_renderchain_create_first_pass(dev, &chain->chain, info, fmt))
      return false;
   if (!d3d9_hlsl_load_program(chain->chain.dev, &chain->stock_shader, stock_hlsl_program))
      return false;

   d3d9_hlsl_bind_program(dev, &chain->stock_shader);

   return true;
}

static void hlsl_d3d9_renderchain_render_pass(
      hlsl_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned pass_index)
{
   /* Currently we override the passes shader program 
      with the stock shader as at least the last pass 
      is not setup correctly */
#if 0
   d3d9_hlsl_bind_program(chain->chain.dev, pass);
#else
   d3d9_hlsl_bind_program(chain->chain.dev, &chain->stock_shader);
#endif

   IDirect3DDevice9_SetTexture(chain->chain.dev, 0,
         (IDirect3DBaseTexture9*)pass->tex);

   /* D3D8 sets the sampler address modes - 
      I've left them out for the time being 
      but maybe this is a bug in d3d9 */
#if 0
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
#endif
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_MINFILTER,
         d3d_translate_filter(pass->info.pass->filter));
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_MAGFILTER,
         d3d_translate_filter(pass->info.pass->filter));

   IDirect3DDevice9_SetVertexDeclaration(
         chain->chain.dev, pass->vertex_decl);
   IDirect3DDevice9_SetStreamSource(
         chain->chain.dev, 0, pass->vertex_buf,
         0,sizeof(struct Vertex));

#if 0
   /* Set orig texture. */
   d3d9_hlsl_renderchain_bind_orig(chain, chain->dev, pass);

   /* Set prev textures. */
   d3d9_hlsl_renderchain_bind_prev(chain, chain->dev, pass);

   /* Set lookup textures */
   for (i = 0; i < chain->luts->count; i++)
   {
      CGparameter vparam;
      CGparameter fparam = d3d9_hlsl_get_constant_by_name(
            pass->fprg, chain->luts->data[i].id);
      int bound_index    = -1;

      if (fparam)
      {
         unsigned index  = cgGetParameterResourceIndex(fparam);
         bound_index     = index;

         d3d9_renderchain_add_lut_internal(chain, index, i);
      }

      vparam = d3d9_hlsl_get_constant_by_name(pass->vprg,
            chain->luts->data[i].id);

      if (vparam)
      {
         unsigned index = cgGetParameterResourceIndex(vparam);
         if (index != (unsigned)bound_index)
            d3d9_renderchain_add_lut_internal(chain, index, i);
      }
   }

   /* We only bother binding passes which are two indices behind. */
   if (pass_index >= 3)
      d3d9_hlsl_renderchain_bind_pass(chain, chain->chain.dev,
            pass, pass_index);
#endif

   IDirect3DDevice9_BeginScene(chain->chain.dev);
   IDirect3DDevice9_DrawPrimitive(chain->chain.dev,
         D3DPT_TRIANGLESTRIP, 0, 2);
   IDirect3DDevice9_EndScene(chain->chain.dev);

   /* So we don't render with linear filter into render targets,
    * which apparently looked odd (too blurry). */
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

   d3d9_renderchain_unbind_all(&chain->chain);
}

static void hlsl_d3d9_renderchain_render(
      d3d9_video_t *d3d,
      const void *frame,
      unsigned width, unsigned height,
      unsigned pitch, unsigned rotation)
{
   LPDIRECT3DSURFACE9 back_buffer, target;
   unsigned i, current_width, current_height,
          out_width = 0, out_height = 0;
   struct shader_pass *last_pass    = NULL;
   struct shader_pass *first_pass   = NULL;
   hlsl_renderchain_t *chain        = (hlsl_renderchain_t*)
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

   d3d9_blit_to_texture(first_pass->tex,
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
   d3d9_device_get_render_target(
         chain->chain.dev, 0, (void**)&back_buffer);

   /* In-between render target passes. */
   for (i = 0; i < chain->chain.passes->count - 1; i++)
   {
      D3DVIEWPORT9   viewport        = {0};
      struct shader_pass *from_pass  = (struct shader_pass*)
         &chain->chain.passes->data[i];
      struct shader_pass *to_pass    = (struct shader_pass*)
         &chain->chain.passes->data[i + 1];

      IDirect3DTexture9_GetSurfaceLevel(
		      (LPDIRECT3DTEXTURE9)to_pass->tex, 0, (IDirect3DSurface9**)&target);
      IDirect3DDevice9_SetRenderTarget(chain->chain.dev, 0, target);

      d3d9_convert_geometry(&from_pass->info,
            &out_width, &out_height,
            current_width, current_height, chain->chain.final_viewport);

      /* Clear out whole FBO. */
      viewport.Width  = to_pass->info.tex_w;
      viewport.Height = to_pass->info.tex_h;
      viewport.MinZ   = 0.0f;
      viewport.MaxZ   = 1.0f;

      IDirect3DDevice9_SetViewport(
            chain->chain.dev, (D3DVIEWPORT9*)&viewport);
      IDirect3DDevice9_Clear(
            chain->chain.dev, 0, 0, D3DCLEAR_TARGET,
            0, 1, 0);

      viewport.Width  = out_width;
      viewport.Height = out_height;

      IDirect3DDevice9_SetViewport(
            chain->chain.dev, (D3DVIEWPORT9*)&viewport);

      hlsl_d3d9_renderchain_set_vertices(
            d3d,
            chain, from_pass,
            current_width, current_height,
            out_width, out_height,
            out_width, out_height,
            chain->chain.frame_count, 0);

      hlsl_d3d9_renderchain_render_pass(chain,
            from_pass, 
            i + 1);

      current_width  = out_width;
      current_height = out_height;
      IDirect3DSurface9_Release(target);
   }

   /* Final pass */
   IDirect3DDevice9_SetRenderTarget(chain->chain.dev, 0, back_buffer);

   last_pass = (struct shader_pass*)&chain->chain.passes->
      data[chain->chain.passes->count - 1];

   d3d9_convert_geometry(&last_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->chain.final_viewport);

   IDirect3DDevice9_SetViewport(
         chain->chain.dev, (D3DVIEWPORT9*)chain->chain.final_viewport);

   hlsl_d3d9_renderchain_set_vertices(
         d3d,
         chain, last_pass,
         current_width, current_height,
         out_width, out_height,
         chain->chain.final_viewport->Width,
         chain->chain.final_viewport->Height,
         chain->chain.frame_count, rotation);

   hlsl_d3d9_renderchain_render_pass(chain, last_pass,
         chain->chain.passes->count);

   chain->chain.frame_count++;

   if (back_buffer)
      IDirect3DSurface9_Release(back_buffer);

   d3d9_renderchain_end_render(&chain->chain);
   d3d9_hlsl_bind_program(chain->chain.dev, &chain->stock_shader);
   hlsl_d3d9_renderchain_calc_and_set_shader_mvp(
         chain, &chain->stock_shader,
         chain->chain.final_viewport->Width,
         chain->chain.final_viewport->Height, 0);
}

static bool hlsl_d3d9_renderchain_add_pass(
      hlsl_renderchain_t *chain,
      const struct LinkInfo *info)
{
   struct shader_pass pass;

   pass.info                   = *info;
   pass.last_width             = 0;
   pass.last_height            = 0;
   pass.attrib_map             = (struct unsigned_vector_list*)
      unsigned_vector_list_new();
   pass.pool                   = D3DPOOL_DEFAULT;

   d3d9_hlsl_load_program_from_file(
         chain->chain.dev, &pass, info->pass->source.path);

   if (hlsl_d3d9_renderchain_init_shader_fvf(&chain->chain, &pass))
      return d3d9_renderchain_add_pass(&chain->chain, &pass,
            info);
   return false;
}

/* END HLSL RENDERCHAIN */

static uint32_t d3d9_hlsl_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_HLSL);

   return flags;
}

static void d3d9_hlsl_deinitialize(d3d9_video_t *d3d)
{
   font_driver_free_osd();

   hlsl_d3d9_renderchain_free(d3d->renderchain_data);

   d3d9_vertex_buffer_free(d3d->menu_display.buffer,
         d3d->menu_display.decl);

   d3d->renderchain_data    = NULL;
   d3d->menu_display.buffer = NULL;
   d3d->menu_display.decl   = NULL;
}

static bool d3d9_hlsl_init_base(
      d3d9_video_t *d3d, const video_info_t *info)
{
   D3DPRESENT_PARAMETERS d3dpp;
#ifndef _XBOX
   HWND focus_window  = win32_get_window();
#endif

   memset(&d3dpp, 0, sizeof(d3dpp));

   g_pD3D9            = (LPDIRECT3D9)d3d9_create();

   /* this needs g_pD3D9 created first */
   d3d9_make_d3dpp(d3d, info, &d3dpp);

   if (!g_pD3D9)
      return false;
   if (!d3d9_create_device(&d3d->dev, &d3dpp,
            g_pD3D9,
            focus_window,
            d3d->cur_mon_id)
      )
      return false;
   return true;
}

static bool renderchain_d3d_hlsl_init_first(
      enum gfx_ctx_api api,
      void **renderchain_handle)
{
   hlsl_renderchain_t *renderchain =
      (hlsl_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return false;

   d3d9_init_renderchain(&renderchain->chain);

   *renderchain_handle = renderchain;

   return true;
}

static bool d3d9_hlsl_init_chain(d3d9_video_t *d3d,
      unsigned input_scale, bool rgb32)
{
   struct LinkInfo link_info;
#ifndef _XBOX
   unsigned current_width, current_height, out_width, out_height;
#endif
   unsigned i           = 0;
   settings_t *settings = config_get_ptr();
   bool video_smooth    = settings->bools.video_smooth;

   /* Setup information for first pass. */
   link_info.pass       = NULL;
   link_info.tex_w      = input_scale * RARCH_SCALE_BASE;
   link_info.tex_h      = input_scale * RARCH_SCALE_BASE;
   link_info.pass       = &d3d->shader.pass[0];

   if (!renderchain_d3d_hlsl_init_first(GFX_CTX_DIRECT3D9_API,
            &d3d->renderchain_data))
      return false;
   if (!d3d->renderchain_data)
      return false;

   RARCH_LOG("[D3D9]: Using HLSL shader backend.\n");

   if (
         !hlsl_d3d9_renderchain_init(
            d3d, (hlsl_renderchain_t*)d3d->renderchain_data,
            d3d->dev, &d3d->final_viewport, &link_info,
            rgb32
            ? RETRO_PIXEL_FORMAT_XRGB8888 
            : RETRO_PIXEL_FORMAT_RGB565
            )
      )
      return false;

   d3d9_log_info(&link_info);

#ifndef _XBOX
   current_width  = link_info.tex_w;
   current_height = link_info.tex_h;
   out_width      = 0;
   out_height     = 0;

   for (i = 1; i < d3d->shader.passes; i++)
   {
      d3d9_convert_geometry(
            &link_info,
            &out_width, &out_height,
            current_width, current_height, &d3d->final_viewport);

      link_info.pass  = &d3d->shader.pass[i];
      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      current_width   = out_width;
      current_height  = out_height;

      if (!hlsl_d3d9_renderchain_add_pass(
               (hlsl_renderchain_t*)d3d->renderchain_data, &link_info))
      {
         RARCH_ERR("[D3D9]: Failed to add pass.\n");
         return false;
      }
      d3d9_log_info(&link_info);
   }
#endif

   {
      hlsl_renderchain_t *_chain = (hlsl_renderchain_t*)d3d->renderchain_data;
      d3d9_renderchain_t *chain   = (d3d9_renderchain_t*)&_chain->chain;

      for (i = 0; i < d3d->shader.luts; i++)
      {
         if (!d3d9_renderchain_add_lut(
                  chain,
                  d3d->shader.lut[i].id,
                  d3d->shader.lut[i].path,
                  d3d->shader.lut[i].filter == RARCH_FILTER_UNSPEC 
                  ? video_smooth 
                  : (d3d->shader.lut[i].filter == RARCH_FILTER_LINEAR)))
         {
            RARCH_ERR("[D3D9]: Failed to init LUTs.\n");
            return false;
         }
      }
   }

   return true;
}

static bool d3d9_hlsl_initialize(
      d3d9_video_t *d3d, const video_info_t *info)
{
   unsigned width, height;
   bool ret             = true;

   if (!g_pD3D9)
      ret               = d3d9_hlsl_init_base(d3d, info);
   else if (d3d->needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;

      d3d9_make_d3dpp(d3d, info, &d3dpp);

      /* The D3DX font driver uses POOL_DEFAULT resources
       * and will prevent a clean reset here
       * another approach would be to keep track of all created D3D
       * font objects and free/realloc them around the d3d_reset call  */
#ifdef HAVE_MENU
      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
#endif

      if (!d3d9_reset(d3d->dev, &d3dpp))
      {
         d3d9_hlsl_deinitialize(d3d);
         IDirect3D9_Release(g_pD3D9);
         g_pD3D9 = NULL;

         ret     = d3d9_hlsl_init_base(d3d, info);
         if (ret)
            RARCH_LOG("[D3D9]: Recovered from dead state.\n");
      }

#ifdef HAVE_MENU
      menu_driver_init(info->is_threaded);
#endif
   }

   if (!ret)
      return ret;

   if (!d3d9_hlsl_init_chain(d3d, info->input_scale, info->rgb32))
   {
      RARCH_ERR("[D3D9]: Failed to initialize render chain.\n");
      return false;
   }

   video_driver_get_size(&width, &height);
   d3d9_set_viewport(d3d,
      width, height, false, true);

   font_driver_init_osd(d3d, info,
         false,
         info->is_threaded,
         FONT_DRIVER_RENDER_D3D9_API);

   {
      static const D3DVERTEXELEMENT9 VertexElements[4] = {
         {0, offsetof(Vertex, x),
            D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_POSITION,0},
         {0, offsetof(Vertex, u),
            D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_TEXCOORD,0},
         {0, offsetof(Vertex, color),
            D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_COLOR,   0},
         D3DDECL_END()
      };
      if (!d3d9_vertex_declaration_new(d3d->dev,
               (void*)VertexElements, (void**)&d3d->menu_display.decl))
         return false;
   }

   d3d->menu_display.offset = 0;
   d3d->menu_display.size   = 1024;
   d3d->menu_display.buffer = d3d9_vertex_buffer_new(
         d3d->dev, d3d->menu_display.size * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
         0,
         D3DPOOL_DEFAULT,
         NULL);

   if (!d3d->menu_display.buffer)
      return false;

   d3d_matrix_identity(&d3d->mvp_transposed);
   d3d_matrix_ortho_off_center_lh(&d3d->mvp_transposed, 0, 1, 0, 1, 0, 1);
   d3d_matrix_transpose(&d3d->mvp, &d3d->mvp_transposed);

   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SCISSORTESTENABLE, TRUE);

   return true;
}

static bool d3d9_hlsl_restore(d3d9_video_t *d3d)
{
   d3d9_hlsl_deinitialize(d3d);

   if (!d3d9_hlsl_initialize(d3d, &d3d->video_info))
   {
      RARCH_ERR("[D3D9]: Restore error.\n");
      return false;
   }

   d3d->needs_restore = false;

   return true;
}

static bool d3d9_hlsl_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return false;

   if (!string_is_empty(d3d->shader_path))
      free(d3d->shader_path);
   d3d->shader_path = NULL;

   switch (type)
   {
      case RARCH_SHADER_CG:
      case RARCH_SHADER_HLSL:
         if (!string_is_empty(path))
            d3d->shader_path = strdup(path);

         break;
      case RARCH_SHADER_NONE:
         break;
      default:
         RARCH_WARN("[D3D9]: Only Cg shaders are supported. Falling back to stock.\n");
   }

   if (!d3d9_process_shader(d3d) || !d3d9_hlsl_restore(d3d))
   {
      RARCH_ERR("[D3D9]: Failed to set shader.\n");
      return false;
   }

   return true;
}

static bool d3d9_hlsl_init_internal(d3d9_video_t *d3d,
      const video_info_t *info, input_driver_t **input,
      void **input_data)
{
#ifdef HAVE_MONITOR
   bool windowed_full;
   RECT mon_rect;
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use;
#endif
#ifdef HAVE_WINDOW
   DWORD style;
   unsigned win_width        = 0;
   unsigned win_height       = 0;
   RECT rect                 = {0};
#endif
   unsigned full_x           = 0;
   unsigned full_y           = 0;
   settings_t    *settings   = config_get_ptr();
   overlay_t *menu           = (overlay_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return false;

   d3d->menu                 = menu;
   d3d->cur_mon_id           = 0;
   d3d->menu->tex_coords[0]  = 0;
   d3d->menu->tex_coords[1]  = 0;
   d3d->menu->tex_coords[2]  = 1;
   d3d->menu->tex_coords[3]  = 1;
   d3d->menu->vert_coords[0] = 0;
   d3d->menu->vert_coords[1] = 1;
   d3d->menu->vert_coords[2] = 1;
   d3d->menu->vert_coords[3] = -1;

#ifdef HAVE_WINDOW
   memset(&d3d->windowClass, 0, sizeof(d3d->windowClass));
   d3d->windowClass.lpfnWndProc = wnd_proc_d3d_common;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
      d3d->windowClass.lpfnWndProc = wnd_proc_d3d_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
      d3d->windowClass.lpfnWndProc = wnd_proc_d3d_winraw;
#endif
   win32_window_init(&d3d->windowClass, true, NULL);
#endif

#ifdef HAVE_MONITOR
   win32_monitor_info(&current_mon, &hm_to_use, &d3d->cur_mon_id);

   mon_rect              = current_mon.rcMonitor;
   g_win32_resize_width  = info->width;
   g_win32_resize_height = info->height;

   windowed_full         = settings->bools.video_windowed_fullscreen;

   full_x                = (windowed_full || info->width  == 0) ?
      (mon_rect.right  - mon_rect.left) : info->width;
   full_y                = (windowed_full || info->height == 0) ?
      (mon_rect.bottom - mon_rect.top)  : info->height;
#else
   d3d9_get_video_size(d3d, &full_x, &full_y);
#endif
   {
      unsigned new_width  = info->fullscreen ? full_x : info->width;
      unsigned new_height = info->fullscreen ? full_y : info->height;
      video_driver_set_size(new_width, new_height);
   }

#ifdef HAVE_WINDOW
   video_driver_get_size(&win_width, &win_height);

   win32_set_style(&current_mon, &hm_to_use, &win_width, &win_height,
         info->fullscreen, windowed_full, &rect, &mon_rect, &style);

   win32_window_create(d3d, style, &mon_rect, win_width,
         win_height, info->fullscreen);

   win32_set_window(&win_width, &win_height, info->fullscreen,
      windowed_full, &rect);
#endif

   d3d->video_info = *info;

   if (!d3d9_hlsl_initialize(d3d, &d3d->video_info))
      return false;

   d3d9_hlsl_fake_context.get_flags   = d3d9_hlsl_get_flags;
#ifndef _XBOX_
   d3d9_hlsl_fake_context.get_metrics = win32_get_metrics;
#endif
   video_context_driver_set(&d3d9_hlsl_fake_context); 
   {
      const char *shader_preset   = retroarch_get_shader_preset();
      enum rarch_shader_type type = video_shader_parse_type(shader_preset);

      d3d9_hlsl_set_shader(d3d, type, shader_preset);
   }

   d3d_input_driver(settings->arrays.input_joypad_driver,
      settings->arrays.input_joypad_driver, input, input_data);

   {
      char version_str[128];
      D3DADAPTER_IDENTIFIER9 ident = {0};

      IDirect3D9_GetAdapterIdentifier(g_pD3D9, 0, 0, &ident);

      version_str[0] = '\0';

      snprintf(version_str, sizeof(version_str), "%u.%u.%u.%u",
            HIWORD(ident.DriverVersion.HighPart),
            LOWORD(ident.DriverVersion.HighPart),
            HIWORD(ident.DriverVersion.LowPart),
            LOWORD(ident.DriverVersion.LowPart));

      RARCH_LOG("[D3D9]: Using GPU: \"%s\".\n", ident.Description);
      RARCH_LOG("[D3D9]: GPU API Version: %s\n", version_str);

      video_driver_set_gpu_device_string(ident.Description);
      video_driver_set_gpu_api_version_string(version_str);
   }

   return true;
}

static void *d3d9_hlsl_init(const video_info_t *info,
      input_driver_t **input, void **input_data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)calloc(1, sizeof(*d3d));

   if (!d3d)
      return NULL;
   if (!d3d9_initialize_symbols(GFX_CTX_DIRECT3D9_API))
      goto error;

#ifndef _XBOX
   win32_window_reset();
   win32_monitor_init();
#endif

   /* Default values */
   d3d->dev                  = NULL;
   d3d->dev_rotation         = 0;
   d3d->needs_restore        = false;
#ifdef HAVE_OVERLAY
   d3d->overlays_enabled     = false;
#endif
   d3d->should_resize        = false;
   d3d->menu                 = NULL;

   if (!d3d9_hlsl_init_internal(d3d, info, input, input_data))
      goto error;

   d3d->keep_aspect       = info->force_aspect;

   return d3d;

error:
   RARCH_ERR("[D3D9]: Failed to init D3D.\n");
   free(d3d);
   return NULL;
}

static void d3d9_hlsl_free(void *data)
{
   d3d9_video_t   *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_OVERLAY
   d3d9_free_overlays(d3d);
   if (d3d->overlays)
      free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
#endif

   d3d9_free_overlay(d3d, d3d->menu);
   if (d3d->menu)
      free(d3d->menu);
   d3d->menu          = NULL;

   d3d9_hlsl_deinitialize(d3d);

   if (!string_is_empty(d3d->shader_path))
      free(d3d->shader_path);

   IDirect3DDevice9_Release(d3d->dev);
   IDirect3D9_Release(g_pD3D9);
   d3d->shader_path = NULL;
   d3d->dev         = NULL;
   g_pD3D9          = NULL;

   d3d9_deinitialize_symbols();

#ifndef _XBOX
   win32_monitor_from_window();
   win32_destroy_window();
#endif
   free(d3d);
}

static bool d3d9_hlsl_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   D3DVIEWPORT9 screen_vp;
   unsigned i                          = 0;
   d3d9_video_t *d3d                   = (d3d9_video_t*)data;
   unsigned width                      = video_info->width;
   unsigned height                     = video_info->height;
   bool statistics_show                = video_info->statistics_show;
   unsigned black_frame_insertion      = video_info->black_frame_insertion;
   struct font_params *osd_params      = (struct font_params*)
      &video_info->osd_stat_params;
   const char *stat_text               = video_info->stat_text;
   bool menu_is_alive                  = video_info->menu_is_alive;
   bool overlay_behind_menu            = video_info->overlay_behind_menu;
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                 = video_info->widgets_active;
#endif

   if (!frame)
      return true;

   /* We cannot recover in fullscreen. */
   if (d3d->needs_restore)
   {
#ifndef _XBOX
      HWND window = win32_get_window();
      if (IsIconic(window))
         return true;
#endif

      if (!d3d9_hlsl_restore(d3d))
      {
         RARCH_ERR("[D3D9]: Failed to restore.\n");
         return false;
      }
   }

   if (d3d->should_resize)
   {
      hlsl_renderchain_t *_chain = (hlsl_renderchain_t*)d3d->renderchain_data;
      d3d9_renderchain_t *chain  = (d3d9_renderchain_t*)&_chain->chain;

      d3d9_set_viewport(d3d, width, height, false, true);

      if (chain)
         chain->final_viewport   = (D3DVIEWPORT9*)&d3d->final_viewport;

      d3d9_recompute_pass_sizes(chain->dev, chain, d3d);

      d3d->should_resize         = false;
   }

   /* render_chain() only clears out viewport,
    * clear out everything. */
   screen_vp.X      = 0;
   screen_vp.Y      = 0;
   screen_vp.MinZ   = 0;
   screen_vp.MaxZ   = 1;
   screen_vp.Width  = width;
   screen_vp.Height = height;
   IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
   IDirect3DDevice9_Clear(d3d->dev, 0, 0, D3DCLEAR_TARGET,
         0, 1, 0);

   IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev, 0,
         (const float*)&d3d->mvp_transposed, 4);
   hlsl_d3d9_renderchain_render(
         d3d, frame, frame_width, frame_height,
         pitch, d3d->dev_rotation);
   
   if (black_frame_insertion && !d3d->menu->enabled)
   {
      unsigned n;
      for (n = 0; n < video_info->black_frame_insertion; ++n) 
      {   
#ifdef _XBOX
        bool ret = true;
        IDirect3DDevice9_Present(d3d->dev, NULL, NULL, NULL, NULL);
#else
        bool ret = (IDirect3DDevice9_Present(d3d->dev,
                 NULL, NULL, NULL, NULL) != D3DERR_DEVICELOST);
#endif
        if (!ret || d3d->needs_restore)
          return true;
        IDirect3DDevice9_Clear(d3d->dev, 0, 0, D3DCLEAR_TARGET,
              0, 1, 0);
      }
   }   

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled && overlay_behind_menu)
   {
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev, 0,
            (const float*)&d3d->mvp_transposed, 4);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev, 0,
            (const float*)&d3d->mvp_transposed, 4);
      d3d9_overlay_render(d3d, width, height, d3d->menu, false);

      d3d->menu_display.offset = 0;
      IDirect3DDevice9_SetVertexDeclaration(d3d->dev, (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);
      IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
            (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
            0, sizeof(Vertex));
      IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
      menu_driver_frame(menu_is_alive, video_info);
   }
   else if (statistics_show)
   {
      if (osd_params)
      {
         IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
         IDirect3DDevice9_BeginScene(d3d->dev);
         font_driver_render_msg(d3d, stat_text,
               (const struct font_params*)osd_params, NULL);
         IDirect3DDevice9_EndScene(d3d->dev);
      }
   }
#endif

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled && !overlay_behind_menu)
   {
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev, 0,
            (const float*)&d3d->mvp_transposed, 4);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (msg && *msg)
   {
      IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
      IDirect3DDevice9_BeginScene(d3d->dev);
      font_driver_render_msg(d3d, msg, NULL, NULL);
      IDirect3DDevice9_EndScene(d3d->dev);
   }

   win32_update_title();
   IDirect3DDevice9_Present(d3d->dev, NULL, NULL, NULL, NULL);

   return true;
}

static const video_poke_interface_t d3d9_hlsl_poke_interface = {
   d3d9_hlsl_get_flags,
   d3d9_load_texture,
   d3d9_unload_texture,
   d3d9_set_video_mode,
#if defined(_XBOX) || defined(__WINRT__)
   NULL,
#else
   /* UWP does not expose this information easily */
   win32_get_refresh_rate,
#endif
   NULL,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d9_set_aspect_ratio,
   d3d9_apply_state_changes,
   d3d9_set_menu_texture_frame,
   d3d9_set_menu_texture_enable,
   d3d9_set_osd_msg,

   win32_show_cursor,
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL,                         /* get_hw_render_interface */
   NULL,                         /* set_hdr_max_nits */
   NULL,                         /* set_hdr_paper_white_nits */
   NULL,                         /* set_hdr_contrast */
   NULL                          /* set_hdr_expand_gamut */
};

static void d3d9_hlsl_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &d3d9_hlsl_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool d3d9_hlsl_gfx_widgets_enabled(void *data)
{
   return false; /* currently disabled due to memory issues */
}
#endif

static void d3d9_hlsl_set_resize(d3d9_video_t *d3d,
      unsigned new_width, unsigned new_height)
{
   /* No changes? */
   if (     (new_width  == d3d->video_info.width)
         && (new_height == d3d->video_info.height))
      return;

   d3d->video_info.width  = new_width;
   d3d->video_info.height = new_height;
   video_driver_set_size(new_width, new_height);
}

static bool d3d9_hlsl_alive(void *data)
{
   unsigned temp_width   = 0;
   unsigned temp_height  = 0;
   bool ret              = false;
   bool        quit      = false;
   bool        resize    = false;
   d3d9_video_t *d3d     = (d3d9_video_t*)data;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   win32_check_window(NULL, &quit, &resize, &temp_width, &temp_height);

   if (quit)
      d3d->quitting      = quit;

   if (resize)
   {
      d3d->should_resize = true;
      d3d9_hlsl_set_resize(d3d, temp_width, temp_height);
      d3d9_hlsl_restore(d3d);
   }

   ret = !quit;

   if (  temp_width  != 0 &&
         temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   return ret;
}

static void d3d9_hlsl_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
#ifdef _XBOX
   int interval          = 0;
#endif
   d3d9_video_t     *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

#ifdef _XBOX
   if (!state)
      interval           = 1;
#endif

   d3d->video_info.vsync = !state;

#ifdef _XBOX
   IDirect3DDevice9_SetRenderState(d3d->dev,
         D3DRS_PRESENTINTERVAL,
         interval ?
         D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE);
#else
   d3d->needs_restore    = true;
   d3d9_hlsl_restore(d3d);
#endif
}

#ifdef _XBOX
static bool d3d9_hlsl_suppress_screensaver(void *data, bool enable)
{
   return true;
}
#endif

video_driver_t video_d3d9_hlsl = {
   d3d9_hlsl_init,
   d3d9_hlsl_frame,
   d3d9_hlsl_set_nonblock_state,
   d3d9_hlsl_alive,
   NULL,                      /* focus */
#ifdef _XBOX
   d3d9_hlsl_suppress_screensaver,
#else
   win32_suppress_screensaver,
#endif
   d3d9_has_windowed,
   d3d9_hlsl_set_shader,
   d3d9_hlsl_free,
   "d3d9_hlsl",
   d3d9_set_viewport,
   d3d9_set_rotation,
   d3d9_viewport_info,
   d3d9_read_viewport,
   NULL,                      /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d9_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
   d3d9_hlsl_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   d3d9_hlsl_gfx_widgets_enabled
#endif
};
