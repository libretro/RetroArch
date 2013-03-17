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
#include "config_file.hpp"
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

void D3DVideo::set_viewport(unsigned x, unsigned y, unsigned width, unsigned height)
{
   D3DVIEWPORT9 viewport;
   viewport.X = x;
   viewport.Y = y;
   viewport.Width = width;
   viewport.Height = height;
   viewport.MinZ = 0.0f;
   viewport.MaxZ = 1.0f;

   font_rect.left = x + width * g_settings.video.msg_pos_x;
   font_rect.right = x + width;
   font_rect.top = y + (1.0f - g_settings.video.msg_pos_y) * height - g_settings.video.font_size; 
   font_rect.bottom = height;

   font_rect_shifted = font_rect;
   font_rect_shifted.left -= 2;
   font_rect_shifted.right -= 2;
   font_rect_shifted.top += 2;
   font_rect_shifted.bottom += 2;

   final_viewport = viewport;
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
      float device_aspect = static_cast<float>(width) / static_cast<float>(height);
      if (fabs(device_aspect - desired_aspect) < 0.001)
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
   gfx_set_dwm();

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

   gfx_window_title_reset();
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

   show_cursor(!info->fullscreen);
   Callback::quit = false;

   ShowWindow(hWnd, SW_RESTORE);
   UpdateWindow(hWnd);
   SetForegroundWindow(hWnd);
   SetFocus(hWnd);

   // This should only be done once here
   // to avoid set_shader() to be overridden
   // later.
#ifdef HAVE_CG
   auto shader_type = g_settings.video.shader_type;
   if ((shader_type == RARCH_SHADER_CG ||
            shader_type == RARCH_SHADER_AUTO) && *g_settings.video.cg_shader_path)
      cg_shader = g_settings.video.cg_shader_path;
#endif

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

   if (!chain->render(frame, width, height, pitch, rotation))
   {
      RARCH_ERR("[D3D9]: Failed to render scene.\n");
      return false;
   }

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

   RARCH_PERFORMANCE_STOP(d3d_frame);

   if (dev->Present(nullptr, nullptr, nullptr, nullptr) != D3D_OK)
   {
      needs_restore = true;
      return true;
   }

   update_title();
   return true;
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

void D3DVideo::init_chain_singlepass(const video_info_t &video_info)
{
   LinkInfo info = {0};
   LinkInfo info_second = {0};

#ifdef HAVE_CG
   info.shader_path = cg_shader;
#endif

   bool second_pass = g_settings.video.render_to_texture;

   if (second_pass)
   {
      info.scale_x       = g_settings.video.fbo.scale_x;
      info.scale_y       = g_settings.video.fbo.scale_y;
      info.filter_linear = video_info.smooth;
      info.tex_w         = next_pow2(RARCH_SCALE_BASE * video_info.input_scale);
      info.tex_h         = next_pow2(RARCH_SCALE_BASE * video_info.input_scale);
      info.scale_type_x  = info.scale_type_y = LinkInfo::Relative;

      info_second.scale_x       = info_second.scale_y = 1.0f;
      info_second.scale_type_x  = info_second.scale_type_y = LinkInfo::Viewport;
      info_second.filter_linear = g_settings.video.second_pass_smooth;
      info_second.tex_w         = next_pow2(info.tex_w * info.scale_x);
      info_second.tex_h         = next_pow2(info.tex_h * info.scale_y);
      info_second.shader_path   = g_settings.video.second_pass_shader;
   }
   else
   {
      info.scale_x            = info.scale_y = 1.0f;
      info.filter_linear      = video_info.smooth;
      info.tex_w = info.tex_h = RARCH_SCALE_BASE * video_info.input_scale;
      info.scale_type_x       = info.scale_type_y = LinkInfo::Viewport;
   }

   chain = std::unique_ptr<RenderChain>(new RenderChain(
               video_info,
               dev, cgCtx,
               info,
               video_info.rgb32 ? RenderChain::ARGB : RenderChain::RGB565,
               final_viewport));

   if (second_pass)
      chain->add_pass(info_second);
}

static std::vector<std::string> tokenize(const std::string &str)
{
   std::vector<std::string> list;
   char *elem = strdup(str.c_str());

   char *save;
   const char *tex = strtok_r(elem, ";", &save);
   while (tex)
   {
      list.push_back(tex);
      tex = strtok_r(nullptr, ";", &save);
   }
   free(elem);

   return list;
}

void D3DVideo::init_imports(ConfigFile &conf, const std::string &basedir)
{
   std::string imports;
   if (!conf.get("imports", imports))
      return;

   std::vector<std::string> list = tokenize(imports);

   state_tracker_info tracker_info = {0};
   std::vector<state_tracker_uniform_info> uniforms;

   for (auto itr = list.begin(); itr != list.end(); ++itr)
   {
      auto &elem = *itr;

      state_tracker_uniform_info info;
      std::memset(&info, 0, sizeof(info));
      std::string semantic, wram, input_slot, mask, equal;

      state_tracker_type tracker_type;
      state_ram_type ram_type = RARCH_STATE_NONE;

      conf.get(elem + "_semantic", semantic);
      if (semantic == "capture")
         tracker_type = RARCH_STATE_CAPTURE;
      else if (semantic == "transition")
         tracker_type = RARCH_STATE_TRANSITION;
      else if (semantic == "transition_count")
         tracker_type = RARCH_STATE_TRANSITION_COUNT;
      else if (semantic == "capture_previous")
         tracker_type = RARCH_STATE_CAPTURE_PREV;
      else if (semantic == "transition_previous")
         tracker_type = RARCH_STATE_TRANSITION_PREV;
#ifdef HAVE_PYTHON
      else if (semantic == "python")
         tracker_type = RARCH_STATE_PYTHON;
#endif
      else
         throw std::logic_error("Invalid semantic.");

      unsigned addr = 0;
#ifdef HAVE_PYTHON
      if (tracker_type != RARCH_STATE_PYTHON)
#endif
      {
         unsigned input_slot = 0;
         if (conf.get_hex(elem + "_input_slot", input_slot))
         {
            switch (input_slot)
            {
               case 1:
                  ram_type = RARCH_STATE_INPUT_SLOT1;
                  break;

               case 2:
                  ram_type = RARCH_STATE_INPUT_SLOT2;
                  break;

               default:
                  throw std::logic_error("Invalid input slot for import.");
            }
         }
         else if (conf.get_hex(elem + "_wram", addr))
            ram_type = RARCH_STATE_WRAM;
         else
            throw std::logic_error("No address assigned to semantic.");
      }

      unsigned memtype;
      switch (ram_type)
      {
         case RARCH_STATE_WRAM:
            memtype = RETRO_MEMORY_SYSTEM_RAM;
            break;

         default:
            memtype = -1u;
      }

      if ((memtype != -1u) && (addr >= pretro_get_memory_size(memtype)))
         throw std::logic_error("Semantic address out of bounds.");

      unsigned bitmask = 0, bitequal = 0;
      conf.get_hex(elem + "_mask", bitmask);
      conf.get_hex(elem + "_equal", bitequal);

      strlcpy(info.id, elem.c_str(), sizeof(info.id));
      info.addr     = addr;
      info.type     = tracker_type;
      info.ram_type = ram_type;
      info.mask     = bitmask;
      info.equal    = bitequal;

      uniforms.push_back(info);
   }

   tracker_info.wram = (uint8_t*)pretro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   tracker_info.info = uniforms.data();
   tracker_info.info_elem = uniforms.size();

   std::string py_path;
   std::string py_class;
#ifdef HAVE_PYTHON
   conf.get("import_script", py_path);
   conf.get("import_script_class", py_class);
   tracker_info.script_is_file = true;
#endif

   state_tracker_t *state_tracker = state_tracker_init(&tracker_info);
   if (!state_tracker)
      throw std::runtime_error("Failed to initialize state tracker.");

   std::shared_ptr<state_tracker_t> tracker(state_tracker, [](state_tracker_t *tracker) {
            state_tracker_free(tracker);
         });

   chain->add_state_tracker(tracker);
}

void D3DVideo::init_luts(ConfigFile &conf, const std::string &basedir)
{
   std::string textures;
   if (!conf.get("textures", textures))
      return;

   std::vector<std::string> list = tokenize(textures);

   for (unsigned i = 0; i < list.size(); i++)
   {
      const std::string &id = list[i];

      bool smooth = true;
      conf.get(id + "_filter", smooth);

      std::string path;
      if (!conf.get(id, path))
         throw std::runtime_error("Failed to get LUT texture path!");

      chain->add_lut(id, basedir + path, smooth);
   }
}

void D3DVideo::init_chain_multipass(const video_info_t &info)
{
   ConfigFile conf(cg_shader);

   int shaders = 0;
   if (!conf.get("shaders", shaders))
      throw std::runtime_error("Couldn't find \"shaders\" in meta-shader");

   if (shaders < 1)
      throw std::runtime_error("Must have at least one shader!");

   RARCH_LOG("[D3D9 Meta-Cg] Found %d shaders.\n", shaders);

   std::string basedir = cg_shader;
   size_t pos = basedir.rfind('/');
   if (pos == std::string::npos)
      pos = basedir.rfind('\\');

   if (pos != std::string::npos)
      basedir.replace(basedir.begin() + pos + 1, basedir.end(), "");
   else
      basedir = "./";

   bool use_extra_pass = false;
   bool use_first_pass_only = false;

   std::vector<std::string> shader_paths;
   std::vector<LinkInfo::ScaleType> scale_types_x;
   std::vector<LinkInfo::ScaleType> scale_types_y;
   std::vector<float> scales_x;
   std::vector<float> scales_y;
   std::vector<unsigned> abses_x;
   std::vector<unsigned> abses_y;
   std::vector<bool> filters;

   // Shader paths.
   for (int i = 0; i < shaders; i++)
   {
      char buf[256];
      snprintf(buf, sizeof(buf), "shader%d", i);

      std::string relpath;
      if (!conf.get(buf, relpath))
         throw std::runtime_error("Couldn't locate shader path in meta-shader");

      shader_paths.push_back(basedir);
      shader_paths.back() += relpath;
   }

   // Dimensions.
   for (int i = 0; i < shaders; i++)
   {
      char attr_type[64];
      char attr_type_x[64];
      char attr_type_y[64];
      char attr_scale[64];
      char attr_scale_x[64];
      char attr_scale_y[64];
      int abs_x = RARCH_SCALE_BASE * info.input_scale;
      int abs_y = RARCH_SCALE_BASE * info.input_scale;
      double scale_x = 1.0f;
      double scale_y = 1.0f;

      std::string attr   = "source";
      std::string attr_x = "source";
      std::string attr_y = "source";
      snprintf(attr_type,    sizeof(attr_type),    "scale_type%d", i);
      snprintf(attr_type_x,  sizeof(attr_type_x),  "scale_type_x%d", i);
      snprintf(attr_type_y,  sizeof(attr_type_x),  "scale_type_y%d", i);
      snprintf(attr_scale,   sizeof(attr_scale),   "scale%d", i);
      snprintf(attr_scale_x, sizeof(attr_scale_x), "scale_x%d", i);
      snprintf(attr_scale_y, sizeof(attr_scale_y), "scale_y%d", i);

      bool has_scale = false;

      if (conf.get(attr_type, attr))
      {
         attr_x = attr_y = attr;
         has_scale = true;
      }
      else
      {
         if (conf.get(attr_type_x, attr))
            has_scale = true;
         if (conf.get(attr_type_y, attr))
            has_scale = true;
      }

      if (attr_x == "source")
         scale_types_x.push_back(LinkInfo::Relative);
      else if (attr_x == "viewport")
         scale_types_x.push_back(LinkInfo::Viewport);
      else if (attr_x == "absolute")
         scale_types_x.push_back(LinkInfo::Absolute);
      else
         throw std::runtime_error("Invalid scale_type_x!");

      if (attr_y == "source")
         scale_types_y.push_back(LinkInfo::Relative);
      else if (attr_y == "viewport")
         scale_types_y.push_back(LinkInfo::Viewport);
      else if (attr_y == "absolute")
         scale_types_y.push_back(LinkInfo::Absolute);
      else
         throw std::runtime_error("Invalid scale_type_y!");

      double scale = 0.0;
      if (conf.get(attr_scale, scale))
         scale_x = scale_y = scale;
      else
      {
         conf.get(attr_scale_x, scale_x);
         conf.get(attr_scale_y, scale_y);
      }

      int absolute = 0;
      if (conf.get(attr_scale, absolute))
         abs_x = abs_y = absolute;
      else
      {
         conf.get(attr_scale_x, abs_x);
         conf.get(attr_scale_y, abs_y);
      }

      scales_x.push_back(scale_x);
      scales_y.push_back(scale_y);
      abses_x.push_back(abs_x);
      abses_y.push_back(abs_y);

      if (has_scale && i == shaders - 1)
         use_extra_pass = true;
      else if (!has_scale && i == 0)
         use_first_pass_only = true;
      else if (i > 0)
         use_first_pass_only = false;
   }

   // Filter options.
   for (int i = 0; i < shaders; i++)
   {
      char attr_filter[64];
      snprintf(attr_filter, sizeof(attr_filter), "filter_linear%d", i);
      bool filter = info.smooth;
      conf.get(attr_filter, filter);
      filters.push_back(filter);
   }

   // Setup information for first pass.
   LinkInfo link_info = {0};
   link_info.shader_path = shader_paths[0];

   if (use_first_pass_only)
   {
      link_info.scale_x = link_info.scale_y = 1.0f;
      link_info.scale_type_x = link_info.scale_type_y = LinkInfo::Viewport;
   }
   else
   {
      link_info.scale_x = scales_x[0];
      link_info.scale_y = scales_y[0];
      link_info.abs_x = abses_x[0];
      link_info.abs_y = abses_y[0];
      link_info.scale_type_x = scale_types_x[0];
      link_info.scale_type_y = scale_types_y[0];
   }

   link_info.filter_linear = filters[0];
   link_info.tex_w = link_info.tex_h = info.input_scale * RARCH_SCALE_BASE;

   chain = std::unique_ptr<RenderChain>(
         new RenderChain(
            video_info,
            dev, cgCtx,
            link_info,
            info.rgb32 ? RenderChain::ARGB : RenderChain::RGB565,
            final_viewport));

   unsigned current_width = link_info.tex_w;
   unsigned current_height = link_info.tex_h;
   unsigned out_width = 0;
   unsigned out_height = 0;

   for (int i = 1; i < shaders; i++)
   {
      RenderChain::convert_geometry(link_info,
            out_width, out_height,
            current_width, current_height, final_viewport);

      link_info.scale_x = scales_x[i];
      link_info.scale_y = scales_y[i];
      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);
      link_info.scale_type_x = scale_types_x[i];
      link_info.scale_type_y = scale_types_y[i];
      link_info.filter_linear = filters[i];
      link_info.shader_path = shader_paths[i];

      current_width = out_width;
      current_height = out_height;

      if (i == shaders - 1 && !use_extra_pass)
      {
         link_info.scale_x = link_info.scale_y = 1.0f;
         link_info.scale_type_x = link_info.scale_type_y = LinkInfo::Viewport;
      }

      chain->add_pass(link_info);
   }

   if (use_extra_pass)
   {
      RenderChain::convert_geometry(link_info,
            out_width, out_height,
            current_width, current_height, final_viewport);

      link_info.scale_x = link_info.scale_y = 1.0f;
      link_info.scale_type_x = link_info.scale_type_y = LinkInfo::Viewport;
      link_info.filter_linear = info.smooth;
      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);
      link_info.shader_path = "";
      chain->add_pass(link_info);
   }

   init_luts(conf, basedir);
   init_imports(conf, basedir);
}

bool D3DVideo::set_shader(const std::string &path)
{
   auto old_shader = cg_shader;
   bool restore_old = false;
   try
   {
      cg_shader = path;
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
      restore();
   }

   return !restore_old;
}

bool D3DVideo::init_chain(const video_info_t &video_info)
{
   try
   {
      if (cg_shader.find(".cgp") != std::string::npos)
         init_chain_multipass(video_info);
      else
         init_chain_singlepass(video_info);
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

static bool d3d9_set_shader(void *data, enum rarch_shader_type type, const char *path, unsigned index)
{
   // TODO: Add support for directly setting this param.
   if (index != RARCH_SHADER_INDEX_MULTIPASS)
      return false;

#ifdef HAVE_CG
   if (type != RARCH_SHADER_CG)
   {
      RARCH_ERR("[D3D9]: Only Cg shaders supported.\n");
      return false;
   }
#endif

   return reinterpret_cast<D3DVideo*>(data)->set_shader(path);
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
   d3d9_set_rotation,
   d3d9_viewport_info,
   d3d9_read_viewport,
};

