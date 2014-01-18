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

#include "d3d9.hpp"
#include "render_chain.hpp"

#include "../gfx_common.h"
#ifndef _XBOX
#include "../context/win32_common.h"
#endif

#include "../../compat/posix_string.h"
#include "../../performance.h"

static void d3d_render_msg(void *data, const char *msg, void *userdata);

#ifndef _XBOX
#define HAVE_MONITOR
#define HAVE_WINDOW
#endif

#ifdef HAVE_MONITOR
#define IDI_ICON 1
#define MAX_MONITORS 9

namespace Monitor
{
   static HMONITOR last_hm;
   static HMONITOR all_hms[MAX_MONITORS];
   static unsigned num_mons;
}

static BOOL CALLBACK monitor_enum_proc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
   Monitor::all_hms[Monitor::num_mons++] = hMonitor;
   return TRUE;
}

// Multi-monitor support.
RECT d3d_monitor_rect(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   Monitor::num_mons = 0;
   EnumDisplayMonitors(NULL, NULL, monitor_enum_proc, 0);

   if (!Monitor::last_hm)
      Monitor::last_hm = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
   HMONITOR hm_to_use = Monitor::last_hm;

   unsigned fs_monitor = g_settings.video.monitor_index;
   if (fs_monitor && fs_monitor <= Monitor::num_mons && Monitor::all_hms[fs_monitor - 1])
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

static void d3d_deinitialize(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d_deinit_font(d3d);
   d3d_deinit_chain(d3d);
#ifdef HAVE_CG
   d3d_deinit_shader(d3d);
#endif

   d3d->needs_restore = false;
}


#ifdef HAVE_WINDOW

extern LRESULT CALLBACK WindowProc(HWND hWnd, UINT message,
        WPARAM wParam, LPARAM lParam);
#endif

static bool d3d_init_base(void *data, const video_info_t *info)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   D3DPRESENT_PARAMETERS d3dpp;
   d3d_make_d3dpp(d3d, info, &d3dpp);

   d3d->g_pD3D = D3DCREATE_CTX(D3D_SDK_VERSION);
   if (!d3d->g_pD3D)
   {
      RARCH_ERR("Failed to create D3D interface!\n");
      return false;
   }

   if (FAILED(d3d->d3d_err = d3d->g_pD3D->CreateDevice(
               d3d->cur_mon_id,
               D3DDEVTYPE_HAL,
               d3d->hWnd,
               D3DCREATE_HARDWARE_VERTEXPROCESSING,
               &d3dpp,
               &d3d->dev)))
   {
      RARCH_WARN("[D3D]: Failed to init device with hardware vertex processing (code: 0x%x). Trying to fall back to software vertex processing.\n",
                 (unsigned)d3d->d3d_err);

      if (FAILED(d3d->d3d_err = d3d->g_pD3D->CreateDevice(
                  d3d->cur_mon_id,
                  D3DDEVTYPE_HAL,
                  d3d->hWnd,
                  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                  &d3dpp,
                  &d3d->dev)))
      {
         RARCH_ERR("Failed to initialize device.\n");
         return false;
      }
   }

   return true;
}

static void d3d_calculate_rect(void *data, unsigned width, unsigned height,
   bool keep, float desired_aspect);

static bool d3d_initialize(void *data, const video_info_t *info)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   bool ret = true;
   if (!d3d->g_pD3D)
      ret = d3d_init_base(d3d, info);
   else if (d3d->needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;
      d3d_make_d3dpp(d3d, info, &d3dpp);
      if (d3d->dev->Reset(&d3dpp) != D3D_OK)
      {
         HRESULT res = d3d->dev->TestCooperativeLevel();
         const char *err;
         switch (res)
         {
            case D3DERR_DEVICELOST:
               err = "DEVICELOST";
               break;

            case D3DERR_DEVICENOTRESET:
               err = "DEVICENOTRESET";
               break;

            case D3DERR_DRIVERINTERNALERROR:
               err = "DRIVERINTERNALERROR";
               break;

            default:
               err = "Unknown";
         }
         // Try to recreate the device completely ...
         RARCH_WARN("[D3D]: Attempting to recover from dead state (%s).\n", err);
         d3d_deinitialize(d3d); 
         d3d->g_pD3D->Release();
         d3d->g_pD3D = NULL;
         ret = d3d_init_base(d3d, info);
         if (ret)
            RARCH_LOG("[D3D]: Recovered from dead state.\n");
         else
            return ret;
      }
   }

   if (!ret)
      return ret;

   d3d_calculate_rect(d3d, d3d->screen_width, d3d->screen_height, info->force_aspect, g_extern.system.aspect_ratio);

#ifdef HAVE_CG
   if (!d3d_init_shader(d3d))
   {
      RARCH_ERR("Failed to initialize Cg.\n");
      return false;
   }
#endif

   if (!d3d_init_chain(d3d, info))
   {
      RARCH_ERR("Failed to initialize render chain.\n");
      return false;
   }

   if (!d3d_init_font(d3d))
   {
      RARCH_ERR("Failed to initialize font.\n");
      return false;
   }

   return true;
}

bool d3d_restore(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d_deinitialize(d3d);
   d3d->needs_restore = !d3d_initialize(d3d, &d3d->video_info);

   if (d3d->needs_restore)
      RARCH_ERR("[D3D]: Restore error.\n");

   return !d3d->needs_restore;
}

#ifdef HAVE_OVERLAY
static void d3d_overlay_render(void *data, overlay_t &overlay)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   if (!overlay.tex)
      return;

   struct overlay_vertex
   {
      float x, y, z;
      float u, v;
      float r, g, b, a;
   } vert[4];

   if (!overlay.vert_buf)
   {
      d3d->dev->CreateVertexBuffer(
            sizeof(vert),
#ifdef _XBOX
            0,
#else
            d3d->dev->GetSoftwareVertexProcessing() ? D3DUSAGE_SOFTWAREPROCESSING : 0,
#endif
            0,
            D3DPOOL_MANAGED,
            &overlay.vert_buf,
            NULL);
   }

   for (unsigned i = 0; i < 4; i++)
   {
      vert[i].z = 0.5f;
      vert[i].r = vert[i].g = vert[i].b = 1.0f;
      vert[i].a = overlay.alpha_mod;
   }

   float overlay_width = d3d->final_viewport.Width;
   float overlay_height = d3d->final_viewport.Height;

   vert[0].x = overlay.vert_coords.x * overlay_width;
   vert[1].x = (overlay.vert_coords.x + overlay.vert_coords.w) * overlay_width;
   vert[2].x = overlay.vert_coords.x * overlay_width;
   vert[3].x = (overlay.vert_coords.x + overlay.vert_coords.w) * overlay_width;
   vert[0].y = overlay.vert_coords.y * overlay_height;
   vert[1].y = overlay.vert_coords.y * overlay_height;
   vert[2].y = (overlay.vert_coords.y + overlay.vert_coords.h) * overlay_height;
   vert[3].y = (overlay.vert_coords.y + overlay.vert_coords.h) * overlay_height;

   vert[0].u = overlay.tex_coords.x;
   vert[1].u = overlay.tex_coords.x + overlay.tex_coords.w;
   vert[2].u = overlay.tex_coords.x;
   vert[3].u = overlay.tex_coords.x + overlay.tex_coords.w;
   vert[0].v = overlay.tex_coords.y;
   vert[1].v = overlay.tex_coords.y;
   vert[2].v = overlay.tex_coords.y + overlay.tex_coords.h;
   vert[3].v = overlay.tex_coords.y + overlay.tex_coords.h;

   // Align texels and vertices.
   for (unsigned i = 0; i < 4; i++)
   {
      vert[i].x -= 0.5f;
      vert[i].y += 0.5f;
   }

   void *verts;
   overlay.vert_buf->Lock(0, sizeof(vert), &verts, 0);
   memcpy(verts, vert, sizeof(vert));
   overlay.vert_buf->Unlock();

   // enable alpha
   d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
   d3d->dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   d3d->dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

   // set vertex decl for overlay
   D3DVERTEXELEMENT vElems[4] = {
      {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
      {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
      {0, 20, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
      D3DDECL_END()
   };
   LPDIRECT3DVERTEXDECLARATION vertex_decl;
   d3d->dev->CreateVertexDeclaration(vElems, &vertex_decl);
   d3d->dev->SetVertexDeclaration(vertex_decl);
   vertex_decl->Release();

   d3d->dev->SetStreamSource(0, overlay.vert_buf, 0, sizeof(overlay_vertex));

   if (overlay.fullscreen)
   {
      // set viewport to full window
      D3DVIEWPORT vp_full;
      vp_full.X = 0;
      vp_full.Y = 0;
      vp_full.Width = d3d->screen_width;
      vp_full.Height = d3d->screen_height;
      vp_full.MinZ = 0.0f;
      vp_full.MaxZ = 1.0f;
      d3d->dev->SetViewport(&vp_full);
   }

   // render overlay
   d3d->dev->SetTexture(0, overlay.tex);
   d3d->dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d->dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   d3d->dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
   d3d->dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
   if (SUCCEEDED(d3d->dev->BeginScene()))
   {
      d3d->dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
      d3d->dev->EndScene();
   }

   // restore previous state
   d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
   d3d->dev->SetViewport(&d3d->final_viewport);
}
#endif

static void d3d_set_viewport(void *data, int x, int y, unsigned width, unsigned height)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   D3DVIEWPORT viewport;

   // D3D doesn't support negative X/Y viewports ...
   if (x < 0)
      x = 0;
   if (y < 0)
      y = 0;

   viewport.X = x;
   viewport.Y = y;
   viewport.Width = width;
   viewport.Height = height;
   viewport.MinZ = 0.0f;
   viewport.MaxZ = 1.0f;

   d3d->final_viewport = viewport;

   d3d_set_font_rect(d3d, NULL);
}

static void d3d_calculate_rect(void *data, unsigned width, unsigned height,
   bool keep, float desired_aspect)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   if (g_settings.video.scale_integer)
   {
      struct rarch_viewport vp = {0};
      gfx_scale_integer(&vp, width, height, desired_aspect, keep);
      d3d_set_viewport(d3d, vp.x, vp.y, vp.width, vp.height);
   }
   else if (!keep)
      d3d_set_viewport(d3d, 0, 0, width, height);
   else
   {
      if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const rarch_viewport_t &custom = g_extern.console.screen.viewports.custom_vp;
         d3d_set_viewport(d3d, custom.x, custom.y, custom.width, custom.height);
      }
      else
      {
         float device_aspect = static_cast<float>(width) / static_cast<float>(height);
         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
            d3d_set_viewport(d3d, 0, 0, width, height);
         else if (device_aspect > desired_aspect)
         {
            float delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            d3d_set_viewport(d3d, int(roundf(width * (0.5f - delta))), 0, unsigned(roundf(2.0f * width * delta)), height);
         }
         else
         {
            float delta = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            d3d_set_viewport(d3d, 0, int(roundf(height * (0.5f - delta))), width, unsigned(roundf(2.0f * height * delta)));
         }
      }
   }
}

static bool d3d_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      const char *msg)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

  if (!frame)
      return true;

   RARCH_PERFORMANCE_INIT(d3d_frame);
   RARCH_PERFORMANCE_START(d3d_frame);
   // We cannot recover in fullscreen.
   if (d3d->needs_restore && IsIconic(d3d->hWnd))
      return true;

   if (d3d->needs_restore && !d3d_restore(d3d))
   {
      RARCH_ERR("[D3D]: Failed to restore.\n");
      return false;
   }

   if (d3d->should_resize)
   {
      d3d_calculate_rect(d3d, d3d->screen_width, d3d->screen_height, d3d->video_info.force_aspect, g_extern.system.aspect_ratio);
      d3d->chain->set_final_viewport(d3d->final_viewport);
      d3d_recompute_pass_sizes(d3d);

      d3d->should_resize = false;
   }

   // render_chain() only clears out viewport, clear out everything.
   D3DVIEWPORT screen_vp;
   screen_vp.X = 0;
   screen_vp.Y = 0;
   screen_vp.MinZ = 0;
   screen_vp.MaxZ = 1;
   screen_vp.Width = d3d->screen_width;
   screen_vp.Height = d3d->screen_height;
   d3d->dev->SetViewport(&screen_vp);
   d3d->dev->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);

   // Insert black frame first, so we can screenshot, etc.
   if (g_settings.video.black_frame_insertion)
   {
      if (d3d->dev->Present(NULL, NULL, NULL, NULL) != D3D_OK)
      {
         RARCH_ERR("[D3D]: Present() failed.\n");
         d3d->needs_restore = true;
         return true;
      }
      d3d->dev->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);
   }

   if (!d3d->chain->render(frame, width, height, pitch, d3d->dev_rotation))
   {
      RARCH_ERR("[D3D]: Failed to render scene.\n");
      return false;
   }

   d3d_render_msg(d3d, msg, NULL);

#ifdef HAVE_MENU
   if (d3d->rgui.enabled)
      d3d_overlay_render(d3d, d3d->rgui);
#endif

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled)
   {
      for (unsigned i = 0; i < d3d->overlays.size(); i++)
         d3d_overlay_render(d3d, d3d->overlays[i]);
   }
#endif

   RARCH_PERFORMANCE_STOP(d3d_frame);

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->update_window_title)
      d3d->ctx_driver->update_window_title();

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->swap_buffers)
      d3d->ctx_driver->swap_buffers();

   return true;
}

static void d3d_set_nonblock_state(void *data, bool state)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d->video_info.vsync = !state;
   d3d_restore(d3d);
}

static bool d3d_alive(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   bool quit = false, resize = false;

   if (d3d->ctx_driver && d3d->ctx_driver->check_window)
      d3d->ctx_driver->check_window(&quit, &resize, &d3d->screen_width,
      &d3d->screen_height, g_extern.frame_count);

   else if (resize)
      d3d->should_resize = true;

   return !quit;
}

static bool d3d_focus(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   if (d3d && d3d->ctx_driver && d3d->ctx_driver->has_focus)
      return d3d->ctx_driver->has_focus();
   return false;
}

static void d3d_set_rotation(void *data, unsigned rot)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d->dev_rotation = rot;
}

#ifdef HAVE_OVERLAY
void d3d_free_overlay(void *data, overlay_t *overlay)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   if (overlay->tex)
      overlay->tex->Release();
   if (overlay->vert_buf)
      overlay->vert_buf->Release();
}

void d3d_free_overlays(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   for (unsigned i = 0; i < d3d->overlays.size(); i++)
      d3d_free_overlay(d3d, &d3d->overlays[i]);
   d3d->overlays.clear();
}
#endif

static void d3d_free(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d_deinitialize(d3d);
#ifdef HAVE_OVERLAY
   d3d_free_overlays(d3d);
#endif
#ifdef HAVE_MENU
   d3d_free_overlay(d3d, &d3d->rgui);
#endif
   if (d3d->dev)
      d3d->dev->Release();
   if (d3d->g_pD3D)
      d3d->g_pD3D->Release();

#ifdef HAVE_MONITOR
   Monitor::last_hm = MonitorFromWindow(d3d->hWnd, MONITOR_DEFAULTTONEAREST);
#endif
   DestroyWindow(d3d->hWnd);

#ifndef _XBOX
   UnregisterClass("RetroArch", GetModuleHandle(NULL));
#endif
}

static void d3d_viewport_info(void *data, struct rarch_viewport *vp)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   vp->x           = d3d->final_viewport.X;
   vp->y           = d3d->final_viewport.Y;
   vp->width       = d3d->final_viewport.Width;
   vp->height      = d3d->final_viewport.Height;

   vp->full_width  = d3d->screen_width;
   vp->full_height = d3d->screen_height;
}

static bool d3d_read_viewport(void *data, uint8_t *buffer)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   RARCH_PERFORMANCE_INIT(d3d_read_viewport);
   RARCH_PERFORMANCE_START(d3d_read_viewport);
   bool ret = true;
   LPDIRECT3DSURFACE target = NULL;
   LPDIRECT3DSURFACE dest   = NULL;

   if (FAILED(d3d->d3d_err = d3d->dev->GetRenderTarget(0, &target)))
   {
      ret = false;
      goto end;
   }

   if (FAILED(d3d->d3d_err = d3d->dev->CreateOffscreenPlainSurface(d3d->screen_width,
        d3d->screen_height,
        D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM,
        &dest, NULL)))
   {
      ret = false;
      goto end;
   }

   if (FAILED(d3d->d3d_err = d3d->dev->GetRenderTargetData(target, dest)))
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

      for (unsigned y = 0; y < d3d->final_viewport.Height; y++, pixels -= pitchpix)
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
   return ret;
}

static bool d3d_set_shader(void *data, enum rarch_shader_type type, const char *path)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   std::string shader = "";
   if (path && type == RARCH_SHADER_CG)
      shader = path;

   auto old_shader = d3d->cg_shader;
   bool restore_old = false;
   d3d->cg_shader = path;

   if (!d3d_process_shader(d3d) || !d3d_restore(d3d))
   {
      RARCH_ERR("[D3D]: Setting shader failed.\n");
      restore_old = true;
   }

   if (restore_old)
   {
      d3d->cg_shader = old_shader;
      d3d_process_shader(d3d);
      d3d_restore(d3d);
   }

   return !restore_old;
}

#ifdef HAVE_MENU
static void d3d_get_poke_interface(void *data, const video_poke_interface_t **iface);
#endif

#ifdef HAVE_OVERLAY
static void d3d_overlay_tex_geom(void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   d3d->overlays[index].tex_coords.x = x;
   d3d->overlays[index].tex_coords.y = y;
   d3d->overlays[index].tex_coords.w = w;
   d3d->overlays[index].tex_coords.h = h;
}

static void d3d_overlay_vertex_geom(void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   y = 1.0f - y;
   h = -h;
   d3d->overlays[index].vert_coords.x = x;
   d3d->overlays[index].vert_coords.y = y;
   d3d->overlays[index].vert_coords.w = w;
   d3d->overlays[index].vert_coords.h = h;
}

static bool d3d_overlay_load(void *data, const texture_image *images, unsigned num_images)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d_free_overlays(data);
   d3d->overlays.resize(num_images);

   for (unsigned i = 0; i < num_images; i++)
   {
      unsigned width = images[i].width;
      unsigned height = images[i].height;
      overlay_t &overlay = d3d->overlays[i];
      if (FAILED(d3d->dev->CreateTexture(width, height, 1,
                  0,
                  D3DFMT_A8R8G8B8,
                  D3DPOOL_MANAGED,
                  &overlay.tex, NULL)))
      {
         RARCH_ERR("[D3D]: Failed to create overlay texture\n");
         return false;
      }

      D3DLOCKED_RECT d3dlr;
      if (SUCCEEDED(overlay.tex->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
      {
         uint32_t *dst = static_cast<uint32_t*>(d3dlr.pBits);
         const uint32_t *src = images[i].pixels;
         unsigned pitch = d3dlr.Pitch >> 2;
         for (unsigned y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
         overlay.tex->UnlockRect(0);
      }

      overlay.tex_w = width;
      overlay.tex_h = height;
      d3d_overlay_tex_geom(d3d, i, 0, 0, 1, 1); // Default. Stretch to whole screen.
      d3d_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d_overlay_enable(void *data, bool state)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   for (unsigned i = 0; i < d3d->overlays.size(); i++)
      d3d->overlays_enabled = state;

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->show_mouse)
      d3d->ctx_driver->show_mouse(state);
}

static void d3d_overlay_full_screen(void *data, bool enable)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   for (unsigned i = 0; i < d3d->overlays.size(); i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d_overlay_set_alpha(void *data, unsigned index, float mod)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d->overlays[index].alpha_mod = mod;
}

static const video_overlay_interface_t d3d_overlay_interface = {
   d3d_overlay_enable,
   d3d_overlay_load,
   d3d_overlay_tex_geom,
   d3d_overlay_vertex_geom,
   d3d_overlay_full_screen,
   d3d_overlay_set_alpha,
};

static void d3d_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &d3d_overlay_interface;
}
#endif

static void d3d_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         gfx_set_square_pixel_viewport(g_extern.system.av_info.geometry.base_width, g_extern.system.av_info.geometry.base_height);
         break;

      case ASPECT_RATIO_CORE:
         gfx_set_core_viewport();
         break;

      case ASPECT_RATIO_CONFIG:
         gfx_set_config_viewport();
         break;

      default:
         break;
   }

   g_extern.system.aspect_ratio = aspectratio_lut[aspect_ratio_idx].value;
   d3d->video_info.force_aspect = true;
   d3d->should_resize = true;
   return;
}

static void d3d_apply_state_changes(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d->should_resize = true;
}

static void d3d_render_msg(void *data, const char *msg, void *userdata)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d_font_msg(d3d, msg, userdata);

   if (userdata)
      d3d_set_font_rect(d3d, NULL);
}

static void d3d_set_osd_msg(void *data, const char *msg, void *userdata)
{
   font_params_t *params = (font_params_t*)userdata;
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   if (params)
      d3d_set_font_rect(d3d, params);

   d3d_render_msg(d3d, msg, params);
}

static void d3d_show_mouse(void *data, bool state)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->show_mouse)
      d3d->ctx_driver->show_mouse(state);
}

#ifdef HAVE_MENU
static void d3d_set_rgui_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   if (!d3d->rgui.tex || d3d->rgui.tex_w != width || d3d->rgui.tex_h != height)
   {
      if (d3d->rgui.tex)
         d3d->rgui.tex->Release();
      if (FAILED(d3d->dev->CreateTexture(width, height, 1,
                  0, D3DFMT_A8R8G8B8,
                  D3DPOOL_MANAGED,
                  &d3d->rgui.tex, NULL)))
      {
         RARCH_ERR("[D3D]: Failed to create rgui texture\n");
         return;
      }
      d3d->rgui.tex_w = width;
      d3d->rgui.tex_h = height;
   }

   d3d->rgui.alpha_mod = alpha;


   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(d3d->rgui.tex->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
   {
      if (rgb32)
      {
         uint8_t *dst = (uint8_t*)d3dlr.pBits;
         const uint32_t *src = (const uint32_t*)frame;
         for (unsigned h = 0; h < height; h++, dst += d3dlr.Pitch, src += width)
         {
            memcpy(dst, src, width * sizeof(uint32_t));
            memset(dst + width * sizeof(uint32_t), 0, d3dlr.Pitch - width * sizeof(uint32_t));
         }
      }
      else
      {
         uint32_t *dst = (uint32_t*)d3dlr.pBits;
         const uint16_t *src = (const uint16_t*)frame;
         for (unsigned h = 0; h < height; h++, dst += d3dlr.Pitch >> 2, src += width)
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

      d3d->rgui.tex->UnlockRect(0);
   }
}

static void d3d_set_rgui_texture_enable(void *data, bool state, bool full_screen)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d->rgui.enabled = state;
   d3d->rgui.fullscreen = full_screen;
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
   d3d_set_rgui_texture_frame,
   d3d_set_rgui_texture_enable,
#endif
   d3d_set_osd_msg,

   d3d_show_mouse,
};

static void d3d_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &d3d_poke_interface;
}

// Delay constructor due to lack of exceptions.
static bool d3d_construct(void *data, const video_info_t *info, const input_driver_t **input,
      void **input_data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d->should_resize = false;
#ifndef _XBOX
   gfx_set_dwm();
#endif

#ifdef HAVE_MENU
   memset(&d3d->rgui, 0, sizeof(d3d->rgui));
   d3d->rgui.tex_coords.x = 0;
   d3d->rgui.tex_coords.y = 0;
   d3d->rgui.tex_coords.w = 1;
   d3d->rgui.tex_coords.h = 1;
   d3d->rgui.vert_coords.x = 0;
   d3d->rgui.vert_coords.y = 1;
   d3d->rgui.vert_coords.w = 1;
   d3d->rgui.vert_coords.h = -1;
#endif

#ifdef HAVE_WINDOW
   memset(&d3d->windowClass, 0, sizeof(d3d->windowClass));
   d3d->windowClass.cbSize        = sizeof(d3d->windowClass);
   d3d->windowClass.style         = CS_HREDRAW | CS_VREDRAW;
   d3d->windowClass.lpfnWndProc   = WindowProc;
   d3d->windowClass.hInstance     = NULL;
   d3d->windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
   d3d->windowClass.lpszClassName = "RetroArch";
   d3d->windowClass.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
   d3d->windowClass.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
   if (!info->fullscreen)
      d3d->windowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;

   RegisterClassEx(&d3d->windowClass);
#endif

#ifdef HAVE_MONITOR
   RECT mon_rect = d3d_monitor_rect(d3d);
#endif

   bool windowed_full = g_settings.video.windowed_fullscreen;

   unsigned full_x = (windowed_full || info->width  == 0) ? (mon_rect.right  - mon_rect.left) : info->width;
   unsigned full_y = (windowed_full || info->height == 0) ? (mon_rect.bottom - mon_rect.top)  : info->height;
   RARCH_LOG("[D3D]: Monitor size: %dx%d.\n", (int)(mon_rect.right  - mon_rect.left), (int)(mon_rect.bottom - mon_rect.top));

   d3d->screen_width  = info->fullscreen ? full_x : info->width;
   d3d->screen_height = info->fullscreen ? full_y : info->height;

   unsigned win_width  = d3d->screen_width;
   unsigned win_height = d3d->screen_height;

#ifdef HAVE_WINDOW
   if (!info->fullscreen)
   {
      RECT rect   = {0};
      rect.right  = d3d->screen_width;
      rect.bottom = d3d->screen_height;
      AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
      win_width  = rect.right - rect.left;
      win_height = rect.bottom - rect.top;
   }

   char buffer[128];
   gfx_get_fps(buffer, sizeof(buffer), NULL, 0);
   std::string title = buffer;
   title += " || Direct3D";

   d3d->hWnd = CreateWindowEx(0, "RetroArch", title.c_str(),
         info->fullscreen ?
         (WS_EX_TOPMOST | WS_POPUP) : WS_OVERLAPPEDWINDOW,
         info->fullscreen ? mon_rect.left : CW_USEDEFAULT,
         info->fullscreen ? mon_rect.top  : CW_USEDEFAULT,
         win_width, win_height,
         NULL, NULL, NULL, d3d);
#endif

   driver.display_type  = RARCH_DISPLAY_WIN32;
   driver.video_display = 0;
   driver.video_window  = (uintptr_t)d3d->hWnd;

#ifdef HAVE_WINDOW
   if (d3d && d3d->ctx_driver && d3d->ctx_driver->show_mouse)
      d3d->ctx_driver->show_mouse(!info->fullscreen
#ifdef HAVE_OVERLAY
      || d3d->overlays_enabled
#endif
   );

   ShowWindow(d3d->hWnd, SW_RESTORE);
   UpdateWindow(d3d->hWnd);
   SetForegroundWindow(d3d->hWnd);
   SetFocus(d3d->hWnd);
#endif

   // This should only be done once here
   // to avoid set_shader() to be overridden
   // later.
#ifdef HAVE_CG
   enum rarch_shader_type type = gfx_shader_parse_type(g_settings.video.shader_path, RARCH_SHADER_NONE);
   if (g_settings.video.shader_enable && type == RARCH_SHADER_CG)
      d3d->cg_shader = g_settings.video.shader_path;

   if (!d3d_process_shader(d3d))
      return false;
#endif

   d3d->video_info = *info;
   if (!d3d_initialize(d3d, &d3d->video_info))
      return false;

   if (input && input_data &&
      d3d->ctx_driver && d3d->ctx_driver->input_driver)
      d3d->ctx_driver->input_driver(input, input_data);

   RARCH_LOG("[D3D]: Init complete.\n");
   return true;
}

static const gfx_ctx_driver_t *d3d_get_context(void)
{
   // TODO: GL core contexts through ANGLE?
   enum gfx_ctx_api api = GFX_CTX_DIRECT3D9_API;
   unsigned major = 0;
   unsigned minor = 0;
   return gfx_ctx_init_first(api, major, minor);
}

static void *d3d_init(const video_info_t *info, const input_driver_t **input,
      void **input_data)
{
   D3DVideo *vid = (D3DVideo*)calloc(1, sizeof(D3DVideo));
   if (!vid)
      return NULL;

   vid->ctx_driver = d3d_get_context();
   if (!vid->ctx_driver)
   {
      free(vid);
      return NULL;
   }

   //default values
   vid->g_pD3D           = NULL;
   vid->dev              = NULL;
   vid->font             = NULL;
   vid->dev_rotation     = 0;
   vid->needs_restore    = false;
#ifdef HAVE_CG
   vid->cgCtx            = NULL;
#endif
#ifdef HAVE_OVERLAY
   vid->overlays_enabled = false;
#endif
   vid->chain            = NULL;

   if (!d3d_construct(vid, info, input, input_data))
   {
      RARCH_ERR("[D3D]: Failed to init D3D.\n");
      free(vid);
      return NULL;
   }

   return vid;
}

const video_driver_t video_d3d = {
   d3d_init,
   d3d_frame,
   d3d_set_nonblock_state,
   d3d_alive,
   d3d_focus,
   d3d_set_shader,
   d3d_free,
   "d3d9",
#ifdef HAVE_MENU
   NULL,
#endif
   d3d_set_rotation,
   d3d_viewport_info,
   d3d_read_viewport,
#ifdef HAVE_OVERLAY
   d3d_get_overlay_interface,
#endif
   d3d_get_poke_interface
};