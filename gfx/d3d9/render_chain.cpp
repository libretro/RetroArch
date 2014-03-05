/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "render_chain.hpp"
#include <string.h>

#ifdef HAVE_CG
#include "render_chain_cg.h"
#endif

RenderChain::~RenderChain()
{
   clear();
   destroy_stock_shader();
   if (tracker)
      state_tracker_free(tracker);
}

RenderChain::RenderChain(const video_info_t *video_info,
      LPDIRECT3DDEVICE dev_,
      CGcontext cgCtx_,
      const D3DVIEWPORT &final_viewport_)
   : dev(dev_), cgCtx(cgCtx_), video_info(*video_info), tracker(NULL),
   final_viewport(final_viewport_), frame_count(0)
{}

bool RenderChain::init(const LinkInfo &info, PixelFormat fmt)
{
   pixel_size = fmt == RGB565 ? 2 : 4;
   if (!create_first_pass(info, fmt))
      return false;
   log_info(info);
   if (!compile_shaders(fStock, vStock, ""))
      return false;

   return true;
}

void RenderChain::clear(void)
{
   for (unsigned i = 0; i < Textures; i++)
   {
      if (prev.tex[i])
         prev.tex[i]->Release();
      if (prev.vertex_buf[i])
         prev.vertex_buf[i]->Release();
   }

   if (passes[0].vertex_decl)
      passes[0].vertex_decl->Release();
   for (unsigned i = 1; i < passes.size(); i++)
   {
      if (passes[i].tex)
         passes[i].tex->Release();
      if (passes[i].vertex_buf)
         passes[i].vertex_buf->Release();
      if (passes[i].vertex_decl)
         passes[i].vertex_decl->Release();
      destroy_shader(i);
   }

   for (unsigned i = 0; i < luts.size(); i++)
   {
      if (luts[i].tex)
         luts[i].tex->Release();
   }

   passes.clear();
   luts.clear();
}

void RenderChain::set_final_viewport(const D3DVIEWPORT& final_viewport)
{
   this->final_viewport = final_viewport;
}

bool RenderChain::set_pass_size(unsigned pass_index, unsigned width, unsigned height)
{
   Pass &pass = passes[pass_index];
   if (width != pass.info.tex_w || height != pass.info.tex_h)
   {
      pass.tex->Release();
      pass.info.tex_w = width;
      pass.info.tex_h = height;

      if (FAILED(dev->CreateTexture(width, height, 1,
         D3DUSAGE_RENDERTARGET,
         passes.back().info.pass->fbo.fp_fbo ? D3DFMT_A32B32G32R32F : D3DFMT_A8R8G8B8,
         D3DPOOL_DEFAULT,
         &pass.tex, NULL)))
         return false;

      dev->SetTexture(0, pass.tex);
      dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      dev->SetTexture(0, NULL);
   }

   return true;
}

bool RenderChain::add_pass(const LinkInfo &info)
{
   Pass pass;
   pass.info = info;
   pass.last_width = 0;
   pass.last_height = 0;

   compile_shaders(pass.fPrg, pass.vPrg, info.pass->source.cg);
   if (!init_shader_fvf(pass))
      return false;

   if (FAILED(dev->CreateVertexBuffer(
               4 * sizeof(Vertex),
               dev->GetSoftwareVertexProcessing() ? D3DUSAGE_SOFTWAREPROCESSING : 0,
               0,
               D3DPOOL_DEFAULT,
               &pass.vertex_buf,
               NULL)))
      return false;

   if (FAILED(dev->CreateTexture(info.tex_w, info.tex_h, 1,
               D3DUSAGE_RENDERTARGET,
               passes.back().info.pass->fbo.fp_fbo ? D3DFMT_A32B32G32R32F : D3DFMT_A8R8G8B8,
               D3DPOOL_DEFAULT,
               &pass.tex, NULL)))
      return false;

   dev->SetTexture(0, pass.tex);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   dev->SetTexture(0, NULL);

   passes.push_back(pass);

   log_info(info);
   return true;
}

bool RenderChain::add_lut(const std::string &id,
      const std::string &path,
      bool smooth)
{
   LPDIRECT3DTEXTURE lut;

   RARCH_LOG("[D3D]: Loading LUT texture: %s.\n", path.c_str());

   if (FAILED(D3DXCreateTextureFromFileExA(
               dev,
               path.c_str(),
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
               NULL,
               &lut)))
      return false;

   dev->SetTexture(0, lut);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   dev->SetTexture(0, NULL);

   lut_info info = { lut, id, smooth };
   luts.push_back(info);
   return true;
}

void RenderChain::add_state_tracker(state_tracker_t *tracker)
{
   if (this->tracker)
      state_tracker_free(this->tracker);
   this->tracker = tracker;
}

void RenderChain::start_render(void)
{
   passes[0].tex = prev.tex[prev.ptr];
   passes[0].vertex_buf = prev.vertex_buf[prev.ptr];
   passes[0].last_width = prev.last_width[prev.ptr];
   passes[0].last_height = prev.last_height[prev.ptr];
}

void RenderChain::end_render(void)
{
   prev.last_width[prev.ptr] = passes[0].last_width;
   prev.last_height[prev.ptr] = passes[0].last_height;
   prev.ptr = (prev.ptr + 1) & TexturesMask;
}

bool RenderChain::render(const void *data,
      unsigned width, unsigned height, unsigned pitch, unsigned rotation)
{
   start_render();

   unsigned current_width = width, current_height = height;
   unsigned out_width = 0, out_height = 0;
   convert_geometry(passes[0].info, out_width, out_height,
         current_width, current_height, final_viewport);

   blit_to_texture(data, width, height, pitch);

   // Grab back buffer.
   LPDIRECT3DSURFACE back_buffer;
   dev->GetRenderTarget(0, &back_buffer);

   // In-between render target passes.
   for (unsigned i = 0; i < passes.size() - 1; i++)
   {
      Pass &from_pass = passes[i];
      Pass &to_pass = passes[i + 1];

      LPDIRECT3DSURFACE target;
      to_pass.tex->GetSurfaceLevel(0, &target);
      dev->SetRenderTarget(0, target);

      convert_geometry(from_pass.info,
            out_width, out_height,
            current_width, current_height, final_viewport);

      // Clear out whole FBO.
      D3DVIEWPORT viewport = {0};
      viewport.Width = to_pass.info.tex_w;
      viewport.Height = to_pass.info.tex_h;
      viewport.MinZ = 0.0f;
      viewport.MaxZ = 1.0f;
      dev->SetViewport(&viewport);
      dev->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);
      
      viewport.Width = out_width;
      viewport.Height = out_height;
      set_viewport(viewport);

      set_vertices(from_pass,
            current_width, current_height,
            out_width, out_height,
            out_width, out_height, 0);

      render_pass(from_pass, i + 1);

      current_width = out_width;
      current_height = out_height;
      target->Release();
   }

   // Final pass
   dev->SetRenderTarget(0, back_buffer);
   Pass &last_pass = passes.back();

   convert_geometry(last_pass.info,
         out_width, out_height,
         current_width, current_height, final_viewport);
   set_viewport(final_viewport);
   set_vertices(last_pass,
            current_width, current_height,
            out_width, out_height,
            final_viewport.Width, final_viewport.Height,
            rotation);
   render_pass(last_pass, passes.size());

   frame_count++;

   back_buffer->Release();

   end_render();
   set_shaders(fStock, vStock);
   set_mvp(vStock, final_viewport.Width, final_viewport.Height, 0);
   return true;
}

D3DTEXTUREFILTERTYPE RenderChain::translate_filter(enum gfx_filter_type type)
{
   if (type == RARCH_FILTER_UNSPEC)
      return g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT;
   else
      return type == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT;
}

D3DTEXTUREFILTERTYPE RenderChain::translate_filter(bool smooth)
{
   return smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT;
}

bool RenderChain::create_first_pass(const LinkInfo &info, PixelFormat fmt)
{
   D3DXMATRIX ident;
   D3DXMatrixIdentity(&ident);
   dev->SetTransform(D3DTS_WORLD, &ident);
   dev->SetTransform(D3DTS_VIEW, &ident);

   Pass pass;
   pass.info = info;
   pass.last_width = 0;
   pass.last_height = 0;

   prev.ptr = 0;
   for (unsigned i = 0; i < Textures; i++)
   {
      prev.last_width[i] = 0;
      prev.last_height[i] = 0;

      if (FAILED(dev->CreateVertexBuffer(
                  4 * sizeof(Vertex),
                  dev->GetSoftwareVertexProcessing() ? D3DUSAGE_SOFTWAREPROCESSING : 0,
                  0,
                  D3DPOOL_DEFAULT,
                  &prev.vertex_buf[i],
                  NULL)))
      {
         return false;
      }

      if (FAILED(dev->CreateTexture(info.tex_w, info.tex_h, 1, 0,
                  fmt == RGB565 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8,
                  D3DPOOL_MANAGED,
                  &prev.tex[i], NULL)))
      {
         return false;
      }

      dev->SetTexture(0, prev.tex[i]);
      dev->SetSamplerState(0, D3DSAMP_MINFILTER,
            translate_filter(info.pass->filter));
      dev->SetSamplerState(0, D3DSAMP_MAGFILTER,
            translate_filter(info.pass->filter));
      dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      dev->SetTexture(0, NULL);
   }

   compile_shaders(pass.fPrg, pass.vPrg, info.pass->source.cg);
   if (!init_shader_fvf(pass))
      return false;
   passes.push_back(pass);
   return true;
}

void RenderChain::set_vertices(Pass &pass,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   const LinkInfo &info = pass.info;

   if (pass.last_width != width || pass.last_height != height)
   {
      pass.last_width = width;
      pass.last_height = height;

      float _u = static_cast<float>(width) / info.tex_w;
      float _v = static_cast<float>(height) / info.tex_h;
      Vertex vert[4];
      for (unsigned i = 0; i < 4; i++)
      {
         vert[i].z = 0.5f;
         vert[i].r = vert[i].g = vert[i].b = vert[i].a = 1.0f;
      }

      vert[0].x = 0.0f;
      vert[1].x = out_width;
      vert[2].x = 0.0f;
      vert[3].x = out_width;
      vert[0].y = out_height;
      vert[1].y = out_height;
      vert[2].y = 0.0f;
      vert[3].y = 0.0f;

      vert[0].u = 0.0f;
      vert[1].u = _u;
      vert[2].u = 0.0f;
      vert[3].u = _u;
      vert[0].v = 0.0f;
      vert[1].v = 0.0f;
      vert[2].v = _v;
      vert[3].v = _v;

      vert[0].lut_u = 0.0f;
      vert[1].lut_u = 1.0f;
      vert[2].lut_u = 0.0f;
      vert[3].lut_u = 1.0f;
      vert[0].lut_v = 0.0f;
      vert[1].lut_v = 0.0f;
      vert[2].lut_v = 1.0f;
      vert[3].lut_v = 1.0f;

      // Align texels and vertices.
      for (unsigned i = 0; i < 4; i++)
      {
         vert[i].x -= 0.5f;
         vert[i].y += 0.5f;
      }

      void *verts;
      pass.vertex_buf->Lock(0, sizeof(vert), &verts, 0);
      memcpy(verts, vert, sizeof(vert));
      pass.vertex_buf->Unlock();
   }

   set_mvp(pass.vPrg, vp_width, vp_height, rotation);
   set_shader_params(pass,
         width, height,
         info.tex_w, info.tex_h,
         vp_width, vp_height);
}

void RenderChain::set_viewport(const D3DVIEWPORT &vp)
{
   dev->SetViewport(&vp);
}

void RenderChain::set_mvp(CGprogram &vPrg,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   D3DXMATRIX proj, ortho, rot, tmp;
   D3DXMatrixOrthoOffCenterLH(&ortho, 0, vp_width, 0, vp_height, 0, 1);

   if (rotation)
      D3DXMatrixRotationZ(&rot, rotation * (M_PI / 2.0));
   else
      D3DXMatrixIdentity(&rot);

   D3DXMatrixMultiply(&proj, &ortho, &rot);
   D3DXMatrixTranspose(&tmp, &proj);

   set_shader_mvp(vPrg, tmp);
}

void RenderChain::clear_texture(Pass &pass)
{
   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(pass.tex->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
   {
      memset(d3dlr.pBits, 0, pass.info.tex_h * d3dlr.Pitch);
      pass.tex->UnlockRect(0);
   }
}

void RenderChain::convert_geometry(const LinkInfo &info,
      unsigned &out_width, unsigned &out_height,
      unsigned width, unsigned height,
      const D3DVIEWPORT &final_viewport)
{
   switch (info.pass->fbo.type_x)
   {
      case RARCH_SCALE_VIEWPORT:
         out_width = info.pass->fbo.scale_x * final_viewport.Width;
         break;

      case RARCH_SCALE_ABSOLUTE:
         out_width = info.pass->fbo.abs_x;
         break;

      case RARCH_SCALE_INPUT:
         out_width = info.pass->fbo.scale_x * width;
         break;
   }

   switch (info.pass->fbo.type_y)
   {
      case RARCH_SCALE_VIEWPORT:
         out_height = info.pass->fbo.scale_y * final_viewport.Height;
         break;

      case RARCH_SCALE_ABSOLUTE:
         out_height = info.pass->fbo.abs_y;
         break;

      case RARCH_SCALE_INPUT:
         out_height = info.pass->fbo.scale_y * height;
         break;
   }
}

void RenderChain::blit_to_texture(const void *frame,
      unsigned width, unsigned height,
      unsigned pitch)
{
   Pass &first = passes[0];
   if (first.last_width != width || first.last_height != height)
      clear_texture(first);

   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(first.tex->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
   {
      for (unsigned y = 0; y < height; y++)
      {
         const uint8_t *in = reinterpret_cast<const uint8_t*>(frame) + y * pitch;
         uint8_t *out = reinterpret_cast<uint8_t*>(d3dlr.pBits) + y * d3dlr.Pitch;
         memcpy(out, in, width * pixel_size);
      }

      first.tex->UnlockRect(0);
   }
}

void RenderChain::render_pass(Pass &pass, unsigned pass_index)
{
   set_shaders(pass.fPrg, pass.vPrg);
   dev->SetTexture(0, pass.tex);
   dev->SetSamplerState(0, D3DSAMP_MINFILTER,
         translate_filter(pass.info.pass->filter));
   dev->SetSamplerState(0, D3DSAMP_MAGFILTER,
         translate_filter(pass.info.pass->filter));

   dev->SetVertexDeclaration(pass.vertex_decl);
   for (unsigned i = 0; i < 4; i++)
      dev->SetStreamSource(i, pass.vertex_buf, 0, sizeof(Vertex));

   bind_orig(pass);
   bind_prev(pass);
   bind_pass(pass, pass_index);
   bind_luts(pass);
   bind_tracker(pass, pass_index);

   if (SUCCEEDED(dev->BeginScene()))
   {
      dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
      dev->EndScene();
   }

   // So we don't render with linear filter into render targets,
   // which apparently looked odd (too blurry).
   dev->SetSamplerState(0, D3DSAMP_MINFILTER,
         D3DTEXF_POINT);
   dev->SetSamplerState(0, D3DSAMP_MAGFILTER,
         D3DTEXF_POINT);

   unbind_all();
}

void RenderChain::log_info(const LinkInfo &info)
{
   RARCH_LOG("[D3D]: Render pass info:\n");
   RARCH_LOG("\tTexture width: %u\n", info.tex_w);
   RARCH_LOG("\tTexture height: %u\n", info.tex_h);

   RARCH_LOG("\tScale type (X): ");
   switch (info.pass->fbo.type_x)
   {
      case RARCH_SCALE_INPUT:
         RARCH_LOG("Relative @ %fx\n", info.pass->fbo.scale_x);
         break;

      case RARCH_SCALE_VIEWPORT:
         RARCH_LOG("Viewport @ %fx\n", info.pass->fbo.scale_x);
         break;

      case RARCH_SCALE_ABSOLUTE:
         RARCH_LOG("Absolute @ %u px\n", info.pass->fbo.abs_x);
         break;
   }

   RARCH_LOG("\tScale type (Y): ");
   switch (info.pass->fbo.type_y)
   {
      case RARCH_SCALE_INPUT:
         RARCH_LOG("Relative @ %fx\n", info.pass->fbo.scale_y);
         break;

      case RARCH_SCALE_VIEWPORT:
         RARCH_LOG("Viewport @ %fx\n", info.pass->fbo.scale_y);
         break;

      case RARCH_SCALE_ABSOLUTE:
         RARCH_LOG("Absolute @ %u px\n", info.pass->fbo.abs_y);
         break;
   }

   RARCH_LOG("\tBilinear filter: %s\n", info.pass->filter == RARCH_FILTER_LINEAR ? "true" : "false");
}

void RenderChain::unbind_all()
{
   // Have to be a bit anal about it.
   // Render targets hate it when they have filters apparently.
   for (unsigned i = 0; i < bound_tex.size(); i++)
   {
      dev->SetSamplerState(bound_tex[i], D3DSAMP_MAGFILTER,
            D3DTEXF_POINT);
      dev->SetSamplerState(bound_tex[i], D3DSAMP_MINFILTER,
            D3DTEXF_POINT);
      dev->SetTexture(bound_tex[i], NULL);
   }

   for (unsigned i = 0; i < bound_vert.size(); i++)
      dev->SetStreamSource(bound_vert[i], 0, 0, 0);

   bound_tex.clear();
   bound_vert.clear();
}