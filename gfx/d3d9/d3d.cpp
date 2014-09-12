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

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include "d3d.hpp"
#ifndef _XBOX
#include "render_chain.hpp"
#endif
#include "../../file.h"
#include "../gfx_common.h"

#include "../context/win32_common.h"

#ifndef _XBOX
#define HAVE_MONITOR
#define HAVE_WINDOW
#endif

#include "../../compat/posix_string.h"
#include "../../performance.h"

#if defined(HAVE_CG)
#define HAVE_SHADERS
#endif

#ifdef HAVE_HLSL
#include "../../gfx/shader_hlsl.h"
#endif

#include "d3d_shared.h"

#ifdef _XBOX
#include "../../xdk/xdk_resources.h"
#include "render_chain_xdk.h"
#endif

#ifdef HAVE_MONITOR
static BOOL CALLBACK monitor_enum_proc(HMONITOR hMonitor,
      HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
   Monitor::all_hms[Monitor::num_mons++] = hMonitor;
   return TRUE;
}

// Multi-monitor support.
static RECT d3d_monitor_rect(d3d_video_t *d3d)
{
   Monitor::num_mons = 0;
   EnumDisplayMonitors(NULL, NULL, monitor_enum_proc, 0);

   if (!Monitor::last_hm)
      Monitor::last_hm = MonitorFromWindow(
            GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
   HMONITOR hm_to_use = Monitor::last_hm;

   unsigned fs_monitor = g_settings.video.monitor_index;
   if (fs_monitor && fs_monitor <= Monitor::num_mons 
         && Monitor::all_hms[fs_monitor - 1])
   {
      hm_to_use = Monitor::all_hms[fs_monitor - 1];
      d3d->cur_mon_id = fs_monitor - 1;
   }
   else
   {
      for (unsigned i = 0; i < Monitor::num_mons; i++)
      {
         if (Monitor::all_hms[i] == hm_to_use)
         {
            d3d->cur_mon_id = i;
            break;
         }
      }
   }

   MONITORINFOEX current_mon;
   memset(&current_mon, 0, sizeof(current_mon));
   current_mon.cbSize = sizeof(MONITORINFOEX);
   GetMonitorInfo(hm_to_use, (MONITORINFO*)&current_mon);

   return current_mon.rcMonitor;
}
#endif

#ifndef _XBOX
static void d3d_recompute_pass_sizes(d3d_video_t *d3d)
{
   LinkInfo link_info = {0};
   link_info.pass = &d3d->shader.pass[0];
   link_info.tex_w = link_info.tex_h = 
      d3d->video_info.input_scale * RARCH_SCALE_BASE;

   unsigned current_width = link_info.tex_w;
   unsigned current_height = link_info.tex_h;
   unsigned out_width = 0;
   unsigned out_height = 0;

   if (!renderchain_set_pass_size(d3d->chain, 0,
            current_width, current_height))
   {
      RARCH_ERR("[D3D]: Failed to set pass size.\n");
      return;
   }

   for (unsigned i = 1; i < d3d->shader.passes; i++)
   {
      renderchain_convert_geometry(d3d->chain, &link_info,
            out_width, out_height,
            current_width, current_height, &d3d->final_viewport);

      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      if (!renderchain_set_pass_size(d3d->chain, i,
               link_info.tex_w, link_info.tex_h))
      {
         RARCH_ERR("[D3D]: Failed to set pass size.\n");
         return;
      }

      current_width = out_width;
      current_height = out_height;

      link_info.pass = &d3d->shader.pass[i];
   }
}
#endif

#ifndef DONT_HAVE_STATE_TRACKER
static bool d3d_init_imports(d3d_video_t *d3d)
{
   if (!d3d->shader.variables)
      return true;

   state_tracker_info tracker_info = {0};

   tracker_info.wram = (uint8_t*)
      pretro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   tracker_info.info = d3d->shader.variable;
   tracker_info.info_elem = d3d->shader.variables;

#ifdef HAVE_PYTHON
   if (*d3d->shader.script_path)
   {
      tracker_info.script = d3d->shader.script_path;
      tracker_info.script_is_file = true;
   }

   tracker_info.script_class = 
      *d3d->shader.script_class ? d3d->shader.script_class : NULL;
#endif

   state_tracker_t *state_tracker = state_tracker_init(&tracker_info);
   if (!state_tracker)
   {
      RARCH_ERR("Failed to initialize state tracker.\n");
      return false;
   }

   renderchain_add_state_tracker(d3d->chain, state_tracker);
   return true;
}
#endif

static bool d3d_init_chain(d3d_video_t *d3d, const video_info_t *video_info)
{
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;
   /* Setup information for first pass. */
#ifdef _XBOX
   /* TODO - properly implement this. */
   d3d_video_t *link_info = (d3d_video_t*)d3d;
   link_info->tex_w = link_info->tex_h = 
	   RARCH_SCALE_BASE * video_info->input_scale;

   //d3d_deinit_chain(d3d);
#else
   LinkInfo link_info = {0};
   link_info.pass = &d3d->shader.pass[0];
   link_info.tex_w = link_info.tex_h = 
      video_info->input_scale * RARCH_SCALE_BASE;
#endif

#ifdef _XBOX
   if (!renderchain_init(d3d, video_info))
   {
      RARCH_ERR("[D3D]: Failed to init render chain.\n");
      return false;
   }
#else
   d3d->chain = new renderchain_t();
   if (!d3d->chain)
      return false;

   if (!renderchain_init(d3d->chain, &d3d->video_info, d3dr,
            d3d->cgCtx, &d3d->final_viewport, &link_info,
            d3d->video_info.rgb32 ? ARGB : RGB565))
   {
      RARCH_ERR("[D3D9]: Failed to init render chain.\n");
      return false;
   }

   unsigned current_width = link_info.tex_w;
   unsigned current_height = link_info.tex_h;
   unsigned out_width = 0;
   unsigned out_height = 0;

   for (unsigned i = 1; i < d3d->shader.passes; i++)
   {
      renderchain_convert_geometry(d3d->chain, &link_info,
            out_width, out_height,
            current_width, current_height, &d3d->final_viewport);

      link_info.pass = &d3d->shader.pass[i];
      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      current_width = out_width;
      current_height = out_height;

      if (!renderchain_add_pass(d3d->chain, &link_info))
      {
         RARCH_ERR("[D3D9]: Failed to add pass.\n");
         return false;
      }
   }

   if (!d3d_init_luts(d3d))
   {
      RARCH_ERR("[D3D9]: Failed to init LUTs.\n");
      return false;
   }
#endif

#ifndef _XBOX
#ifndef DONT_HAVE_STATE_TRACKER
   if (!d3d_init_imports(d3d))
   {
      RARCH_ERR("[D3D9]: Failed to init imports.\n");
      return false;
   }
#endif
#endif

   return true;
}

#ifdef _XBOX
static void d3d_reinit_renderchain(void *data,
      const video_info_t *video)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   d3d->pixel_size   = video->rgb32 ?
      sizeof(uint32_t) : sizeof(uint16_t);
   d3d->tex_w = d3d->tex_h = 
      RARCH_SCALE_BASE * video->input_scale;

   RARCH_LOG(
         "Reinitializing renderchain - and textures (%u x %u @ %u bpp)\n",
         d3d->tex_w, d3d->tex_h, d3d->pixel_size * CHAR_BIT);

   d3d_deinit_chain(d3d);
   d3d_init_chain(d3d, video);
}
#endif

#ifdef _XBOX
#ifdef HAVE_RMENU
extern struct texture_image *menu_texture;
#endif

#ifdef _XBOX1
static bool texture_image_render(void *data,
      struct texture_image *out_img,
      int x, int y, int w, int h, bool force_fullscreen)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;

   if (out_img->pixels == NULL || out_img->vertex_buf == NULL)
      return false;

   float fX = static_cast<float>(x);
   float fY = static_cast<float>(y);

   // create the new vertices
   Vertex newVerts[] =
   {
      // x,           y,              z,     color, u ,v
      {fX,            fY,             0.0f,  0,     0, 0},
      {fX + w,        fY,             0.0f,  0,     1, 0},
      {fX + w,        fY + h,         0.0f,  0,     1, 1},
      {fX,            fY + h,         0.0f,  0,     0, 1}
   };

   // load the existing vertices
   Vertex *pCurVerts;

   HRESULT ret = out_img->vertex_buf->Lock(0, 0,
         (unsigned char**)&pCurVerts, 0);

   if (FAILED(ret))
      return false;

   // copy the new verts over the old verts
   memcpy(pCurVerts, newVerts, 4 * sizeof(Vertex));
   out_img->vertex_buf->Unlock();

   d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
   d3d->dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d->dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

   /* Also blend the texture with the set alpha value. */
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);

   /* Draw the quad. */
   d3dr->SetTexture(0, out_img->pixels);
   d3d_set_stream_source(d3dr, 0,
         out_img->vertex_buf, 0, sizeof(Vertex));
   d3dr->SetVertexShader(D3DFVF_CUSTOMVERTEX);

   if (force_fullscreen)
   {
      D3DVIEWPORT vp = {0};
      vp.Width  = w;
      vp.Height = h;
      vp.X      = 0;
      vp.Y      = 0;
      vp.MinZ   = 0.0f;
      vp.MaxZ   = 1.0f;
      d3d_set_viewport(d3dr, &vp);
   }
   d3d_draw_primitive(d3dr, D3DPT_QUADLIST, 0, 1);

   return true;
}
#endif

#ifdef HAVE_MENU
static void d3d_draw_texture(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
#if defined(HAVE_RMENU)
   menu_texture->x = 0;
   menu_texture->y = 0;

   if (d3d->menu_texture_enable)
   {
      d3d->dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      d3d->dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
      d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
      texture_image_render(d3d, menu_texture,
            menu_texture->x, menu_texture->y,
         d3d->screen_width, d3d->screen_height, true);
      d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
   }
#endif
}
#endif
#endif

#ifdef HAVE_FBO
static bool d3d_init_multipass(d3d_video_t *d3d)
{
   config_file_t *conf = config_file_new(d3d->cg_shader.c_str());
   if (!conf)
   {
      RARCH_ERR("Failed to load preset.\n");
      return false;
   }

   memset(&d3d->shader, 0, sizeof(d3d->shader));

   if (!gfx_shader_read_conf_cgp(conf, &d3d->shader))
   {
      config_file_free(conf);
      RARCH_ERR("Failed to parse CGP file.\n");
      return false;
   }

   config_file_free(conf);

   gfx_shader_resolve_relative(&d3d->shader, d3d->cg_shader.c_str());

   RARCH_LOG("[D3D9 Meta-Cg] Found %u shaders.\n", d3d->shader.passes);

   for (unsigned i = 0; i < d3d->shader.passes; i++)
   {
      if (!d3d->shader.pass[i].fbo.valid)
      {
         d3d->shader.pass[i].fbo.scale_x = 
            d3d->shader.pass[i].fbo.scale_y = 1.0f;
         d3d->shader.pass[i].fbo.type_x = 
            d3d->shader.pass[i].fbo.type_y = RARCH_SCALE_INPUT;
      }
   }

   bool use_extra_pass = d3d->shader.passes < GFX_MAX_SHADERS && 
      d3d->shader.pass[d3d->shader.passes - 1].fbo.valid;

   if (use_extra_pass)
   {
      d3d->shader.passes++;
      gfx_shader_pass &dummy_pass = d3d->shader.pass[d3d->shader.passes - 1];
      dummy_pass.fbo.scale_x = dummy_pass.fbo.scale_y = 1.0f;
      dummy_pass.fbo.type_x = dummy_pass.fbo.type_y = RARCH_SCALE_VIEWPORT;
      dummy_pass.filter = RARCH_FILTER_UNSPEC;
   }
   else
   {
      gfx_shader_pass &pass = d3d->shader.pass[d3d->shader.passes - 1];
      pass.fbo.scale_x = pass.fbo.scale_y = 1.0f;
      pass.fbo.type_x = pass.fbo.type_y = RARCH_SCALE_VIEWPORT;
   }

   return true;
}
#endif

static void d3d_set_font_rect(d3d_video_t *d3d,
      const struct font_params *params)
{
#ifndef _XBOX
   float pos_x = g_settings.video.msg_pos_x;
   float pos_y = g_settings.video.msg_pos_y;
   float font_size = g_settings.video.font_size;

   if (params)
   {
      pos_x = params->x;
      pos_y = params->y;
      font_size *= params->scale;
   }

   d3d->font_rect.left = d3d->final_viewport.X + 
      d3d->final_viewport.Width * pos_x;
   d3d->font_rect.right = d3d->final_viewport.X + 
      d3d->final_viewport.Width;
   d3d->font_rect.top = d3d->final_viewport.Y + 
      (1.0f - pos_y) * d3d->final_viewport.Height - font_size; 
   d3d->font_rect.bottom = d3d->final_viewport.Height;

   d3d->font_rect_shifted = d3d->font_rect;
   d3d->font_rect_shifted.left -= 2;
   d3d->font_rect_shifted.right -= 2;
   d3d->font_rect_shifted.top += 2;
   d3d->font_rect_shifted.bottom += 2;
#endif
}

static bool d3d_init_singlepass(d3d_video_t *d3d)
{
#ifndef _XBOX
   memset(&d3d->shader, 0, sizeof(d3d->shader));
   d3d->shader.passes = 1;
   gfx_shader_pass &pass = d3d->shader.pass[0];
   pass.fbo.valid = true;
   pass.fbo.scale_x = pass.fbo.scale_y = 1.0;
   pass.fbo.type_x = pass.fbo.type_y = RARCH_SCALE_VIEWPORT;
   strlcpy(pass.source.path, d3d->cg_shader.c_str(),
         sizeof(pass.source.path));
#endif

   return true;
}

static bool d3d_process_shader(d3d_video_t *d3d)
{
#ifdef HAVE_FBO
   if (strcmp(path_get_extension(
               d3d->cg_shader.c_str()), "cgp") == 0)
      return d3d_init_multipass(d3d);
#endif

   return d3d_init_singlepass(d3d);
}

#ifndef _XBOX
static bool d3d_init_luts(d3d_video_t *d3d)
{
   for (unsigned i = 0; i < d3d->shader.luts; i++)
   {
      bool ret = renderchain_add_lut(
            d3d->chain, d3d->shader.lut[i].id, d3d->shader.lut[i].path,
         d3d->shader.lut[i].filter == RARCH_FILTER_UNSPEC ?
            g_settings.video.smooth :
            (d3d->shader.lut[i].filter == RARCH_FILTER_LINEAR));

      if (!ret)
         return ret;
   }

   return true;
}
#endif

#ifdef HAVE_OVERLAY
#include "d3d_overlays.cpp"
#endif

static bool d3d_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      const char *msg)
{
   D3DVIEWPORT screen_vp;
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;

  if (!frame)
      return true;

   RARCH_PERFORMANCE_INIT(d3d_frame);
   RARCH_PERFORMANCE_START(d3d_frame);

#ifndef _XBOX
   /* We cannot recover in fullscreen. */
   if (d3d->needs_restore && IsIconic(d3d->hWnd))
      return true;
#endif
   if (d3d->needs_restore && !d3d_restore(d3d))
   {
      RARCH_ERR("[D3D]: Failed to restore.\n");
      return false;
   }

   if (d3d->should_resize)
   {
      d3d_calculate_rect(d3d, d3d->screen_width,
            d3d->screen_height, d3d->video_info.force_aspect,
            g_extern.system.aspect_ratio);

#ifndef _XBOX
      renderchain_set_final_viewport(d3d->chain, &d3d->final_viewport);
      d3d_recompute_pass_sizes(d3d);
#endif

      d3d->should_resize = false;
   }

   /* render_chain() only clears out viewport, 
    * clear out everything. */
   screen_vp.X = 0;
   screen_vp.Y = 0;
   screen_vp.MinZ = 0;
   screen_vp.MaxZ = 1;
   screen_vp.Width = d3d->screen_width;
   screen_vp.Height = d3d->screen_height;
   d3d_set_viewport(d3dr, &screen_vp);
   d3dr->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);

   /* Insert black frame first, so we 
    * can screenshot, etc. */
   if (g_settings.video.black_frame_insertion)
   {
      d3d_swap(d3d, d3dr);
      if (d3d->needs_restore)
         return true;
      d3dr->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);
   }

#ifdef _XBOX
   renderchain_render_pass(d3d, frame, width, height,
         pitch, d3d->dev_rotation);
#else
   if (!renderchain_render(d3d->chain, frame, width,
            height, pitch, d3d->dev_rotation))
   {
      RARCH_ERR("[D3D]: Failed to render scene.\n");
      return false;
   }
#endif

   if (d3d->font_ctx && d3d->font_ctx->render_msg && msg)
   {
      struct font_params font_parms = {0};
#ifdef _XBOX
#if defined(_XBOX1)
      float msg_width  = 60;
      float msg_height = 365;
#elif defined(_XBOX360)
      float msg_width  = (g_extern.lifecycle_state & (1ULL << MODE_MENU_HD)) ? 160 : 100;
      float msg_height = 120;
#endif
      font_parms.x = msg_width;
      font_parms.y = msg_height;
      font_parms.scale = 21;
#endif
      d3d->font_ctx->render_msg(d3d, msg, &font_parms);
   }

#ifdef HAVE_MENU
#ifndef _XBOX
   if (d3d->menu && d3d->menu->enabled)
      d3d_overlay_render(d3d, d3d->menu);
#endif
#endif

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled)
   {
      for (unsigned i = 0; i < d3d->overlays.size(); i++)
         d3d_overlay_render(d3d, &d3d->overlays[i]);
   }
#endif

#ifdef HAVE_MENU
   if (g_extern.lifecycle_state & (1ULL << MODE_MENU) 
         && driver.menu_ctx && driver.menu_ctx->frame)
      driver.menu_ctx->frame();

#ifdef _XBOX
   /* TODO - should be refactored. */
   if (d3d && d3d->menu_texture_enable)
      d3d_draw_texture(d3d);
#endif
#endif

   RARCH_PERFORMANCE_STOP(d3d_frame);

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->update_window_title)
      d3d->ctx_driver->update_window_title(d3d);

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->swap_buffers)
      d3d->ctx_driver->swap_buffers(d3d);

   return true;
}

static bool d3d_read_viewport(void *data, uint8_t *buffer)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;

   RARCH_PERFORMANCE_INIT(d3d_read_viewport);
   RARCH_PERFORMANCE_START(d3d_read_viewport);
   bool ret = true;

   (void)data;
   (void)buffer;

#ifdef _XBOX
   ret = false;
#else
   LPDIRECT3DSURFACE target = NULL;
   LPDIRECT3DSURFACE dest   = NULL;

   if (FAILED(d3d->d3d_err = d3dr->GetRenderTarget(0, &target)))
   {
      ret = false;
      goto end;
   }

   if (FAILED(d3d->d3d_err = d3dr->CreateOffscreenPlainSurface(
               d3d->screen_width,
               d3d->screen_height,
               D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM,
               &dest, NULL)))
   {
      ret = false;
      goto end;
   }

   if (FAILED(d3d->d3d_err = d3dr->GetRenderTargetData(target, dest)))
   {
      ret = false;
      goto end;
   }

   D3DLOCKED_RECT rect;
   if (SUCCEEDED(dest->LockRect(&rect, NULL, D3DLOCK_READONLY)))
   {
      unsigned pitchpix = rect.Pitch / 4;
      const uint32_t *pixels = (const uint32_t*)rect.pBits;
      pixels += d3d->final_viewport.X;
      pixels += (d3d->final_viewport.Height - 1) * pitchpix;
      pixels -= d3d->final_viewport.Y * pitchpix;

      for (unsigned y = 0; y < d3d->final_viewport.Height;
            y++, pixels -= pitchpix)
      {
         for (unsigned x = 0; x < d3d->final_viewport.Width; x++)
         {
            *buffer++ = (pixels[x] >>  0) & 0xff;
            *buffer++ = (pixels[x] >>  8) & 0xff;
            *buffer++ = (pixels[x] >> 16) & 0xff;
         }
      }

      dest->UnlockRect();
   }
   else
      ret = false;

end:
   RARCH_PERFORMANCE_STOP(d3d_read_viewport);
   if (target)
      target->Release();
   if (dest)
      dest->Release();
#endif
   return ret;
}

static bool d3d_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   std::string shader = "";

   switch (type)
   {
      case RARCH_SHADER_CG:
         if (path)
            shader = path;
#ifdef HAVE_HLSL
         d3d->shader = &hlsl_backend;
#endif
         break;
      default:
         break;
   }

   std::string old_shader = d3d->cg_shader;
   bool restore_old = false;
#ifdef HAVE_CG
   d3d->cg_shader = shader;
#endif

   if (!d3d_process_shader(d3d) || !d3d_restore(d3d))
   {
      RARCH_ERR("[D3D]: Setting shader failed.\n");
      restore_old = true;
   }

   if (restore_old)
   {
#ifdef HAVE_CG
      d3d->cg_shader = old_shader;
#endif
      d3d_process_shader(d3d);
      d3d_restore(d3d);
   }

   return !restore_old;
}

#ifdef HAVE_MENU
static void d3d_get_poke_interface(void *data,
      const video_poke_interface_t **iface);
#endif

#ifdef HAVE_MENU
static void d3d_set_menu_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   (void)frame;
   (void)rgb32;
   (void)width;
   (void)height;
   (void)alpha;

#ifndef _XBOX
   if (!d3d->menu->tex || d3d->menu->tex_w != width 
         || d3d->menu->tex_h != height)
   {
      if (d3d->menu && d3d->menu->tex)
         d3d->menu->tex->Release();
      if (FAILED(d3d->dev->CreateTexture(width, height, 1,
                  0, D3DFMT_A8R8G8B8,
                  D3DPOOL_MANAGED,
                  &d3d->menu->tex, NULL)))
      {
         RARCH_ERR("[D3D]: Failed to create menu texture.\n");
         return;
      }
      d3d->menu->tex_w = width;
      d3d->menu->tex_h = height;
   }

   d3d->menu->alpha_mod = alpha;


   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(d3d->menu->tex->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
   {
      if (rgb32)
      {
         uint8_t *dst = (uint8_t*)d3dlr.pBits;
         const uint32_t *src = (const uint32_t*)frame;
         for (unsigned h = 0; h < height;
               h++, dst += d3dlr.Pitch, src += width)
         {
            memcpy(dst, src, width * sizeof(uint32_t));
            memset(dst + width * sizeof(uint32_t), 0,
                  d3dlr.Pitch - width * sizeof(uint32_t));
         }
      }
      else
      {
         uint32_t *dst = (uint32_t*)d3dlr.pBits;
         const uint16_t *src = (const uint16_t*)frame;
         for (unsigned h = 0; h < height;
               h++, dst += d3dlr.Pitch >> 2, src += width)
         {
            for (unsigned w = 0; w < width; w++)
            {
               uint16_t c = src[w];
               uint32_t r = (c >> 12) & 0xf;
               uint32_t g = (c >>  8) & 0xf;
               uint32_t b = (c >>  4) & 0xf;
               uint32_t a = (c >>  0) & 0xf;
               r = ((r << 4) | r) << 16;
               g = ((g << 4) | g) <<  8;
               b = ((b << 4) | b) <<  0;
               a = ((a << 4) | a) << 24;
               dst[w] = r | g | b | a;
            }
         }
      }

      if (d3d->menu)
         d3d->menu->tex->UnlockRect(0);
   }
#endif
}

static void d3d_set_menu_texture_enable(void *data,
      bool state, bool full_screen)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

#ifdef _XBOX
   d3d->menu_texture_enable = state;
   d3d->menu_texture_full_screen = full_screen;
#else
   if (!d3d || !d3d->menu)
      return;

   d3d->menu->enabled = state;
   d3d->menu->fullscreen = full_screen;
#endif
}
#endif

static const video_poke_interface_t d3d_poke_interface = {
   NULL,
#ifdef HAVE_FBO
   NULL,
   NULL,
#endif
   d3d_set_aspect_ratio,
   d3d_apply_state_changes,
#ifdef HAVE_MENU
   d3d_set_menu_texture_frame,
   d3d_set_menu_texture_enable,
#endif
   d3d_set_osd_msg,

   d3d_show_mouse,
};

static void d3d_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &d3d_poke_interface;
}

video_driver_t video_d3d = {
   d3d_init,
   d3d_frame,
   d3d_set_nonblock_state,
   d3d_alive,
   d3d_focus,
   d3d_set_shader,
   d3d_free,
   "d3d",
   d3d_set_rotation,
   d3d_viewport_info,
   d3d_read_viewport,
#ifdef HAVE_OVERLAY
   d3d_get_overlay_interface,
#endif
   d3d_get_poke_interface
};
