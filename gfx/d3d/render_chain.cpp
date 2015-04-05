/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "render_chain.h"
#include <string.h>
#include <retro_inline.h>

struct lut_info
{
   LPDIRECT3DTEXTURE tex;
   char id[64];
   bool smooth;
};

typedef struct renderchain
{
   LPDIRECT3DDEVICE dev;
#ifdef HAVE_CG
   CGcontext cgCtx;
#endif
   unsigned pixel_size;
   const video_info_t *video_info;
   state_tracker_t *tracker;
   struct state_tracker_uniform uniform_info[MAX_VARIABLES];
   unsigned uniform_cnt;
   struct
   {
      LPDIRECT3DTEXTURE tex[TEXTURES];
      LPDIRECT3DVERTEXBUFFER vertex_buf[TEXTURES];
      unsigned ptr;
      unsigned last_width[TEXTURES];
      unsigned last_height[TEXTURES];
   } prev;
   std::vector<Pass> passes;
#ifdef HAVE_CG
   CGprogram vStock, fStock;
#endif
   std::vector<lut_info> luts;
   D3DVIEWPORT *final_viewport;
   unsigned frame_count;
   std::vector<unsigned> bound_tex;
   std::vector<unsigned> bound_vert;
} renderchain_t;

static INLINE D3DTEXTUREFILTERTYPE translate_filter(unsigned type)
{
   settings_t *settings = config_get_ptr();

   switch (type)
   {
		case RARCH_FILTER_UNSPEC:
            return settings->video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT;
		case RARCH_FILTER_LINEAR:
			return D3DTEXF_LINEAR;
		case RARCH_FILTER_NEAREST:
			return D3DTEXF_POINT;
   }
   
   return D3DTEXF_POINT;
}

static INLINE D3DTEXTUREFILTERTYPE translate_filter(bool smooth)
{
   if (smooth)
      return D3DTEXF_LINEAR;
   return D3DTEXF_POINT;
}

#ifdef HAVE_CG
#include "render_chain_cg.h"
#endif

void renderchain_free(void *data)
{
   renderchain_t *chain = (renderchain_t*)data;

   if (!chain)
      return;

   renderchain_clear(chain);
   renderchain_destroy_stock_shader(chain);
   if (chain->tracker)
      state_tracker_free(chain->tracker);
}

void *renderchain_new(void)
{
   renderchain_t *renderchain = new renderchain_t();
   if (!renderchain)
      return NULL;

   return renderchain;
}

void renderchain_deinit(void *data)
{
   renderchain_t *renderchain = (renderchain_t*)data;

   if (!renderchain)
      return;

   delete (renderchain_t *)renderchain;
}

bool renderchain_init(void *data, const video_info_t *video_info,
      void *dev_,
      void *shader_context,
      const void *final_viewport_,
      const void *info_data, PixelFormat fmt)
{
   const LinkInfo *info  = (const LinkInfo*)info_data;
   renderchain_t *chain  = (renderchain_t*)data;
   CGcontext cgCtx_      = (CGcontext)shader_context;

   if (!chain)
      return false;

   chain->dev            = (LPDIRECT3DDEVICE)dev_;
#ifdef HAVE_CG
   chain->cgCtx          = cgCtx_;
#endif
   chain->video_info     = video_info;
   chain->tracker        = NULL;
   chain->final_viewport =  (D3DVIEWPORT*)final_viewport_;
   chain->frame_count    = 0;
   chain->pixel_size     = (fmt == RGB565) ? 2 : 4;

   if (!renderchain_create_first_pass(chain, info, fmt))
      return false;
   renderchain_log_info(chain, info);
   if (!renderchain_compile_shaders(chain, &chain->fStock, &chain->vStock, ""))
      return false;

   return true;
}

void renderchain_clear(void *data)
{
   unsigned i;
   renderchain_t *chain = (renderchain_t*)data;

   for (i = 0; i < TEXTURES; i++)
   {
      if (chain->prev.tex[i])
         d3d_texture_free(chain->prev.tex[i]);
      if (chain->prev.vertex_buf[i])
         d3d_vertex_buffer_free(chain->prev.vertex_buf[i]);
   }

   if (chain->passes[0].vertex_decl)
      chain->passes[0].vertex_decl->Release();

   for (i = 1; i < chain->passes.size(); i++)
   {
      if (chain->passes[i].tex)
         d3d_texture_free(chain->passes[i].tex);
      if (chain->passes[i].vertex_buf)
         d3d_vertex_buffer_free(chain->passes[i].vertex_buf);
      if (chain->passes[i].vertex_decl)
         chain->passes[i].vertex_decl->Release();
      renderchain_destroy_shader(chain, i);
   }

   for (i = 0; i < chain->luts.size(); i++)
   {
      if (chain->luts[i].tex)
         d3d_texture_free(chain->luts[i].tex);
   }

   chain->passes.clear();
   chain->luts.clear();
}

void renderchain_set_final_viewport(void *data, const void *viewport_data)
{
   renderchain_t *chain = (renderchain_t*)data;
   const D3DVIEWPORT *final_viewport = (const D3DVIEWPORT*)viewport_data;

   if (chain)
      chain->final_viewport = (D3DVIEWPORT*)final_viewport;
}

bool renderchain_set_pass_size(void *data, unsigned pass_index,
      unsigned width, unsigned height)
{
   renderchain_t *chain = (renderchain_t*)data;
   LPDIRECT3DDEVICE d3dr = chain->dev;
   Pass *pass = (Pass*)&chain->passes[pass_index];

   if (width != pass->info.tex_w || height != pass->info.tex_h)
   {
      d3d_texture_free(pass->tex);

      pass->info.tex_w = width;
      pass->info.tex_h = height;
      pass->tex        = (LPDIRECT3DTEXTURE)d3d_texture_new(
      d3dr, NULL, width, height, 1,
         D3DUSAGE_RENDERTARGET,
         chain->passes.back().info.pass->fbo.fp_fbo ? 
         D3DFMT_A32B32G32R32F : D3DFMT_A8R8G8B8,
         D3DPOOL_DEFAULT, 0, 0, 0,
         NULL, NULL);
      
      if (!pass->tex)
         return false;

      d3d_set_texture(d3dr, 0, pass->tex);
      d3d_set_sampler_address_u(d3dr, 0, D3DTADDRESS_BORDER);
      d3d_set_sampler_address_v(d3dr, 0, D3DTADDRESS_BORDER);
      d3d_set_texture(d3dr, 0, NULL);
   }

   return true;
}

bool renderchain_add_pass(void *data, const void *info_data)
{
   Pass pass;
   const LinkInfo *info  = (const LinkInfo*)info_data;
   renderchain_t *chain  = (renderchain_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)chain->dev;

   pass.info             = *info;
   pass.last_width       = 0;
   pass.last_height      = 0;

   renderchain_compile_shaders(chain, &pass.fPrg, 
        &pass.vPrg, info->pass->source.path);

   if (!renderchain_init_shader_fvf(chain, &pass))
      return false;

   pass.vertex_buf = (LPDIRECT3DVERTEXBUFFER)d3d_vertex_buffer_new(d3dr, 4 * sizeof(Vertex),
	   d3dr->GetSoftwareVertexProcessing() ? D3DUSAGE_SOFTWAREPROCESSING : 0,
	   0, D3DPOOL_DEFAULT, NULL);

   if (!pass.vertex_buf)
      return false;

   pass.tex = d3d_texture_new(d3dr, NULL, info->tex_w, info->tex_h, 1,
               D3DUSAGE_RENDERTARGET,
               chain->passes.back().info.pass->fbo.fp_fbo 
               ? D3DFMT_A32B32G32R32F : D3DFMT_A8R8G8B8,
               D3DPOOL_DEFAULT, 0, 0, 0, NULL, NULL);

   if (!pass.tex)
      return false;

   d3d_set_texture(d3dr, 0, pass.tex);
   d3d_set_sampler_address_u(d3dr, 0, D3DTADDRESS_BORDER);
   d3d_set_sampler_address_v(d3dr, 0, D3DTADDRESS_BORDER);
   d3d_set_texture(d3dr, 0, NULL);

   chain->passes.push_back(pass);

   renderchain_log_info(chain, info);
   return true;
}

bool renderchain_add_lut(void *data, const char *id,
      const char *path,
      bool smooth)
{
   lut_info info;
   renderchain_t *chain = (renderchain_t*)data;
   LPDIRECT3DDEVICE d3dr = chain->dev;
   LPDIRECT3DTEXTURE lut = (LPDIRECT3DTEXTURE)
      d3d_texture_new(d3dr,
            path,
            D3DX_DEFAULT_NONPOW2,
            D3DX_DEFAULT_NONPOW2,
            0,
            0,
            D3DFMT_FROM_FILE,
            D3DPOOL_MANAGED,
            smooth ? D3DX_FILTER_LINEAR : D3DX_FILTER_POINT,
            0,
            0,
            NULL,
            NULL
            );

   RARCH_LOG("[D3D]: LUT texture loaded: %s.\n", path);

   info.tex    = lut;
   info.smooth = smooth;
   strcpy(info.id, id);
   if (!lut)
      return false;

   d3d_set_texture(d3dr, 0, lut);
   d3d_set_sampler_address_u(d3dr, 0, D3DTADDRESS_BORDER);
   d3d_set_sampler_address_v(d3dr, 0, D3DTADDRESS_BORDER);
   d3d_set_texture(d3dr, 0, NULL);

   chain->luts.push_back(info);
   return true;
}

void renderchain_add_state_tracker(void *data, void *tracker_data)
{
   state_tracker_t *tracker = (state_tracker_t*)tracker_data;
   renderchain_t     *chain = (renderchain_t*)data;
   if (chain->tracker)
      state_tracker_free(chain->tracker);
   chain->tracker = tracker;
}

void renderchain_start_render(void *data)
{
   renderchain_t *chain         = (renderchain_t*)data;

   if (!chain)
      return;

   chain->passes[0].tex         = chain->prev.tex[chain->prev.ptr];
   chain->passes[0].vertex_buf  = chain->prev.vertex_buf[chain->prev.ptr];
   chain->passes[0].last_width  = chain->prev.last_width[chain->prev.ptr];
   chain->passes[0].last_height = chain->prev.last_height[chain->prev.ptr];
}

void renderchain_end_render(void *data)
{
   renderchain_t *chain                     = (renderchain_t*)data;

   if (!chain)
      return;

   chain->prev.last_width[chain->prev.ptr]  = chain->passes[0].last_width;
   chain->prev.last_height[chain->prev.ptr] = chain->passes[0].last_height;
   chain->prev.ptr                          = (chain->prev.ptr + 1) & TEXTURESMASK;
}

bool renderchain_render(void *chain_data, const void *data,
      unsigned width, unsigned height, unsigned pitch, unsigned rotation)
{
   Pass *last_pass;
   LPDIRECT3DSURFACE back_buffer, target;
   unsigned i, current_width, current_height, out_width = 0, out_height = 0;
   renderchain_t *chain  = (renderchain_t*)chain_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)chain->dev;
   global_t *global      = global_get_ptr();

   renderchain_start_render(chain);

   current_width         = width;
   current_height        = height;
   renderchain_convert_geometry(chain, &chain->passes[0].info,
         &out_width, &out_height,
         current_width, current_height, chain->final_viewport);

#ifdef _XBOX1
   d3dr->SetFlickerFilter(global->console.screen.flicker_filter_index);
   d3dr->SetSoftDisplayFilter(global->console.softfilter_enable);
#endif
   renderchain_blit_to_texture(chain, data, width, height, pitch);

   /* Grab back buffer. */
   d3dr->GetRenderTarget(0, &back_buffer);

   /* In-between render target passes. */
   for (i = 0; i < chain->passes.size() - 1; i++)
   {
      D3DVIEWPORT viewport = {0};
      Pass *from_pass = (Pass*)&chain->passes[i];
      Pass *to_pass   = (Pass*)&chain->passes[i + 1];

      to_pass->tex->GetSurfaceLevel(0, &target);
      d3dr->SetRenderTarget(0, target);

      renderchain_convert_geometry(chain, &from_pass->info,
            &out_width, &out_height,
            current_width, current_height, chain->final_viewport);

      /* Clear out whole FBO. */
      viewport.Width  = to_pass->info.tex_w;
      viewport.Height = to_pass->info.tex_h;
      viewport.MinZ   = 0.0f;
      viewport.MaxZ   = 1.0f;
      d3d_set_viewport(d3dr, &viewport);
      d3d_clear(d3dr, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);
      
      viewport.Width  = out_width;
      viewport.Height = out_height;
      renderchain_set_viewport(chain, &viewport);

      renderchain_set_vertices(chain, from_pass,
            current_width, current_height,
            out_width, out_height,
            out_width, out_height, 0);

      renderchain_render_pass(chain, from_pass, i + 1);

      current_width = out_width;
      current_height = out_height;
      target->Release();
   }

   /* Final pass */
   d3dr->SetRenderTarget(0, back_buffer);

   last_pass = (Pass*)&chain->passes.back();

   renderchain_convert_geometry(chain, &last_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->final_viewport);
   renderchain_set_viewport(chain, chain->final_viewport);
   renderchain_set_vertices(chain, last_pass,
            current_width, current_height,
            out_width, out_height,
            chain->final_viewport->Width, chain->final_viewport->Height,
            rotation);
   renderchain_render_pass(chain, last_pass, chain->passes.size());

   chain->frame_count++;

   back_buffer->Release();

   renderchain_end_render(chain);
   renderchain_set_shaders(chain, &chain->fStock, &chain->vStock);
   renderchain_set_mvp(chain, chain->vStock, chain->final_viewport->Width,
         chain->final_viewport->Height, 0);

   return true;
}

bool renderchain_create_first_pass(void *data, const void *info_data,
      PixelFormat fmt)
{
   unsigned i;
   Pass pass;
   D3DXMATRIX ident;
   const LinkInfo *info  = (const LinkInfo*)info_data;
   renderchain_t *chain  = (renderchain_t*)data;
   LPDIRECT3DDEVICE d3dr = NULL;

   if (!chain)
	   return false;
   
   d3dr = (LPDIRECT3DDEVICE)chain->dev;

   D3DXMatrixIdentity(&ident);

   d3d_set_transform(d3dr, D3DTS_WORLD, &ident);
   d3d_set_transform(d3dr, D3DTS_VIEW, &ident);

   pass.info        = *info;
   pass.last_width  = 0;
   pass.last_height = 0;

   chain->prev.ptr  = 0;

   for (i = 0; i < TEXTURES; i++)
   {
      chain->prev.last_width[i]  = 0;
      chain->prev.last_height[i] = 0;
      chain->prev.vertex_buf[i]  = d3d_vertex_buffer_new(
            d3dr, 4 * sizeof(Vertex),
            d3dr->GetSoftwareVertexProcessing() 
            ? D3DUSAGE_SOFTWAREPROCESSING : 0,
            0,
            D3DPOOL_DEFAULT,
            NULL);

      if (!chain->prev.vertex_buf[i])
         return false;

      chain->prev.tex[i] = (LPDIRECT3DTEXTURE)d3d_texture_new(
      d3dr, NULL, info->tex_w, info->tex_h, 1, 0,
      fmt == RGB565 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8,
      D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL);

      if (!chain->prev.tex[i])
         return false;

      d3d_set_texture(d3dr, 0, chain->prev.tex[i]);
      d3d_set_sampler_minfilter(d3dr, 0,
            translate_filter(info->pass->filter));
      d3d_set_sampler_magfilter(d3dr, 0,
            translate_filter(info->pass->filter));
      d3d_set_sampler_address_u(d3dr, 0, D3DTADDRESS_BORDER);
      d3d_set_sampler_address_v(d3dr, 0, D3DTADDRESS_BORDER);
      d3d_set_texture(d3dr, 0, NULL);
   }

   renderchain_compile_shaders(chain, &pass.fPrg,
         &pass.vPrg, info->pass->source.path);

   if (!renderchain_init_shader_fvf(chain, &pass))
      return false;
   chain->passes.push_back(pass);
   return true;
}

void renderchain_set_vertices(
	  void *data, void *pass_data,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   Pass          *pass  = (Pass*)pass_data;
   renderchain_t *chain = (renderchain_t*)data;
   const LinkInfo *info = (const LinkInfo*)&pass->info;

   if (pass->last_width != width || pass->last_height != height)
   {
      Vertex vert[4];
      unsigned i;
      void *verts       = NULL;
      float _u          = float(width)  / info->tex_w;
      float _v          = float(height) / info->tex_h;

      pass->last_width  = width;
      pass->last_height = height;

      for (i = 0; i < 4; i++)
      {
         vert[i].z      = 0.5f;
         vert[i].r      = vert[i].g = vert[i].b = vert[i].a = 1.0f;
      }

      vert[0].x         = 0.0f;
      vert[1].x         = out_width;
      vert[2].x         = 0.0f;
      vert[3].x         = out_width;
      vert[0].y         = out_height;
      vert[1].y         = out_height;
      vert[2].y         = 0.0f;
      vert[3].y         = 0.0f;

      vert[0].u         = 0.0f;
      vert[1].u         = _u;
      vert[2].u         = 0.0f;
      vert[3].u         = _u;
      vert[0].v         = 0.0f;
      vert[1].v         = 0.0f;
      vert[2].v         = _v;
      vert[3].v         = _v;

      vert[0].lut_u     = 0.0f;
      vert[1].lut_u     = 1.0f;
      vert[2].lut_u     = 0.0f;
      vert[3].lut_u     = 1.0f;
      vert[0].lut_v     = 0.0f;
      vert[1].lut_v     = 0.0f;
      vert[2].lut_v     = 1.0f;
      vert[3].lut_v     = 1.0f;

      /* Align texels and vertices. */
      for (i = 0; i < 4; i++)
      {
         vert[i].x     -= 0.5f;
         vert[i].y     += 0.5f;
      }

      verts             = d3d_vertex_buffer_lock(pass->vertex_buf);
      memcpy(verts, vert, sizeof(vert));
      d3d_vertex_buffer_unlock(pass->vertex_buf);
   }

   renderchain_set_mvp(chain, pass->vPrg, vp_width, vp_height, rotation);
   renderchain_set_shader_params(chain, pass,
         width, height,
         info->tex_w, info->tex_h,
         vp_width, vp_height);
}

void renderchain_set_viewport(void *data, void *viewport_data)
{
   LPDIRECT3DDEVICE d3dr;
   D3DVIEWPORT       *vp = (D3DVIEWPORT*)viewport_data;
   renderchain_t  *chain = (renderchain_t*)data;

   if (!chain)
      return;

   d3dr = (LPDIRECT3DDEVICE)chain->dev;
   d3d_set_viewport(d3dr, vp);
}

void renderchain_set_mvp(void *data, void *vertex_program,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   D3DXMATRIX proj, ortho, rot, tmp;
   CGprogram     vPrg   = (CGprogram)vertex_program;
   renderchain_t *chain = (renderchain_t*)data;

   if (!chain)
      return;

   D3DXMatrixOrthoOffCenterLH(&ortho, 0, vp_width, 0, vp_height, 0, 1);

   if (rotation)
      D3DXMatrixRotationZ(&rot, rotation * (M_PI / 2.0));
   else
      D3DXMatrixIdentity(&rot);

   D3DXMatrixMultiply(&proj, &ortho, &rot);
   D3DXMatrixTranspose(&tmp, &proj);

   renderchain_set_shader_mvp(chain, &vPrg, &tmp);
}

void renderchain_convert_geometry(
	  void *data, const void *info_data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height,
      D3DVIEWPORT *final_viewport)
{
   const LinkInfo *info = (const LinkInfo*)info_data;
   renderchain_t *chain = (renderchain_t*)data;

   if (!chain || !info)
      return;

   switch (info->pass->fbo.type_x)
   {
      case RARCH_SCALE_VIEWPORT:
         *out_width = info->pass->fbo.scale_x * final_viewport->Width;
         break;

      case RARCH_SCALE_ABSOLUTE:
         *out_width = info->pass->fbo.abs_x;
         break;

      case RARCH_SCALE_INPUT:
         *out_width = info->pass->fbo.scale_x * width;
         break;
   }

   switch (info->pass->fbo.type_y)
   {
      case RARCH_SCALE_VIEWPORT:
         *out_height = info->pass->fbo.scale_y * final_viewport->Height;
         break;

      case RARCH_SCALE_ABSOLUTE:
         *out_height = info->pass->fbo.abs_y;
         break;

      case RARCH_SCALE_INPUT:
         *out_height = info->pass->fbo.scale_y * height;
         break;
   }
}

void renderchain_blit_to_texture(void *data, const void *frame,
      unsigned width, unsigned height,
      unsigned pitch)
{
   D3DLOCKED_RECT d3dlr;
   renderchain_t *chain = (renderchain_t*)data;
   Pass *first          = (Pass*)&chain->passes[0];
   driver_t *driver     = driver_get_ptr();

   if (first->last_width != width || first->last_height != height)
   {
      d3d_lockrectangle_clear(first, first->tex, 0, &d3dlr, 
            NULL, D3DLOCK_NOSYSLOCK);
   }

   d3d_texture_blit(driver->video_data, chain->pixel_size, first->tex,
      &d3dlr, frame, width, height, pitch);
}

void renderchain_render_pass(void *data, void *pass_data, unsigned pass_index)
{
   unsigned i;
   Pass           *pass  = (Pass*)pass_data;
   renderchain_t *chain  = (renderchain_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)chain->dev;

   renderchain_set_shaders(chain, &pass->fPrg, &pass->vPrg);

   d3d_set_texture(d3dr, 0, pass->tex);
   d3d_set_sampler_minfilter(d3dr, 0,
         translate_filter(pass->info.pass->filter));
   d3d_set_sampler_magfilter(d3dr, 0,
         translate_filter(pass->info.pass->filter));

#ifdef _XBOX1
   d3d_set_vertex_shader(d3dr, D3DFVF_XYZ | D3DFVF_TEX1, NULL);
#else
   d3dr->SetVertexDeclaration(pass->vertex_decl);
#endif
   for (i = 0; i < 4; i++)
      d3d_set_stream_source(d3dr, i,
            pass->vertex_buf, 0, sizeof(Vertex));

   renderchain_bind_orig(chain, pass);
   renderchain_bind_prev(chain, pass);
   renderchain_bind_pass(chain, pass, pass_index);
   renderchain_bind_luts(chain, pass);
   renderchain_bind_tracker(chain, pass, pass_index);

   d3d_draw_primitive(d3dr, D3DPT_TRIANGLESTRIP, 0, 2);

   /* So we don't render with linear filter into render targets,
    * which apparently looked odd (too blurry). */
   d3d_set_sampler_minfilter(d3dr, 0, D3DTEXF_POINT);
   d3d_set_sampler_magfilter(d3dr, 0, D3DTEXF_POINT);

   renderchain_unbind_all(chain);
}

void renderchain_log_info(void *data, const void *info_data)
{
   const LinkInfo *info = (const LinkInfo*)info_data;
   RARCH_LOG("[D3D]: Render pass info:\n");
   RARCH_LOG("\tTexture width: %u\n", info->tex_w);
   RARCH_LOG("\tTexture height: %u\n", info->tex_h);

   RARCH_LOG("\tScale type (X): ");

   switch (info->pass->fbo.type_x)
   {
      case RARCH_SCALE_INPUT:
         RARCH_LOG("Relative @ %fx\n", info->pass->fbo.scale_x);
         break;

      case RARCH_SCALE_VIEWPORT:
         RARCH_LOG("Viewport @ %fx\n", info->pass->fbo.scale_x);
         break;

      case RARCH_SCALE_ABSOLUTE:
         RARCH_LOG("Absolute @ %u px\n", info->pass->fbo.abs_x);
         break;
   }

   RARCH_LOG("\tScale type (Y): ");

   switch (info->pass->fbo.type_y)
   {
      case RARCH_SCALE_INPUT:
         RARCH_LOG("Relative @ %fx\n", info->pass->fbo.scale_y);
         break;

      case RARCH_SCALE_VIEWPORT:
         RARCH_LOG("Viewport @ %fx\n", info->pass->fbo.scale_y);
         break;

      case RARCH_SCALE_ABSOLUTE:
         RARCH_LOG("Absolute @ %u px\n", info->pass->fbo.abs_y);
         break;
   }

   RARCH_LOG("\tBilinear filter: %s\n",
         info->pass->filter == RARCH_FILTER_LINEAR ? "true" : "false");
}

void renderchain_unbind_all(void *data)
{
   unsigned i;
   renderchain_t *chain  = (renderchain_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)chain->dev;

   /* Have to be a bit anal about it.
    * Render targets hate it when they have filters apparently.
    */
   for (i = 0; i < chain->bound_tex.size(); i++)
   {
      d3d_set_sampler_minfilter(d3dr,
            chain->bound_tex[i], D3DTEXF_POINT);
      d3d_set_sampler_magfilter(d3dr,
            chain->bound_tex[i], D3DTEXF_POINT);
      d3d_set_texture(d3dr, chain->bound_tex[i], NULL);
   }

   for (i = 0; i < chain->bound_vert.size(); i++)
      d3d_set_stream_source(d3dr, chain->bound_vert[i], 0, 0, 0);

   chain->bound_tex.clear();
   chain->bound_vert.clear();
}
