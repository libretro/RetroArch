/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012      - OV2
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

#ifdef _MSC_VER
#pragma comment( lib, "d3d9" )
#pragma comment( lib, "d3dx9" )
#pragma comment( lib, "cgd3d9" )
#pragma comment( lib, "dxguid" )
#endif

#include "d3d9.hpp"
#include "render_chain.hpp"
#include "../../file.h"
#include "../gfx_common.h"
#include "../../compat/posix_string.h"
#include "../../performance.h"

#include <iostream>
#include <exception>
#include <stdexcept>
#include <cstring>
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <cmath>
#include <algorithm>

#undef min
#undef max

#define IDI_ICON 1
#define MAX_MONITORS 9

/* TODO: Make Cg optional - same as in the GL driver where we can either bake in
 * Cg or HLSL shader support */

namespace Monitor
{
   static HMONITOR last_hm;
   static HMONITOR all_hms[MAX_MONITORS];
   static unsigned num_mons;
   static unsigned cur_mon_id;
}

namespace Callback
{
   static bool quit = false;
   static D3DVideo *curD3D = nullptr;
   static HRESULT d3d_err;

   LRESULT CALLBACK WindowProc(HWND hWnd, UINT message,
         WPARAM wParam, LPARAM lParam)
   {
      switch (message)
      {
         case WM_CREATE:
            LPCREATESTRUCT p_cs;
            p_cs = (LPCREATESTRUCT)lParam;
            curD3D = (D3DVideo*)p_cs->lpCreateParams;
            break;

         case WM_SYSKEYDOWN:
            switch (wParam)
            {
               case VK_F10:
               case VK_RSHIFT:
                  return 0;
            }
            break;

         case WM_DESTROY:
            quit = true;
            return 0;

         case WM_SIZE:
            unsigned new_width, new_height;
            new_width = LOWORD(lParam);
            new_height = HIWORD(lParam);

            if (new_width && new_height)
               curD3D->resize(new_width, new_height);
            return 0;

         default:
            return DefWindowProc(hWnd, message, wParam, lParam);
      }
      return DefWindowProc(hWnd, message, wParam, lParam);
   }
}

void D3DVideo::init_base(const video_info_t &info)
{
   D3DPRESENT_PARAMETERS d3dpp;
   make_d3dpp(info, d3dpp);

   g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
   if (!g_pD3D)
      throw std::runtime_error("Failed to create D3D9 interface!");

   if (FAILED(Callback::d3d_err = g_pD3D->CreateDevice(
               Monitor::cur_mon_id,
               D3DDEVTYPE_HAL,
               hWnd,
               D3DCREATE_HARDWARE_VERTEXPROCESSING,
               &d3dpp,
               &dev)))
   {
      RARCH_WARN("[D3D9]: Failed to init device with hardware vertex processing (code: 0x%x). Trying to fall back to software vertex processing.\n",
                 (unsigned)Callback::d3d_err);

      if (FAILED(Callback::d3d_err = g_pD3D->CreateDevice(
                  Monitor::cur_mon_id,
                  D3DDEVTYPE_HAL,
                  hWnd,
                  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                  &d3dpp,
                  &dev)))
      {
         throw std::runtime_error("Failed to init device");
      }
   }
}

void D3DVideo::make_d3dpp(const video_info_t &info, D3DPRESENT_PARAMETERS &d3dpp)
{
   std::memset(&d3dpp, 0, sizeof(d3dpp));

   d3dpp.Windowed = g_settings.video.windowed_fullscreen || !info.fullscreen;

   d3dpp.PresentationInterval = info.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
   d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
   d3dpp.hDeviceWindow = hWnd;
   d3dpp.BackBufferCount = 2;
   d3dpp.BackBufferFormat = !d3dpp.Windowed ? D3DFMT_X8R8G8B8 : D3DFMT_UNKNOWN;

   if (!d3dpp.Windowed)
   {
      d3dpp.BackBufferWidth = screen_width;
      d3dpp.BackBufferHeight = screen_height;
   }
}

void D3DVideo::init(const video_info_t &info)
{
   if (!g_pD3D)
      init_base(info);
   else if (needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;
      make_d3dpp(info, d3dpp);
      if (dev->Reset(&d3dpp) != D3D_OK)
      {
         HRESULT res = dev->TestCooperativeLevel();
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
         RARCH_WARN("[D3D9]: Attempting to recover from dead state (%s).\n", err);
         deinit(); 
         g_pD3D->Release();
         g_pD3D = nullptr;
         init_base(info);
         RARCH_LOG("[D3D9]: Recovered from dead state.\n");
      }
   }

   calculate_rect(screen_width, screen_height, info.force_aspect, g_extern.system.aspect_ratio);

#ifdef HAVE_CG
   if (!init_cg())
      throw std::runtime_error("Failed to init Cg");
#endif
   if (!init_chain(info))
      throw std::runtime_error("Failed to init render chain");
   if (!init_font())
      throw std::runtime_error("Failed to init Font");
}

void D3DVideo::set_viewport(int x, int y, unsigned width, unsigned height)
{
   D3DVIEWPORT9 viewport;
   viewport.X = std::max(x, 0); // D3D9 doesn't support negative X/Y viewports ...
   viewport.Y = std::max(y, 0);
   viewport.Width = width;
   viewport.Height = height;
   viewport.MinZ = 0.0f;
   viewport.MaxZ = 1.0f;

   final_viewport = viewport;

   set_font_rect(nullptr);
}

void D3DVideo::set_font_rect(font_params_t *params)
{
   float pos_x = g_settings.video.msg_pos_x;
   float pos_y = g_settings.video.msg_pos_y;
   float font_size = g_settings.video.font_size;

   if (params)
   {
      pos_x = params->x;
      pos_y = params->y;
      font_size *= params->scale;
   }

   font_rect.left = final_viewport.X + final_viewport.Width * pos_x;
   font_rect.right = final_viewport.X + final_viewport.Width;
   font_rect.top = final_viewport.Y + (1.0f - pos_y) * final_viewport.Height - font_size; 
   font_rect.bottom = final_viewport.Height;

   font_rect_shifted = font_rect;
   font_rect_shifted.left -= 2;
   font_rect_shifted.right -= 2;
   font_rect_shifted.top += 2;
   font_rect_shifted.bottom += 2;
}

void D3DVideo::set_rotation(unsigned rot)
{
   rotation = rot;
}

void D3DVideo::viewport_info(rarch_viewport &vp)
{
   vp.x      = final_viewport.X;
   vp.y      = final_viewport.Y;
   vp.width  = final_viewport.Width;
   vp.height = final_viewport.Height;

   vp.full_width  = screen_width;
   vp.full_height = screen_height;
}

bool D3DVideo::read_viewport(uint8_t *buffer)
{
   RARCH_PERFORMANCE_INIT(d3d_read_viewport);
   RARCH_PERFORMANCE_START(d3d_read_viewport);
   bool ret = true;
   IDirect3DSurface9 *target = nullptr;
   IDirect3DSurface9 *dest   = nullptr;

   if (FAILED(Callback::d3d_err = dev->GetRenderTarget(0, &target)))
   {
      ret = false;
      goto end;
   }

   if (FAILED(Callback::d3d_err = dev->CreateOffscreenPlainSurface(screen_width, screen_height,
        D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM,
        &dest, nullptr)))
   {
      ret = false;
      goto end;
   }

   if (FAILED(Callback::d3d_err = dev->GetRenderTargetData(target, dest)))
   {
      ret = false;
      goto end;
   }

   D3DLOCKED_RECT rect;
   if (SUCCEEDED(dest->LockRect(&rect, nullptr, D3DLOCK_READONLY)))
   {
      unsigned pitchpix = rect.Pitch / 4;
      const uint32_t *pixels = (const uint32_t*)rect.pBits;
      pixels += final_viewport.X;
      pixels += (final_viewport.Height - 1) * pitchpix;
      pixels -= final_viewport.Y * pitchpix;

      for (unsigned y = 0; y < final_viewport.Height; y++, pixels -= pitchpix)
      {
         for (unsigned x = 0; x < final_viewport.Width; x++)
         {
            *buffer++ = (pixels[x] >>  0) & 0xff;
            *buffer++ = (pixels[x] >>  8) & 0xff;
            *buffer++ = (pixels[x] >> 16) & 0xff;
         }
      }

      dest->UnlockRect();
   }
   else
   {
      ret = false;
      goto end;
   }

end:
   RARCH_PERFORMANCE_STOP(d3d_read_viewport);
   if (target)
      target->Release();
   if (dest)
      dest->Release();
   return ret;
}

void D3DVideo::calculate_rect(unsigned width, unsigned height,
   bool keep, float desired_aspect)
{
   if (g_settings.video.scale_integer)
   {
      struct rarch_viewport vp = {0};
      gfx_scale_integer(&vp, width, height, desired_aspect, keep);
      set_viewport(vp.x, vp.y, vp.width, vp.height);
   }
   else if (!keep)
      set_viewport(0, 0, width, height);
   else
   {
      if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const rarch_viewport_t &custom = g_extern.console.screen.viewports.custom_vp;
         set_viewport(custom.x, custom.y, custom.width, custom.height);
      }
      else
      {
         float device_aspect = static_cast<float>(width) / static_cast<float>(height);
         if (fabs(device_aspect - desired_aspect) < 0.0001)
            set_viewport(0, 0, width, height);
         else if (device_aspect > desired_aspect)
         {
            float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
            set_viewport(width * (0.5 - delta), 0, 2.0 * width * delta, height);
         }
         else
         {
            float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
            set_viewport(0, height * (0.5 - delta), width, 2.0 * height * delta);
         }
      }
   }
}

static void show_cursor(bool show)
{
   if (show)
      while (ShowCursor(TRUE) < 0);
   else
      while (ShowCursor(FALSE) >= 0);
}

static BOOL CALLBACK monitor_enum_proc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
   Monitor::all_hms[Monitor::num_mons++] = hMonitor;
   return TRUE;
}

// Multi-monitor support.
RECT D3DVideo::monitor_rect()
{
   Monitor::num_mons = 0;
   EnumDisplayMonitors(nullptr, nullptr, monitor_enum_proc, 0);

   if (!Monitor::last_hm)
      Monitor::last_hm = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
   HMONITOR hm_to_use = Monitor::last_hm;

   unsigned fs_monitor = g_settings.video.monitor_index;
   if (fs_monitor && fs_monitor <= Monitor::num_mons && Monitor::all_hms[fs_monitor - 1])
   {
      hm_to_use = Monitor::all_hms[fs_monitor - 1];
      Monitor::cur_mon_id = fs_monitor - 1;
   }
   else
   {
      for (unsigned i = 0; i < Monitor::num_mons; i++)
      {
         if (Monitor::all_hms[i] == hm_to_use)
         {
            Monitor::cur_mon_id = i;
            break;
         }
      }
   }

   MONITORINFOEX current_mon;
   std::memset(&current_mon, 0, sizeof(current_mon));
   current_mon.cbSize = sizeof(MONITORINFOEX);
   GetMonitorInfo(hm_to_use, (MONITORINFO*)&current_mon);

   return current_mon.rcMonitor;
}

D3DVideo::D3DVideo(const video_info_t *info) :
   g_pD3D(nullptr), dev(nullptr), font(nullptr),
   rotation(0), needs_restore(false), cgCtx(nullptr)
{
   should_resize = false;
   gfx_set_dwm();

#ifdef HAVE_OVERLAY
   std::memset(&overlay, 0, sizeof(overlay));
#endif

#ifdef HAVE_RGUI
   std::memset(&rgui, 0, sizeof(rgui));
   rgui.tex_coords.x = 0;
   rgui.tex_coords.y = 0;
   rgui.tex_coords.w = 1;
   rgui.tex_coords.h = 1;
   rgui.vert_coords.x = 0;
   rgui.vert_coords.y = 1;
   rgui.vert_coords.w = 1;
   rgui.vert_coords.h = -1;
#endif

   std::memset(&windowClass, 0, sizeof(windowClass));
   windowClass.cbSize        = sizeof(windowClass);
   windowClass.style         = CS_HREDRAW | CS_VREDRAW;
   windowClass.lpfnWndProc   = Callback::WindowProc;
   windowClass.hInstance     = nullptr;
   windowClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
   windowClass.lpszClassName = "RetroArch";
   windowClass.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
   windowClass.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
   if (!info->fullscreen)
      windowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;

   RegisterClassEx(&windowClass);
   RECT mon_rect = monitor_rect();

   bool windowed_full = g_settings.video.windowed_fullscreen;

   unsigned full_x = (windowed_full || info->width  == 0) ? (mon_rect.right  - mon_rect.left) : info->width;
   unsigned full_y = (windowed_full || info->height == 0) ? (mon_rect.bottom - mon_rect.top)  : info->height;
   RARCH_LOG("[D3D9]: Monitor size: %dx%d.\n", (int)(mon_rect.right  - mon_rect.left), (int)(mon_rect.bottom - mon_rect.top));

   screen_width  = info->fullscreen ? full_x : info->width;
   screen_height = info->fullscreen ? full_y : info->height;

   unsigned win_width  = screen_width;
   unsigned win_height = screen_height;

   if (!info->fullscreen)
   {
      RECT rect   = {0};
      rect.right  = screen_width;
      rect.bottom = screen_height;
      AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
      win_width  = rect.right - rect.left;
      win_height = rect.bottom - rect.top;
   }

   char buffer[128];
   gfx_get_fps(buffer, sizeof(buffer), false);
   std::string title = buffer;
   title += " || Direct3D9";

   hWnd = CreateWindowEx(0, "RetroArch", title.c_str(),
         info->fullscreen ?
         (WS_EX_TOPMOST | WS_POPUP) : WS_OVERLAPPEDWINDOW,
         info->fullscreen ? mon_rect.left : CW_USEDEFAULT,
         info->fullscreen ? mon_rect.top  : CW_USEDEFAULT,
         win_width, win_height,
         nullptr, nullptr, nullptr, this);

   driver.display_type  = RARCH_DISPLAY_WIN32;
   driver.video_display = 0;
   driver.video_window  = (uintptr_t)hWnd;

   show_cursor(!info->fullscreen
#ifdef HAVE_OVERLAY
      || overlay.enabled
#endif
   );
   Callback::quit = false;

   ShowWindow(hWnd, SW_RESTORE);
   UpdateWindow(hWnd);
   SetForegroundWindow(hWnd);
   SetFocus(hWnd);

   // This should only be done once here
   // to avoid set_shader() to be overridden
   // later.
#ifdef HAVE_CG
   enum rarch_shader_type type = gfx_shader_parse_type(g_settings.video.shader_path, RARCH_SHADER_NONE);
   if (g_settings.video.shader_enable && type == RARCH_SHADER_CG)
      cg_shader = g_settings.video.shader_path;
#endif

   process_shader();

   video_info = *info;
   init(video_info);

   RARCH_LOG("[D3D9]: Init complete.\n");
}

void D3DVideo::deinit()
{
   deinit_font();
   deinit_chain();
   deinit_cg();

   needs_restore = false;
}

D3DVideo::~D3DVideo()
{
   deinit();
#ifdef HAVE_OVERLAY
   if (overlay.tex)
      overlay.tex->Release();
   if (overlay.vert_buf)
      overlay.vert_buf->Release();
#endif
#ifdef HAVE_RGUI
   if (rgui.tex)
      rgui.tex->Release();
   if (rgui.vert_buf)
      rgui.vert_buf->Release();
#endif
   if (dev)
      dev->Release();
   if (g_pD3D)
      g_pD3D->Release();

   Monitor::last_hm = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
   DestroyWindow(hWnd);

   UnregisterClass("RetroArch", GetModuleHandle(nullptr));
}

bool D3DVideo::restore()
{
   deinit();
   try
   {
      needs_restore = true;
      init(video_info);
      needs_restore = false;
   }
   catch (const std::exception &e)
   {
      RARCH_ERR("[D3D9]: Restore error: (%s).\n", e.what());
      needs_restore = true;
   }

   return !needs_restore;
}

bool D3DVideo::frame(const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      const char *msg)
{
   if (!frame)
      return true;

   RARCH_PERFORMANCE_INIT(d3d_frame);
   RARCH_PERFORMANCE_START(d3d_frame);
   // We cannot recover in fullscreen.
   if (needs_restore && IsIconic(hWnd))
      return true;

   if (needs_restore && !restore())
   {
      RARCH_ERR("[D3D9]: Failed to restore.\n");
      return false;
   }

   if (should_resize)
   {
      calculate_rect(screen_width, screen_height, video_info.force_aspect, g_extern.system.aspect_ratio);
      chain->set_final_viewport(final_viewport);
      recompute_pass_sizes();

      // render_chain() only clears out viewport, clear out everything.
      D3DRECT clear_rect;
      clear_rect.x1 = clear_rect.y1 = 0;
      clear_rect.x2 = screen_width;
      clear_rect.y2 = screen_height;
      dev->Clear(1, &clear_rect, D3DCLEAR_TARGET, 0, 1, 0);

      should_resize = false;
   }

   if (!chain->render(frame, width, height, pitch, rotation))
   {
      RARCH_ERR("[D3D9]: Failed to render scene.\n");
      return false;
   }

   render_msg(msg);

#ifdef HAVE_RGUI
   if (rgui.enabled)
      overlay_render(rgui);
#endif

#ifdef HAVE_OVERLAY
   if (overlay.enabled)
      overlay_render(overlay);
#endif

   RARCH_PERFORMANCE_STOP(d3d_frame);

   if (dev->Present(nullptr, nullptr, nullptr, nullptr) != D3D_OK)
   {
      needs_restore = true;
      return true;
   }

   update_title();
   return true;
}

void D3DVideo::render_msg(const char *msg, font_params_t *params)
{
   if (params)
      set_font_rect(params);

   if (msg && SUCCEEDED(dev->BeginScene()))
   {
      font->DrawTextA(nullptr,
            msg,
            -1,
            &font_rect_shifted,
            DT_LEFT,
            ((font_color >> 2) & 0x3f3f3f) | 0xff000000);

      font->DrawTextA(nullptr,
            msg,
            -1,
            &font_rect,
            DT_LEFT,
            font_color | 0xff000000);

      dev->EndScene();
   }

   if (params)
      set_font_rect(NULL);
}

void D3DVideo::set_nonblock_state(bool state)
{
   video_info.vsync = !state;
   restore();
}

bool D3DVideo::alive()
{
   process();
   return !Callback::quit;
}

bool D3DVideo::focus() const
{
   return GetFocus() == hWnd;
}

void D3DVideo::process()
{
   MSG msg;
   while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
}

#ifdef HAVE_CG
bool D3DVideo::init_cg()
{
   cgCtx = cgCreateContext();
   if (cgCtx == nullptr)
      return false;

   RARCH_LOG("[D3D9 Cg]: Created context.\n");

   HRESULT ret = cgD3D9SetDevice(dev);
   if (FAILED(ret))
      return false;

   return true;
}

void D3DVideo::deinit_cg()
{
   if (cgCtx)
   {
      cgD3D9UnloadAllPrograms();
      cgD3D9SetDevice(nullptr);
      cgDestroyContext(cgCtx);
      cgCtx = nullptr;
   }
}
#endif

void D3DVideo::init_singlepass()
{
   std::memset(&shader, 0, sizeof(shader));
   shader.passes = 1;
   gfx_shader_pass &pass = shader.pass[0];
   strlcpy(pass.source.cg, cg_shader.c_str(), sizeof(pass.source.cg));
}

void D3DVideo::init_imports()
{
   if (!shader.variables)
      return;

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

   tracker_info.script_class = *shader.script_class ? shader.script_class : nullptr;
#endif

   state_tracker_t *state_tracker = state_tracker_init(&tracker_info);
   if (!state_tracker)
      throw std::runtime_error("Failed to initialize state tracker.");

   std::shared_ptr<state_tracker_t> tracker(state_tracker, [](state_tracker_t *tracker) {
            state_tracker_free(tracker);
         });

   chain->add_state_tracker(tracker);
}

void D3DVideo::init_luts()
{
   for (unsigned i = 0; i < shader.luts; i++)
   {
      chain->add_lut(shader.lut[i].id, shader.lut[i].path,
         shader.lut[i].filter == RARCH_FILTER_UNSPEC ?
            g_settings.video.smooth :
            (shader.lut[i].filter == RARCH_FILTER_LINEAR));
   }
}

void D3DVideo::init_multipass()
{
   config_file_t *conf = config_file_new(cg_shader.c_str());
   if (!conf)
      throw std::runtime_error("Failed to load preset");

   std::memset(&shader, 0, sizeof(shader));

   if (!gfx_shader_read_conf_cgp(conf, &shader))
   {
      config_file_free(conf);
      throw std::runtime_error("Failed to parse CGP file.");
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
}

bool D3DVideo::set_shader(const std::string &path)
{
   auto old_shader = cg_shader;
   bool restore_old = false;
   try
   {
      cg_shader = path;
      process_shader();
      restore();
   }
   catch (const std::exception &e)
   {
      RARCH_ERR("[D3D9]: Setting shader failed: (%s).\n", e.what());
      restore_old = true;
   }

   if (restore_old)
   {
      cg_shader = old_shader;
      process_shader();
      restore();
   }

   return !restore_old;
}

void D3DVideo::process_shader()
{
   if (strcmp(path_get_extension(cg_shader.c_str()), "cgp") == 0)
      init_multipass();
   else
      init_singlepass();
}

void D3DVideo::recompute_pass_sizes()
{
   try
   {
      LinkInfo link_info = {0};
      link_info.pass = &shader.pass[0];
      link_info.tex_w = link_info.tex_h = video_info.input_scale * RARCH_SCALE_BASE;

      unsigned current_width = link_info.tex_w;
      unsigned current_height = link_info.tex_h;
      unsigned out_width = 0;
      unsigned out_height = 0;

      chain->set_pass_size(0, current_width, current_height);
      for (unsigned i = 1; i < shader.passes; i++)
      {
         RenderChain::convert_geometry(link_info,
               out_width, out_height,
               current_width, current_height, final_viewport);

         link_info.tex_w = next_pow2(out_width);
         link_info.tex_h = next_pow2(out_height);

         chain->set_pass_size(i, link_info.tex_w, link_info.tex_h);

         current_width = out_width;
         current_height = out_height;

         link_info.pass = &shader.pass[i];
      }
   }
   catch (const std::exception& e)
   {
      RARCH_ERR("[D3D9]: Render chain error: (%s).\n", e.what());
   }
}

bool D3DVideo::init_chain(const video_info_t &video_info)
{
   try
   {
      // Setup information for first pass.
      LinkInfo link_info = {0};

      link_info.pass = &shader.pass[0];
      link_info.tex_w = link_info.tex_h = video_info.input_scale * RARCH_SCALE_BASE;

      chain = std::unique_ptr<RenderChain>(
            new RenderChain(
               video_info,
               dev, cgCtx,
               link_info,
               video_info.rgb32 ? RenderChain::ARGB : RenderChain::RGB565,
               final_viewport));

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

         chain->add_pass(link_info);
      }

      init_luts();
      init_imports();
   }
   catch (const std::exception &e)
   {
      RARCH_ERR("[D3D9]: Render chain error: (%s).\n", e.what());
      return false;
   }

   return true;
}

void D3DVideo::deinit_chain()
{
   chain.reset();
}

bool D3DVideo::init_font()
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

void D3DVideo::deinit_font()
{
   if (font)
      font->Release();
   font = nullptr;
}

void D3DVideo::update_title()
{
   char buffer[128];
   if (gfx_get_fps(buffer, sizeof(buffer), false))
   {
      std::string title = buffer;
      title += " || Direct3D9";
      SetWindowText(hWnd, title.c_str());
   }

   g_extern.frame_count++;
}

void D3DVideo::resize(unsigned new_width, unsigned new_height)
{
   if (!dev)
      return;

   RARCH_LOG("[D3D9]: Resize %ux%u.\n", new_width, new_height);

   if (new_width != video_info.width || new_height != video_info.height)
   {
      video_info.width = screen_width = new_width;
      video_info.height = screen_height = new_height;
      restore();
   }
}

#ifdef HAVE_OVERLAY
bool D3DVideo::overlay_load(const uint32_t *image, unsigned width, unsigned height)
{
   if (overlay.tex)
      overlay.tex->Release();
   if (FAILED(dev->CreateTexture(width, height, 1,
               0,
               D3DFMT_A8R8G8B8,
               D3DPOOL_MANAGED,
               &overlay.tex, nullptr)))
   {
      RARCH_ERR("[D3D9]: Failed to create overlay texture\n");
      return false;
   }
   
   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(overlay.tex->LockRect(0, &d3dlr, nullptr, D3DLOCK_NOSYSLOCK)))
   {
      std::memcpy(d3dlr.pBits, image, height * d3dlr.Pitch);
      overlay.tex->UnlockRect(0);
   }

   overlay.tex_w = width;
   overlay.tex_h = height;

   overlay_tex_geom(0, 0, 1, 1); // Default. Stretch to whole screen.
   overlay_vertex_geom(0, 0, 1, 1);

   return true;
}

void D3DVideo::overlay_tex_geom(float x, float y, float w, float h)
{
   overlay.tex_coords.x = x;
   overlay.tex_coords.y = y;
   overlay.tex_coords.w = w;
   overlay.tex_coords.h = h;
}

void D3DVideo::overlay_vertex_geom(float x, float y, float w, float h)
{
   y = 1.0f - y;
   h = -h;
   overlay.vert_coords.x = x;
   overlay.vert_coords.y = y;
   overlay.vert_coords.w = w;
   overlay.vert_coords.h = h;
}

void D3DVideo::overlay_enable(bool state)
{
   overlay.enabled = state;
   show_cursor(state);
}

void D3DVideo::overlay_full_screen(bool enable)
{
   overlay.fullscreen = enable;
}

void D3DVideo::overlay_set_alpha(float mod)
{
   overlay.alpha_mod = mod;
}

#endif

void D3DVideo::overlay_render(overlay_t &overlay)
{  
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
      dev->CreateVertexBuffer(
            sizeof(vert),
            dev->GetSoftwareVertexProcessing() ? D3DUSAGE_SOFTWAREPROCESSING : 0,
            0,
            D3DPOOL_MANAGED,
            &overlay.vert_buf,
            nullptr);
   }

   for (unsigned i = 0; i < 4; i++)
   {
      vert[i].z = 0.5f;
      vert[i].r = vert[i].g = vert[i].b = 1.0f;
      vert[i].a = overlay.alpha_mod;
   }

   float overlay_width = final_viewport.Width;
   float overlay_height = final_viewport.Height;

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
   std::memcpy(verts, vert, sizeof(vert));
   overlay.vert_buf->Unlock();

   // enable alpha
   dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
   dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

   // set vertex decl for overlay
   D3DVERTEXELEMENT9 vElems[4] = {
      {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
      {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
      {0, 20, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
      D3DDECL_END()
   };
   IDirect3DVertexDeclaration9 * vertex_decl;
   dev->CreateVertexDeclaration(vElems, &vertex_decl);
   dev->SetVertexDeclaration(vertex_decl);
   vertex_decl->Release();

   dev->SetStreamSource(0, overlay.vert_buf, 0, sizeof(overlay_vertex));

   if (overlay.fullscreen)
   {
      // set viewport to full window
      D3DVIEWPORT9 vp_full;
      vp_full.X = 0;
      vp_full.Y = 0;
      vp_full.Width = screen_width;
      vp_full.Height = screen_height;
      vp_full.MinZ = 0.0f;
      vp_full.MaxZ = 1.0f;
      dev->SetViewport(&vp_full);

      // clear new area
      D3DRECT clear_rects[2];
      clear_rects[0].y2 = clear_rects[1].y2 = vp_full.Height;
      clear_rects[0].y1 = clear_rects[1].y1 = 0;
      clear_rects[0].x1 = 0;
      clear_rects[0].x2 = final_viewport.X;
      clear_rects[1].x1 = final_viewport.X + final_viewport.Width;
      clear_rects[1].x2 = vp_full.Width;

      dev->Clear(2, clear_rects, D3DCLEAR_TARGET, 0, 1, 0);
   }

   // render overlay
   dev->SetTexture(0, overlay.tex);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
   dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
   if (SUCCEEDED(dev->BeginScene()))
   {
      dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
      dev->EndScene();
   }

   //restore previous state
   dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
   dev->SetViewport(&final_viewport);
}

#ifdef HAVE_RGUI
void D3DVideo::set_rgui_texture_frame(const void *frame,
      bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   if (!rgui.tex || rgui.tex_w != width || rgui.tex_h != height)
   {
      if (rgui.tex)
         rgui.tex->Release();
      if (FAILED(dev->CreateTexture(width, height, 1,
                  0, D3DFMT_A8R8G8B8,
                  D3DPOOL_MANAGED,
                  &rgui.tex, nullptr)))
      {
         RARCH_ERR("[D3D9]: Failed to create rgui texture\n");
         return;
      }
      rgui.tex_w = width;
      rgui.tex_h = height;
   }

   rgui.alpha_mod = alpha;


   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(rgui.tex->LockRect(0, &d3dlr, nullptr, D3DLOCK_NOSYSLOCK)))
   {
      if (rgb32)
      {
         uint8_t *dst = (uint8_t*)d3dlr.pBits;
         const uint32_t *src = (const uint32_t*)frame;
         for (unsigned h = 0; h < height; h++, dst += d3dlr.Pitch, src += width)
         {
            std::memcpy(dst, src, width * sizeof(uint32_t));
            std::memset(dst + width * sizeof(uint32_t), 0, d3dlr.Pitch - width * sizeof(uint32_t));
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

      rgui.tex->UnlockRect(0);
   }
}

void D3DVideo::set_rgui_texture_enable(bool state, bool fullscreen)
{
   rgui.enabled = state;
   rgui.fullscreen = fullscreen;
}
#endif

static void *d3d9_init(const video_info_t *info, const input_driver_t **input,
      void **input_data)
{
   try
   {
     D3DVideo *vid = new D3DVideo(info);
     if (!vid)
        return nullptr;

     if (input && input_data)
     {
        void *dinput = input_dinput.init();
        *input       = dinput ? &input_dinput : nullptr;
        *input_data  = dinput;
     }

     return vid;
   }
   catch (const std::exception &e)
   {
      RARCH_ERR("[D3D9]: Failed to init D3D9 (%s, code: 0x%x).\n", e.what(), (unsigned)Callback::d3d_err);
      return nullptr;
   }
}

static bool d3d9_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch,
      const char *msg)
{
   return reinterpret_cast<D3DVideo*>(data)->frame(frame,
         width, height, pitch, msg);
}

static void d3d9_set_nonblock_state(void *data, bool state)
{
   reinterpret_cast<D3DVideo*>(data)->set_nonblock_state(state);
}

static bool d3d9_alive(void *data)
{
   return reinterpret_cast<D3DVideo*>(data)->alive();
}

static bool d3d9_focus(void *data)
{
   return reinterpret_cast<D3DVideo*>(data)->focus();
}

static void d3d9_set_rotation(void *data, unsigned rot)
{
   reinterpret_cast<D3DVideo*>(data)->set_rotation(rot);
}

static void d3d9_free(void *data)
{
   delete reinterpret_cast<D3DVideo*>(data);
}

static void d3d9_viewport_info(void *data, struct rarch_viewport *vp)
{
   reinterpret_cast<D3DVideo*>(data)->viewport_info(*vp);
}

static bool d3d9_read_viewport(void *data, uint8_t *buffer)
{
   return reinterpret_cast<D3DVideo*>(data)->read_viewport(buffer);
}

static bool d3d9_set_shader(void *data, enum rarch_shader_type type, const char *path)
{
#ifdef HAVE_CG
   if (type != RARCH_SHADER_CG)
   {
      RARCH_ERR("[D3D9]: Only Cg shaders supported.\n");
      return false;
   }
#endif

   std::string shader = "";
   if (path)
      shader = path;

   return reinterpret_cast<D3DVideo*>(data)->set_shader(shader);
}

#if defined(HAVE_RGUI)
static void d3d9_get_poke_interface(void *data, const video_poke_interface_t **iface);
#endif

#ifdef HAVE_OVERLAY
static bool d3d9_overlay_load(void *data, const uint32_t *image, unsigned width, unsigned height)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_load(image, width, height);
}

static void d3d9_overlay_tex_geom(void *data,
      float x, float y,
      float w, float h)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_tex_geom(x, y, w, h);
}

static void d3d9_overlay_vertex_geom(void *data,
      float x, float y,
      float w, float h)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_vertex_geom(x, y, w, h);
}

static void d3d9_overlay_enable(void *data, bool state)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_enable(state);
}

static void d3d9_overlay_full_screen(void *data, bool enable)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_full_screen(enable);
}

static void d3d9_overlay_set_alpha(void *data, float mod)
{
   return reinterpret_cast<D3DVideo*>(data)->overlay_set_alpha(mod);
}

static const video_overlay_interface_t d3d9_overlay_interface = {
   d3d9_overlay_enable,
   d3d9_overlay_load,
   d3d9_overlay_tex_geom,
   d3d9_overlay_vertex_geom,
   d3d9_overlay_full_screen,
   d3d9_overlay_set_alpha,
};

static void d3d9_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &d3d9_overlay_interface;
}
#endif

static void d3d9_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         gfx_set_square_pixel_viewport(g_extern.frame_cache.width, g_extern.frame_cache.height);
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
   d3d->info().force_aspect = true;
   d3d->should_resize = true;
   return;
}

static void d3d9_apply_state_changes(void *data)
{
   D3DVideo *d3d = reinterpret_cast<D3DVideo*>(data);
   d3d->should_resize = true;
}

static void d3d9_set_osd_msg(void *data, const char *msg, void *userdata)
{
   font_params_t *params = (font_params_t*)userdata;
   reinterpret_cast<D3DVideo*>(data)->render_msg(msg, params);
}

static void d3d9_show_mouse(void *data, bool state)
{
   show_cursor(state);
}

#ifdef HAVE_RGUI
static void d3d9_set_rgui_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   reinterpret_cast<D3DVideo*>(data)->set_rgui_texture_frame(frame, rgb32, width, height, alpha);
}

static void d3d9_set_rgui_texture_enable(void *data, bool state, bool full_screen)
{
   reinterpret_cast<D3DVideo*>(data)->set_rgui_texture_enable(state, full_screen);
}
#endif

static const video_poke_interface_t d3d9_poke_interface = {
   NULL,
#ifdef HAVE_FBO
   NULL,
   NULL,
#endif
   d3d9_set_aspect_ratio,
   d3d9_apply_state_changes,
#ifdef HAVE_RGUI
   d3d9_set_rgui_texture_frame,
   d3d9_set_rgui_texture_enable,
#endif
   d3d9_set_osd_msg,

   d3d9_show_mouse,
};

static void d3d9_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &d3d9_poke_interface;
}

const video_driver_t video_d3d9 = {
   d3d9_init,
   d3d9_frame,
   d3d9_set_nonblock_state,
   d3d9_alive,
   d3d9_focus,
   d3d9_set_shader,
   d3d9_free,
   "d3d9",
#ifdef HAVE_RGUI
   nullptr,
   nullptr,
   nullptr,
#endif
   d3d9_set_rotation,
   d3d9_viewport_info,
   d3d9_read_viewport,
#ifdef HAVE_OVERLAY
   d3d9_get_overlay_interface,
#endif
   d3d9_get_poke_interface
};
