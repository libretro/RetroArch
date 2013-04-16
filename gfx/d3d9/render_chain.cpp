/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include <utility>

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <cstdio>

namespace Global
{
   static const char *stock_program =
      "void main_vertex"
      "("
      "	float4 position : POSITION,"
      "	float2 texCoord : TEXCOORD0,"
      "  float4 color : COLOR,"
      ""
      "  uniform float4x4 modelViewProj,"
      ""
      "	out float4 oPosition : POSITION,"
      "	out float2 otexCoord : TEXCOORD0,"
      "  out float4 oColor : COLOR"
      ")"
      "{"
      "	oPosition = mul(modelViewProj, position);"
      "	otexCoord = texCoord;"
      "  oColor = color;"
      "}"
      ""
      "float4 main_fragment(in float4 color : COLOR, float2 tex : TEXCOORD0, uniform sampler2D s0 : TEXUNIT0) : COLOR"
      "{"
      "   return color * tex2D(s0, tex);"
      "}";
}

#define FVF 0

RenderChain::~RenderChain()
{
   clear();
   if (fStock)
      cgDestroyProgram(fStock);
   if (vStock)
      cgDestroyProgram(vStock);
}

RenderChain::RenderChain(const video_info_t &video_info,
      IDirect3DDevice9 *dev_,
      CGcontext cgCtx_,
      const LinkInfo &info, PixelFormat fmt,
      const D3DVIEWPORT9 &final_viewport_)
   : dev(dev_), cgCtx(cgCtx_), video_info(video_info), final_viewport(final_viewport_), frame_count(0)
{
   pixel_size = fmt == RGB565 ? 2 : 4;
   create_first_pass(info, fmt);
   log_info(info);
   compile_shaders(fStock, vStock, "");
}

void RenderChain::clear()
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
      if (passes[i].fPrg)
         cgDestroyProgram(passes[i].fPrg);
      if (passes[i].vPrg)
         cgDestroyProgram(passes[i].vPrg);
   }

   for (unsigned i = 0; i < luts.size(); i++)
   {
      if (luts[i].tex)
         luts[i].tex->Release();
   }

   passes.clear();
   luts.clear();
}

void RenderChain::add_pass(const LinkInfo &info)
{
   Pass pass;
   pass.info = info;
   pass.last_width = 0;
   pass.last_height = 0;

   compile_shaders(pass.fPrg, pass.vPrg, info.pass->source.cg);
   init_fvf(pass);

   if (FAILED(dev->CreateVertexBuffer(
               4 * sizeof(Vertex),
               dev->GetSoftwareVertexProcessing() ? D3DUSAGE_SOFTWAREPROCESSING : 0,
               FVF,
               D3DPOOL_DEFAULT,
               &pass.vertex_buf,
               nullptr)))
   {
      throw std::runtime_error("Failed to create Vertex buf ...");
   }

   if (FAILED(dev->CreateTexture(info.tex_w, info.tex_h, 1,
               D3DUSAGE_RENDERTARGET,
               passes.back().info.pass->fbo.fp_fbo ? D3DFMT_A32B32G32R32F : D3DFMT_X8R8G8B8,
               D3DPOOL_DEFAULT,
               &pass.tex, nullptr)))
   {
      throw std::runtime_error("Failed to create texture ...");
   }

   dev->SetTexture(0, pass.tex);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   dev->SetTexture(0, nullptr);

   passes.push_back(pass);

   log_info(info);
}

void RenderChain::add_lut(const std::string &id,
      const std::string &path,
      bool smooth)
{
   IDirect3DTexture9 *lut;

   RARCH_LOG("[D3D9]: Loading LUT texture: %s.\n", path.c_str());

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
               nullptr,
               nullptr,
               &lut)))
   {
      throw std::runtime_error("Failed to load LUT!");
   }

   dev->SetTexture(0, lut);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   dev->SetTexture(0, nullptr);

   lut_info info = { lut, id, smooth };
   luts.push_back(info);
}

void RenderChain::add_state_tracker(std::shared_ptr<state_tracker_t> tracker)
{
   this->tracker = tracker;
}

void RenderChain::start_render()
{
   passes[0].tex = prev.tex[prev.ptr];
   passes[0].vertex_buf = prev.vertex_buf[prev.ptr];
   passes[0].last_width = prev.last_width[prev.ptr];
   passes[0].last_height = prev.last_height[prev.ptr];
}

void RenderChain::end_render()
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
   IDirect3DSurface9 *back_buffer;
   dev->GetRenderTarget(0, &back_buffer);

   // In-between render target passes.
   for (unsigned i = 0; i < passes.size() - 1; i++)
   {
      Pass &from_pass = passes[i];
      Pass &to_pass = passes[i + 1];

      IDirect3DSurface9 *target;
      to_pass.tex->GetSurfaceLevel(0, &target);
      dev->SetRenderTarget(0, target);

      convert_geometry(from_pass.info,
            out_width, out_height,
            current_width, current_height, final_viewport);

      D3DVIEWPORT9 viewport = {0};
      viewport.X = 0;
      viewport.Y = 0;
      viewport.Width = out_width;
      viewport.Height = out_height;
      viewport.MinZ = 0.0f;
      viewport.MaxZ = 1.0f;
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
   set_cg_mvp(vStock, final_viewport.Width, final_viewport.Height, rotation);
   return true;
}

void RenderChain::create_first_pass(const LinkInfo &info, PixelFormat fmt)
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
                  FVF,
                  D3DPOOL_DEFAULT,
                  &prev.vertex_buf[i],
                  nullptr)))
      {
         throw std::runtime_error("Failed to create Vertex buf ...");
      }

      if (FAILED(dev->CreateTexture(info.tex_w, info.tex_h, 1, 0,
                  fmt == RGB565 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8,
                  D3DPOOL_MANAGED,
                  &prev.tex[i], nullptr)))
      {
         throw std::runtime_error("Failed to create texture ...");
      }

      dev->SetTexture(0, prev.tex[i]);
      dev->SetSamplerState(0, D3DSAMP_MINFILTER,
            info.pass->filter == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      dev->SetSamplerState(0, D3DSAMP_MAGFILTER,
            info.pass->filter == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      dev->SetTexture(0, nullptr);
   }

   compile_shaders(pass.fPrg, pass.vPrg, info.pass->source.cg);
   init_fvf(pass);
   passes.push_back(pass);
}

void RenderChain::compile_shaders(CGprogram &fPrg, CGprogram &vPrg, const std::string &shader)
{
   CGprofile vertex_profile = cgD3D9GetLatestVertexProfile();
   CGprofile fragment_profile = cgD3D9GetLatestPixelProfile();
   RARCH_LOG("[D3D9 Cg]: Vertex profile: %s\n", cgGetProfileString(vertex_profile));
   RARCH_LOG("[D3D9 Cg]: Fragment profile: %s\n", cgGetProfileString(fragment_profile));
   const char **fragment_opts = cgD3D9GetOptimalOptions(fragment_profile);
   const char **vertex_opts = cgD3D9GetOptimalOptions(vertex_profile);

   if (shader.length() > 0)
   {
      RARCH_LOG("[D3D9 Cg]: Compiling shader: %s.\n", shader.c_str());
      fPrg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
            shader.c_str(), fragment_profile, "main_fragment", fragment_opts);

      if (cgGetLastListing(cgCtx))
         RARCH_ERR("[D3D9 Cg]: Fragment error:\n%s\n", cgGetLastListing(cgCtx));

      vPrg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
            shader.c_str(), vertex_profile, "main_vertex", vertex_opts);

      if (cgGetLastListing(cgCtx))
         RARCH_ERR("[D3D9 Cg]: Vertex error:\n%s\n", cgGetLastListing(cgCtx));
   }
   else
   {
      RARCH_LOG("[D3D9 Cg]: Compiling stock shader.\n");

      fPrg = cgCreateProgram(cgCtx, CG_SOURCE, Global::stock_program,
            fragment_profile, "main_fragment", fragment_opts);

      if (cgGetLastListing(cgCtx))
         RARCH_ERR("[D3D9 Cg]: Fragment error:\n%s\n", cgGetLastListing(cgCtx));

      vPrg = cgCreateProgram(cgCtx, CG_SOURCE, Global::stock_program,
            vertex_profile, "main_vertex", vertex_opts);

      if (cgGetLastListing(cgCtx))
         RARCH_ERR("[D3D9 Cg]: Vertex error:\n%s\n", cgGetLastListing(cgCtx));
   }

   if (!fPrg || !vPrg)
      throw std::runtime_error("Failed to compile shaders!");

   cgD3D9LoadProgram(fPrg, true, 0);
   cgD3D9LoadProgram(vPrg, true, 0);
}

void RenderChain::set_shaders(CGprogram &fPrg, CGprogram &vPrg)
{
   cgD3D9BindProgram(fPrg);
   cgD3D9BindProgram(vPrg);
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
      std::memcpy(verts, vert, sizeof(vert));
      pass.vertex_buf->Unlock();
   }

   set_cg_mvp(pass.vPrg, vp_width, vp_height, rotation);

   set_cg_params(pass,
         width, height,
         info.tex_w, info.tex_h,
         vp_width, vp_height);
}

void RenderChain::set_viewport(const D3DVIEWPORT9 &vp)
{
   dev->SetViewport(&vp);
}

void RenderChain::set_cg_mvp(CGprogram &vPrg,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   D3DXMATRIX proj, ortho, rot;
   D3DXMatrixOrthoOffCenterLH(&ortho, 0, vp_width, 0, vp_height, 0, 1);

   if (rotation)
      D3DXMatrixRotationZ(&rot, rotation * (M_PI / 2.0));
   else
      D3DXMatrixIdentity(&rot);

   D3DXMatrixMultiply(&proj, &ortho, &rot);

   D3DXMATRIX tmp;
   D3DXMatrixTranspose(&tmp, &proj);
   CGparameter cgpModelViewProj = cgGetNamedParameter(vPrg, "modelViewProj");
   if (cgpModelViewProj)
      cgD3D9SetUniformMatrix(cgpModelViewProj, &tmp);
}

template <class T>
static void set_cg_param(CGprogram prog, const char *param,
      const T& val)
{
   CGparameter cgp = cgGetNamedParameter(prog, param);
   if (cgp)
      cgD3D9SetUniform(cgp, &val);
}

void RenderChain::set_cg_params(Pass &pass,
            unsigned video_w, unsigned video_h,
            unsigned tex_w, unsigned tex_h,
            unsigned viewport_w, unsigned viewport_h)
{
   D3DXVECTOR2 video_size;
   video_size.x = video_w;
   video_size.y = video_h;
   D3DXVECTOR2 texture_size;
   texture_size.x = tex_w;
   texture_size.y = tex_h;
   D3DXVECTOR2 output_size;
   output_size.x = viewport_w;
   output_size.y = viewport_h;

   set_cg_param(pass.vPrg, "IN.video_size", video_size);
   set_cg_param(pass.fPrg, "IN.video_size", video_size);
   set_cg_param(pass.vPrg, "IN.texture_size", texture_size);
   set_cg_param(pass.fPrg, "IN.texture_size", texture_size);
   set_cg_param(pass.vPrg, "IN.output_size", output_size);
   set_cg_param(pass.fPrg, "IN.output_size", output_size);

   float frame_cnt = frame_count;
   if (pass.info.pass->frame_count_mod)
      frame_cnt = frame_count % pass.info.pass->frame_count_mod;
   set_cg_param(pass.fPrg, "IN.frame_count", frame_cnt);
   set_cg_param(pass.vPrg, "IN.frame_count", frame_cnt);
}

void RenderChain::clear_texture(Pass &pass)
{
   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(pass.tex->LockRect(0, &d3dlr, nullptr, D3DLOCK_NOSYSLOCK)))
   {
      std::memset(d3dlr.pBits, 0, pass.info.tex_h * d3dlr.Pitch);
      pass.tex->UnlockRect(0);
   }
}

void RenderChain::convert_geometry(const LinkInfo &info,
      unsigned &out_width, unsigned &out_height,
      unsigned width, unsigned height,
      const D3DVIEWPORT9 &final_viewport)
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
   if (SUCCEEDED(first.tex->LockRect(0, &d3dlr, nullptr, D3DLOCK_NOSYSLOCK)))
   {
      for (unsigned y = 0; y < height; y++)
      {
         const uint8_t *in = reinterpret_cast<const uint8_t*>(frame) + y * pitch;
         uint8_t *out = reinterpret_cast<uint8_t*>(d3dlr.pBits) + y * d3dlr.Pitch;
         std::memcpy(out, in, width * pixel_size);
      }

      first.tex->UnlockRect(0);
   }
}

void RenderChain::render_pass(Pass &pass, unsigned pass_index)
{
   set_shaders(pass.fPrg, pass.vPrg);
   dev->SetTexture(0, pass.tex);
   dev->SetSamplerState(0, D3DSAMP_MINFILTER,
         pass.info.pass->filter == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   dev->SetSamplerState(0, D3DSAMP_MAGFILTER,
         pass.info.pass->filter == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);

   dev->SetVertexDeclaration(pass.vertex_decl);
   for (unsigned i = 0; i < 4; i++)
      dev->SetStreamSource(i, pass.vertex_buf, 0, sizeof(Vertex));

   bind_orig(pass);
   bind_prev(pass);
   bind_pass(pass, pass_index);
   bind_luts(pass);
   bind_tracker(pass, pass_index);

   dev->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);
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
   RARCH_LOG("[D3D9 Cg]: Render pass info:\n");
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

void RenderChain::bind_orig(Pass &pass)
{
   D3DXVECTOR2 video_size;
   video_size.x = passes[0].last_width;
   video_size.y = passes[0].last_height;

   D3DXVECTOR2 texture_size;
   texture_size.x = passes[0].info.tex_w;
   texture_size.y = passes[0].info.tex_h;

   set_cg_param(pass.vPrg, "ORIG.video_size", video_size);
   set_cg_param(pass.fPrg, "ORIG.video_size", video_size);
   set_cg_param(pass.vPrg, "ORIG.texture_size", texture_size);
   set_cg_param(pass.fPrg, "ORIG.texture_size", texture_size);

   CGparameter param = cgGetNamedParameter(pass.fPrg, "ORIG.texture");
   if (param)
   {
      unsigned index = cgGetParameterResourceIndex(param);
      dev->SetTexture(index, passes[0].tex);
      dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
            passes[0].info.pass->filter == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      dev->SetSamplerState(index, D3DSAMP_MINFILTER,
            passes[0].info.pass->filter == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      dev->SetSamplerState(index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      dev->SetSamplerState(index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      bound_tex.push_back(index);
   }

   param = cgGetNamedParameter(pass.vPrg, "ORIG.tex_coord");
   if (param)
   {
      unsigned index = pass.attrib_map[cgGetParameterResourceIndex(param)];
      dev->SetStreamSource(index, passes[0].vertex_buf, 0, sizeof(Vertex));
      bound_vert.push_back(index);
   }
}

void RenderChain::bind_prev(Pass &pass)
{
   static const char *prev_names[] = {
      "PREV",
      "PREV1",
      "PREV2",
      "PREV3",
      "PREV4",
      "PREV5",
      "PREV6",
   };

   char attr_texture[64];
   char attr_input_size[64];
   char attr_tex_size[64];
   char attr_coord[64];

   D3DXVECTOR2 texture_size;
   texture_size.x = passes[0].info.tex_w;
   texture_size.y = passes[0].info.tex_h;

   for (unsigned i = 0; i < Textures - 1; i++)
   {
      snprintf(attr_texture,    sizeof(attr_texture),    "%s.texture",      prev_names[i]);
      snprintf(attr_input_size, sizeof(attr_input_size), "%s.video_size",   prev_names[i]);
      snprintf(attr_tex_size,   sizeof(attr_tex_size),   "%s.texture_size", prev_names[i]);
      snprintf(attr_coord,      sizeof(attr_coord),      "%s.tex_coord",    prev_names[i]);

      D3DXVECTOR2 video_size;
      video_size.x = prev.last_width[(prev.ptr - (i + 1)) & TexturesMask];
      video_size.y = prev.last_height[(prev.ptr - (i + 1)) & TexturesMask];

      set_cg_param(pass.vPrg, attr_input_size, video_size);
      set_cg_param(pass.fPrg, attr_input_size, video_size);
      set_cg_param(pass.vPrg, attr_tex_size, texture_size);
      set_cg_param(pass.fPrg, attr_tex_size, texture_size);

      CGparameter param = cgGetNamedParameter(pass.fPrg, attr_texture);
      if (param)
      {
         unsigned index = cgGetParameterResourceIndex(param);

         IDirect3DTexture9 *tex = prev.tex[(prev.ptr - (i + 1)) & TexturesMask];
         dev->SetTexture(index, tex);
         bound_tex.push_back(index);

         dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
               passes[0].info.pass->filter == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);
         dev->SetSamplerState(index, D3DSAMP_MINFILTER,
               passes[0].info.pass->filter == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);
         dev->SetSamplerState(index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         dev->SetSamplerState(index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      }

      param = cgGetNamedParameter(pass.vPrg, attr_coord);
      if (param)
      {
         unsigned index = pass.attrib_map[cgGetParameterResourceIndex(param)];
         IDirect3DVertexBuffer9 *vert_buf = prev.vertex_buf[(prev.ptr - (i + 1)) & TexturesMask];
         bound_vert.push_back(index);

         dev->SetStreamSource(index, vert_buf, 0, sizeof(Vertex));
      }
   }
}

void RenderChain::bind_pass(Pass &pass, unsigned pass_index)
{
   // We only bother binding passes which are two indices behind.
   if (pass_index < 3)
      return;

   for (unsigned i = 1; i < pass_index - 1; i++)
   {
      char pass_base[64];
      snprintf(pass_base, sizeof(pass_base), "PASS%u.", i);

      std::string attr_texture = pass_base;
      attr_texture += "texture";
      std::string attr_video_size = pass_base;
      attr_video_size += "video_size";
      std::string attr_texture_size = pass_base;
      attr_texture_size += "texture_size";
      std::string attr_tex_coord = pass_base;
      attr_tex_coord += "tex_coord";

      D3DXVECTOR2 video_size;
      video_size.x = passes[i].last_width;
      video_size.y = passes[i].last_height;

      D3DXVECTOR2 texture_size;
      texture_size.x = passes[i].info.tex_w;
      texture_size.y = passes[i].info.tex_h;

      set_cg_param(pass.vPrg, attr_video_size.c_str(), video_size);
      set_cg_param(pass.fPrg, attr_video_size.c_str(), video_size);
      set_cg_param(pass.vPrg, attr_texture_size.c_str(), texture_size);
      set_cg_param(pass.fPrg, attr_texture_size.c_str(), texture_size);

      CGparameter param = cgGetNamedParameter(pass.fPrg, attr_texture.c_str());
      if (param)
      {
         unsigned index = cgGetParameterResourceIndex(param);
         bound_tex.push_back(index);

         dev->SetTexture(index, passes[i].tex);
         dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
               passes[i].info.pass->filter == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);
         dev->SetSamplerState(index, D3DSAMP_MINFILTER,
               passes[i].info.pass->filter == RARCH_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT);
         dev->SetSamplerState(index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         dev->SetSamplerState(index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      }

      param = cgGetNamedParameter(pass.vPrg, attr_tex_coord.c_str());
      if (param)
      {
         unsigned index = pass.attrib_map[cgGetParameterResourceIndex(param)];
         dev->SetStreamSource(index, passes[i].vertex_buf, 0, sizeof(Vertex));
         bound_vert.push_back(index);
      }
   }
}

void RenderChain::bind_luts(Pass &pass)
{
   for (unsigned i = 0; i < luts.size(); i++)
   {
      CGparameter fparam = cgGetNamedParameter(pass.fPrg, luts[i].id.c_str());
      int bound_index = -1;
      if (fparam)
      {
         unsigned index = cgGetParameterResourceIndex(fparam);
         bound_index = index;
         dev->SetTexture(index, luts[i].tex);
         dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
               luts[i].smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
         dev->SetSamplerState(index, D3DSAMP_MINFILTER,
               luts[i].smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
         dev->SetSamplerState(index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         dev->SetSamplerState(index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
         bound_tex.push_back(index);
      }

      CGparameter vparam = cgGetNamedParameter(pass.vPrg, luts[i].id.c_str());
      if (vparam)
      {
         unsigned index = cgGetParameterResourceIndex(vparam);
         if (index != (unsigned)bound_index)
         {
            dev->SetTexture(index, luts[i].tex);
            dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
                  luts[i].smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
            dev->SetSamplerState(index, D3DSAMP_MINFILTER,
                  luts[i].smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
            dev->SetSamplerState(index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
            dev->SetSamplerState(index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
            bound_tex.push_back(index);
         }
      }
   }
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
      dev->SetTexture(bound_tex[i], nullptr);
   }

   for (unsigned i = 0; i < bound_vert.size(); i++)
      dev->SetStreamSource(bound_vert[i], 0, 0, 0);

   bound_tex.clear();
   bound_vert.clear();
}

static inline bool validate_param_name(const char *name)
{
   static const char *illegal[] = {
      "PREV.",
      "PREV1.",
      "PREV2.",
      "PREV3.",
      "PREV4.",
      "PREV5.",
      "PREV6.",
      "ORIG.",
      "IN.",
      "PASS",
   };

   for (unsigned i = 0; i < sizeof(illegal) / sizeof(illegal[0]); i++)
      if (std::strstr(name, illegal[i]) == name)
         return false;

   return true;
}

static inline CGparameter find_param_from_semantic(CGparameter param, const std::string &sem)
{
   while (param)
   {
      if (cgGetParameterType(param) == CG_STRUCT)
      {
         CGparameter ret = find_param_from_semantic(cgGetFirstStructParameter(param), sem);
         if (ret)
            return ret;
      }
      else
      {
         if (cgGetParameterSemantic(param) &&
               sem == cgGetParameterSemantic(param) &&
               cgGetParameterDirection(param) == CG_IN &&
               cgGetParameterVariability(param) == CG_VARYING &&
               validate_param_name(cgGetParameterName(param)))
         {
            return param;
         }
      }
      param = cgGetNextParameter(param);
   }

   return nullptr;
}

static inline CGparameter find_param_from_semantic(CGprogram prog, const std::string &sem)
{
   return find_param_from_semantic(cgGetFirstParameter(prog, CG_PROGRAM), sem);
}

#define DECL_FVF_POSITION(stream) \
   { (WORD)(stream), 0 * sizeof(float), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, \
      D3DDECLUSAGE_POSITION, 0 }
#define DECL_FVF_TEXCOORD(stream, offset, index) \
   { (WORD)(stream), (WORD)(offset * sizeof(float)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, \
      D3DDECLUSAGE_TEXCOORD, (BYTE)(index) }
#define DECL_FVF_COLOR(stream, offset, index) \
   { (WORD)(stream), (WORD)(offset * sizeof(float)), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, \
      D3DDECLUSAGE_COLOR, (BYTE)(index) } \

void RenderChain::init_fvf(Pass &pass)
{
   static const D3DVERTEXELEMENT9 decl_end = D3DDECL_END();
   static const D3DVERTEXELEMENT9 position_decl = DECL_FVF_POSITION(0);
   static const D3DVERTEXELEMENT9 tex_coord0 = DECL_FVF_TEXCOORD(1, 3, 0);
   static const D3DVERTEXELEMENT9 tex_coord1 = DECL_FVF_TEXCOORD(2, 5, 1);
   static const D3DVERTEXELEMENT9 color = DECL_FVF_COLOR(3, 7, 0);

   D3DVERTEXELEMENT9 decl[MAXD3DDECLLENGTH] = {{0}};
   if (cgD3D9GetVertexDeclaration(pass.vPrg, decl) == CG_FALSE)
      throw std::runtime_error("Failed to get VertexDeclaration!");

   unsigned count;
   for (count = 0; count < MAXD3DDECLLENGTH; count++)
   {
      if (std::memcmp(&decl_end, &decl[count], sizeof(decl_end)) == 0)
         break;
   }

   // This is completely insane.
   // We do not have a good and easy way of setting up our
   // attribute streams, so we have to do it ourselves, yay!
   // Stream 0 => POSITION
   // Stream 1 => TEXCOORD0
   // Stream 2 => TEXCOORD1
   // Stream 3 => COLOR // Not really used for anything.
   // Stream {4..N} => Texture coord streams for varying resources which have no semantics.

   std::vector<bool> indices(count);
   bool texcoord0_taken = false;
   bool texcoord1_taken = false;
   bool stream_taken[4] = {false};

   CGparameter param = find_param_from_semantic(pass.vPrg, "POSITION");
   if (!param)
      param = find_param_from_semantic(pass.vPrg, "POSITION0");
   if (param)
   {
      stream_taken[0] = true;
      RARCH_LOG("[FVF]: POSITION semantic found.\n");
      unsigned index = cgGetParameterResourceIndex(param);
      decl[index] = position_decl;
      indices[index] = true;
   }

   param = find_param_from_semantic(pass.vPrg, "TEXCOORD");
   if (!param)
      param = find_param_from_semantic(pass.vPrg, "TEXCOORD0");
   if (param)
   {
      stream_taken[1] = true;
      texcoord0_taken = true;
      RARCH_LOG("[FVF]: TEXCOORD0 semantic found.\n");
      unsigned index = cgGetParameterResourceIndex(param);
      decl[index] = tex_coord0;
      indices[index] = true;
   }

   param = find_param_from_semantic(pass.vPrg, "TEXCOORD1");
   if (param)
   {
      stream_taken[2] = true;
      texcoord1_taken = true;
      RARCH_LOG("[FVF]: TEXCOORD1 semantic found.\n");
      unsigned index = cgGetParameterResourceIndex(param);
      decl[index] = tex_coord1;
      indices[index] = true;
   }

   param = find_param_from_semantic(pass.vPrg, "COLOR");
   if (!param)
      param = find_param_from_semantic(pass.vPrg, "COLOR0");
   if (param)
   {
      stream_taken[3] = true;
      RARCH_LOG("[FVF]: COLOR0 semantic found.\n");
      unsigned index = cgGetParameterResourceIndex(param);
      decl[index] = color;
      indices[index] = true;
   }

   // Stream {0, 1, 2, 3} might be already taken. Find first vacant stream.
   unsigned index;
   for (index = 0; index < 4 && stream_taken[index]; index++);

   // Find first vacant texcoord declaration.
   unsigned tex_index = 0;
   if (texcoord0_taken && texcoord1_taken)
      tex_index = 2;
   else if (texcoord1_taken && !texcoord0_taken)
      tex_index = 0;
   else if (texcoord0_taken && !texcoord1_taken)
      tex_index = 1;

   for (unsigned i = 0; i < count; i++)
   {
      if (indices[i])
         pass.attrib_map.push_back(0);
      else
      {
         pass.attrib_map.push_back(index);
         D3DVERTEXELEMENT9 elem = DECL_FVF_TEXCOORD(index, 3, tex_index);
         decl[i] = elem;

         // Find next vacant stream.
         index++;
         while (index < 4 && stream_taken[index]) index++;

         // Find next vacant texcoord declaration.
         tex_index++;
         if (tex_index == 1 && texcoord1_taken)
            tex_index++;
      }
   }

   if (FAILED(dev->CreateVertexDeclaration(decl, &pass.vertex_decl)))
      throw std::runtime_error("Failed to set up FVF!");
}

void RenderChain::bind_tracker(Pass &pass, unsigned pass_index)
{
   if (!tracker)
      return;

   if (pass_index == 1)
      uniform_cnt = state_get_uniform(tracker.get(), uniform_info, MAX_VARIABLES, frame_count);

   for (unsigned i = 0; i < uniform_cnt; i++)
   {
      set_cg_param(pass.fPrg, uniform_info[i].id, uniform_info[i].value);
      set_cg_param(pass.vPrg, uniform_info[i].id, uniform_info[i].value);
   }
}

