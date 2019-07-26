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

#include "../video_shader_parse.h"
#include "../../managers/state_manager.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#define RARCH_HLSL_MAX_SHADERS 16

#include "d3d9_renderchain.h"

typedef struct hlsl_renderchain
{
   struct d3d9_renderchain chain;
   struct shader_pass stock_shader;
} hlsl_renderchain_t;

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
   D3DXHANDLE param         = (D3DXHANDLE)d3d9_hlsl_get_constant_by_name(prog, name);
   if (param)
      d3d9x_constant_table_set_float_array((LPDIRECT3DDEVICE9)userdata, prog, (void*)param, values, 2);
}

static INLINE void d3d9_hlsl_set_param_1f(void *data, void *userdata, const char *name, const void *value)
{
   LPD3DXCONSTANTTABLE prog = (LPD3DXCONSTANTTABLE)data;
   D3DXHANDLE param         = (D3DXHANDLE)d3d9_hlsl_get_constant_by_name(prog, name);
   float *val               = (float*)value;
   if (param)
      d3d9x_constant_table_set_float(prog, (LPDIRECT3DDEVICE9)userdata, (void*)param, *val);
}

static INLINE void d3d9_hlsl_bind_program(void *data,
      LPDIRECT3DDEVICE9 dev)
{
   struct shader_pass *pass = (struct shader_pass*)data;
   if (!pass)
      return;
   d3d9_set_vertex_shader(dev, (LPDIRECT3DVERTEXSHADER9)pass->vprg);
   d3d9_set_pixel_shader(dev,  (LPDIRECT3DPIXELSHADER9)pass->fprg);
}

static INLINE void d3d9_hlsl_set_param_matrix(void *data, void *userdata,
      const char *name, const void *values)
{
   LPD3DXCONSTANTTABLE prog = (LPD3DXCONSTANTTABLE)data;
   D3DXHANDLE param         = (D3DXHANDLE)d3d9_hlsl_get_constant_by_name(prog, name);
   if (param)
      d3d9x_constant_table_set_matrix((LPDIRECT3DDEVICE9)userdata, prog,
            (void*)param, (D3DMATRIX*)values);
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

   d3d9_create_pixel_shader(dev,  (const DWORD*)d3d9x_get_buffer_ptr(code_f),  (void**)&pass->fprg);
   d3d9_create_vertex_shader(dev, (const DWORD*)d3d9x_get_buffer_ptr(code_v), (void**)&pass->vprg);
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

   d3d9_create_pixel_shader(dev,  (const DWORD*)d3d9x_get_buffer_ptr(code_f),  (void**)&pass->fprg);
   d3d9_create_vertex_shader(dev, (const DWORD*)d3d9x_get_buffer_ptr(code_v), (void**)&pass->vprg);
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
   void *fprg                               = pass->ftable;
   void *vprg                               = pass->vtable;

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

   d3d9_hlsl_load_program_from_file(chain->dev, &pass, info->pass->source.path);

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

   d3d_matrix_ortho_off_center_lh(&ortho, 0, vp_width, 0, vp_height, 0, 1);
   d3d_matrix_identity(&rot);
   d3d_matrix_rotation_z(&rot, rotation * (D3D_PI / 2.0));

   d3d_matrix_multiply(&proj, &ortho, &rot);
   d3d_matrix_transpose(&matrix, &proj);

   d3d9_hlsl_set_param_matrix(pass->vtable,
         chain->chain.dev, "modelViewProj", (const void*)&matrix);
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
      d3d9_renderchain_set_vertices_on_change(&chain->chain,
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

static void d3d9_hlsl_destroy_resources(hlsl_renderchain_t *chain)
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
   hlsl_renderchain_t *chain = (hlsl_renderchain_t*)data;

   if (!chain)
      return;

   d3d9_hlsl_destroy_resources(chain);
   d3d9_renderchain_destroy_passes_and_luts(&chain->chain);
   free(chain);
}

void *hlsl_d3d9_renderchain_new(void)
{
   hlsl_renderchain_t *renderchain =
      (hlsl_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   d3d9_init_renderchain(&renderchain->chain);

   return renderchain;
}

static bool hlsl_d3d9_renderchain_init_shader(d3d9_video_t *d3d,
      hlsl_renderchain_t *chain)
{
   RARCH_LOG("[D3D9]: Using HLSL shader backend.\n");

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
   hlsl_renderchain_t *chain     = (hlsl_renderchain_t*)
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
   if (!d3d9_hlsl_load_program(chain->chain.dev, &chain->stock_shader, stock_hlsl_program))
      return false;

   d3d9_hlsl_bind_program(&chain->stock_shader, dev);

   return true;
}

static void hlsl_d3d9_renderchain_set_final_viewport(
      d3d9_video_t *d3d,
      void *renderchain_data,
      const D3DVIEWPORT9 *final_viewport)
{
   hlsl_renderchain_t      *_chain = (hlsl_renderchain_t*)renderchain_data;
   d3d9_renderchain_t      *chain  = (d3d9_renderchain_t*)&_chain->chain;

   if (chain && final_viewport)
      chain->final_viewport = (D3DVIEWPORT9*)final_viewport;

   d3d9_recompute_pass_sizes(chain->dev, chain, d3d);
}

static void hlsl_d3d9_renderchain_render_pass(
      hlsl_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned pass_index)
{
   unsigned i;

   d3d9_hlsl_bind_program(pass, chain->chain.dev);

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
      d3d9_hlsl_renderchain_bind_pass(chain, chain->chain.dev, pass, pass_index);

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
      const void *frame,
      unsigned width, unsigned height,
      unsigned pitch, unsigned rotation)
{
   LPDIRECT3DSURFACE9 back_buffer, target;
   unsigned i, current_width, current_height, out_width = 0, out_height = 0;
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

      d3d9_device_set_render_target(chain->chain.dev, 0, target);

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

      current_width = out_width;
      current_height = out_height;
      d3d9_surface_free(target);
   }

   /* Final pass */
   d3d9_device_set_render_target(chain->chain.dev, 0, back_buffer);

   last_pass = (struct shader_pass*)&chain->chain.passes->
      data[chain->chain.passes->count - 1];

   d3d9_convert_geometry(&last_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->chain.final_viewport);

   d3d9_set_viewports(chain->chain.dev, chain->chain.final_viewport);

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

   d3d9_surface_free(back_buffer);

   d3d9_renderchain_end_render(&chain->chain);
   d3d9_hlsl_bind_program(&chain->stock_shader, chain->chain.dev);
   hlsl_d3d9_renderchain_calc_and_set_shader_mvp(chain, &chain->stock_shader,
         chain->chain.final_viewport->Width,
         chain->chain.final_viewport->Height, 0);

   return true;
}

static bool hlsl_d3d9_renderchain_add_pass(
      void *data, const struct LinkInfo *info)
{
   struct shader_pass pass;
   hlsl_renderchain_t *chain   = (hlsl_renderchain_t*)data;

   pass.info                   = *info;
   pass.last_width             = 0;
   pass.last_height            = 0;
   pass.attrib_map             = (struct unsigned_vector_list*)
      unsigned_vector_list_new();
   pass.pool                   = D3DPOOL_DEFAULT;

   d3d9_hlsl_load_program_from_file(chain->chain.dev, &pass, info->pass->source.path);

   if (!hlsl_d3d9_renderchain_init_shader_fvf(&chain->chain, &pass))
      return false;

   return d3d9_renderchain_add_pass(&chain->chain, &pass,
         info);
}

static bool hlsl_d3d9_renderchain_add_lut(void *data,
      const char *id, const char *path, bool smooth)
{
   hlsl_renderchain_t *_chain  = (hlsl_renderchain_t*)data;
   d3d9_renderchain_t *chain   = (d3d9_renderchain_t*)&_chain->chain;

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
