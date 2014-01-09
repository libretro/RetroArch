/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

// This driver is merged from the external RetroArch-D3D9 driver.
// It is written in C++11 (should be compat with MSVC 2010).
// Might get rewritten in C99 if I have lots of time to burn.

#include "d3d9.hpp"
#include "render_chain.hpp"
#include "../../file.h"
#include "../context/win32_common.h"
#include <algorithm>

#ifdef _MSC_VER
#ifndef _XBOX
#pragma comment( lib, "d3d9" )
#pragma comment( lib, "d3dx9" )
#pragma comment( lib, "cgd3d9" )
#pragma comment( lib, "dxguid" )
#endif
#endif

bool d3d_process_shader(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   if (strcmp(path_get_extension(d3d->cg_shader.c_str()), "cgp") == 0)
      return d3d->init_multipass();

   return d3d->init_singlepass();
}

void d3d_update_title(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   char buffer[128], buffer_fps[128];
   bool fps_draw = g_settings.fps_show;
   if (gfx_get_fps(buffer, sizeof(buffer), fps_draw ? buffer_fps : NULL, sizeof(buffer_fps)))
   {
      std::string title = buffer;
      title += " || Direct3D9";
      SetWindowText(d3d->hWnd, title.c_str());
   }

   if (fps_draw)
      msg_queue_push(g_extern.msg_queue, buffer_fps, 1, 1);

   g_extern.frame_count++;
}

void d3d_set_font_rect(void *data, font_params_t *params)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   float pos_x = g_settings.video.msg_pos_x;
   float pos_y = g_settings.video.msg_pos_y;
   float font_size = g_settings.video.font_size;

   if (params)
   {
      pos_x = params->x;
      pos_y = params->y;
      font_size *= params->scale;
   }

   d3d->font_rect.left = d3d->final_viewport.X + d3d->final_viewport.Width * pos_x;
   d3d->font_rect.right = d3d->final_viewport.X + d3d->final_viewport.Width;
   d3d->font_rect.top = d3d->final_viewport.Y + (1.0f - pos_y) * d3d->final_viewport.Height - font_size; 
   d3d->font_rect.bottom = d3d->final_viewport.Height;

   d3d->font_rect_shifted = d3d->font_rect;
   d3d->font_rect_shifted.left -= 2;
   d3d->font_rect_shifted.right -= 2;
   d3d->font_rect_shifted.top += 2;
   d3d->font_rect_shifted.bottom += 2;
}

void d3d_recompute_pass_sizes(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   LinkInfo link_info = {0};
   link_info.pass = &d3d->shader.pass[0];
   link_info.tex_w = link_info.tex_h = d3d->video_info.input_scale * RARCH_SCALE_BASE;

   unsigned current_width = link_info.tex_w;
   unsigned current_height = link_info.tex_h;
   unsigned out_width = 0;
   unsigned out_height = 0;

   if (!d3d->chain->set_pass_size(0, current_width, current_height))
   {
      RARCH_ERR("[D3D]: Failed to set pass size.\n");
      return;
   }

   for (unsigned i = 1; i < d3d->shader.passes; i++)
   {
      RenderChain::convert_geometry(link_info,
            out_width, out_height,
            current_width, current_height, d3d->final_viewport);

      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      if (!d3d->chain->set_pass_size(i, link_info.tex_w, link_info.tex_h))
      {
         RARCH_ERR("[D3D]: Failed to set pass size.\n");
         return;
      }

      current_width = out_width;
      current_height = out_height;

      link_info.pass = &d3d->shader.pass[i];
   }
}

#ifdef HAVE_CG
bool d3d_init_shader(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d->cgCtx = cgCreateContext();
   if (d3d->cgCtx == NULL)
      return false;

   RARCH_LOG("[D3D9 Cg]: Created context.\n");

   HRESULT ret = cgD3D9SetDevice(d3d->dev);
   if (FAILED(ret))
      return false;

   return true;
}

void d3d_deinit_shader(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   if (!d3d->cgCtx)
      return;

   cgD3D9UnloadAllPrograms();
   cgD3D9SetDevice(NULL);
   cgDestroyContext(d3d->cgCtx);
   d3d->cgCtx = NULL;
}
#endif

bool D3DVideo::init_singlepass(void)
{
   memset(&shader, 0, sizeof(shader));
   shader.passes = 1;
   gfx_shader_pass &pass = shader.pass[0];
   pass.fbo.valid = true;
   pass.fbo.scale_x = pass.fbo.scale_y = 1.0;
   pass.fbo.type_x = pass.fbo.type_y = RARCH_SCALE_VIEWPORT;
   strlcpy(pass.source.cg, cg_shader.c_str(), sizeof(pass.source.cg));

   return true;
}

bool D3DVideo::init_imports(void)
{
   if (!shader.variables)
      return true;

   state_tracker_info tracker_info = {0};

   tracker_info.wram = (uint8_t*)pretro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   tracker_info.info = shader.variable;
   tracker_info.info_elem = shader.variables;

#ifdef HAVE_PYTHON
   if (*shader.script_path)
   {
      tracker_info.script = shader.script_path;
      tracker_info.script_is_file = true;
   }

   tracker_info.script_class = *shader.script_class ? shader.script_class : NULL;
#endif

   state_tracker_t *state_tracker = state_tracker_init(&tracker_info);
   if (!state_tracker)
   {
      RARCH_ERR("Failed to initialize state tracker.\n");
      return false;
   }

   chain->add_state_tracker(state_tracker);
   return true;
}

bool D3DVideo::init_luts(void)
{
   for (unsigned i = 0; i < shader.luts; i++)
   {
      bool ret = chain->add_lut(shader.lut[i].id, shader.lut[i].path,
         shader.lut[i].filter == RARCH_FILTER_UNSPEC ?
            g_settings.video.smooth :
            (shader.lut[i].filter == RARCH_FILTER_LINEAR));

      if (!ret)
         return ret;
   }

   return true;
}

bool D3DVideo::init_multipass(void)
{
   config_file_t *conf = config_file_new(cg_shader.c_str());
   if (!conf)
   {
      RARCH_ERR("Failed to load preset.\n");
      return false;
   }

   memset(&shader, 0, sizeof(shader));

   if (!gfx_shader_read_conf_cgp(conf, &shader))
   {
      config_file_free(conf);
      RARCH_ERR("Failed to parse CGP file.\n");
      return false;
   }

   config_file_free(conf);

   gfx_shader_resolve_relative(&shader, cg_shader.c_str());

   RARCH_LOG("[D3D9 Meta-Cg] Found %d shaders.\n", shader.passes);

   for (unsigned i = 0; i < shader.passes; i++)
   {
      if (!shader.pass[i].fbo.valid)
      {
         shader.pass[i].fbo.scale_x = shader.pass[i].fbo.scale_y = 1.0f;
         shader.pass[i].fbo.type_x = shader.pass[i].fbo.type_y = RARCH_SCALE_INPUT;
      }
   }

   bool use_extra_pass = shader.passes < GFX_MAX_SHADERS && shader.pass[shader.passes - 1].fbo.valid;
   if (use_extra_pass)
   {
      shader.passes++;
      gfx_shader_pass &dummy_pass = shader.pass[shader.passes - 1];
      dummy_pass.fbo.scale_x = dummy_pass.fbo.scale_y = 1.0f;
      dummy_pass.fbo.type_x = dummy_pass.fbo.type_y = RARCH_SCALE_VIEWPORT;
      dummy_pass.filter = RARCH_FILTER_UNSPEC;
   }
   else
   {
      gfx_shader_pass &pass = shader.pass[shader.passes - 1];
      pass.fbo.scale_x = pass.fbo.scale_y = 1.0f;
      pass.fbo.type_x = pass.fbo.type_y = RARCH_SCALE_VIEWPORT;
   }

   return true;
}

bool D3DVideo::init_chain(const video_info_t *video_info)
{
   // Setup information for first pass.
   LinkInfo link_info = {0};

   link_info.pass = &shader.pass[0];
   link_info.tex_w = link_info.tex_h = video_info->input_scale * RARCH_SCALE_BASE;

   delete chain;
   chain = new RenderChain(
         video_info,
         dev, cgCtx,
         final_viewport);

   if (!chain->init(link_info,
            video_info->rgb32 ? RenderChain::ARGB : RenderChain::RGB565))
   {
      RARCH_ERR("[D3D9]: Failed to init render chain.\n");
      return false;
   }

   unsigned current_width = link_info.tex_w;
   unsigned current_height = link_info.tex_h;
   unsigned out_width = 0;
   unsigned out_height = 0;

   for (unsigned i = 1; i < shader.passes; i++)
   {
      RenderChain::convert_geometry(link_info,
            out_width, out_height,
            current_width, current_height, final_viewport);

      link_info.pass = &shader.pass[i];
      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      current_width = out_width;
      current_height = out_height;

      if (!chain->add_pass(link_info))
      {
         RARCH_ERR("[D3D9]: Failed to add pass.\n");
         return false;
      }
   }

   if (!init_luts())
   {
      RARCH_ERR("[D3D9]: Failed to init LUTs.\n");
      return false;
   }

   if (!init_imports())
   {
      RARCH_ERR("[D3D9]: Failed to init imports.\n");
      return false;
   }

   return true;
}

void D3DVideo::deinit_chain(void)
{
   delete chain;
   chain = NULL;
}

bool D3DVideo::init_font(void)
{
   D3DXFONT_DESC desc = {
      static_cast<int>(g_settings.video.font_size), 0, 400, 0,
      false, DEFAULT_CHARSET,
      OUT_TT_PRECIS,
      CLIP_DEFAULT_PRECIS,
      DEFAULT_PITCH,
      "Verdana" // Hardcode ftl :(
   };

   uint32_t r = static_cast<uint32_t>(g_settings.video.msg_color_r * 255) & 0xff;
   uint32_t g = static_cast<uint32_t>(g_settings.video.msg_color_g * 255) & 0xff;
   uint32_t b = static_cast<uint32_t>(g_settings.video.msg_color_b * 255) & 0xff;
   font_color = D3DCOLOR_XRGB(r, g, b);

   return SUCCEEDED(D3DXCreateFontIndirect(dev, &desc, &font));
}

void D3DVideo::deinit_font(void)
{
   if (font)
      font->Release();
   font = NULL;
}