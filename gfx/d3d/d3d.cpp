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

#include <compat/posix_string.h>
#include "../../performance.h"

#if defined(HAVE_CG)
#define HAVE_SHADERS
#endif

#ifdef HAVE_HLSL
#include "../../gfx/shader/shader_hlsl.h"
#endif

#include "d3d_defines.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)

#ifdef HAVE_HLSL
#include "../shader/shader_hlsl.h"
#endif

#endif

/* forward declarations */
static void d3d_calculate_rect(d3d_video_t *d3d,
      unsigned width, unsigned height,
   bool keep, float desired_aspect);
static bool d3d_init_luts(d3d_video_t *d3d);
static void d3d_set_font_rect(d3d_video_t *d3d,
      const struct font_params *params);
static bool d3d_process_shader(d3d_video_t *d3d);
static bool d3d_init_multipass(d3d_video_t *d3d);
static void d3d_deinit_chain(d3d_video_t *d3d);
static bool d3d_init_chain(d3d_video_t *d3d,
      const video_info_t *video_info);

#ifdef HAVE_OVERLAY
static void d3d_free_overlays(void *data);
static void d3d_free_overlay(void *data, overlay_t *overlay);
#endif

#ifdef _XBOX
static void d3d_reinit_renderchain(void *data,
      const video_info_t *video);
static void renderchain_free(void *data);
#endif

void d3d_make_d3dpp(void *data, const video_info_t *info,
	D3DPRESENT_PARAMETERS *d3dpp);

#ifdef HAVE_WINDOW
#define IDI_ICON 1
#define MAX_MONITORS 9

extern LRESULT CALLBACK WindowProc(HWND hWnd, UINT message,
        WPARAM wParam, LPARAM lParam);
static RECT d3d_monitor_rect(d3d_video_t *d3d);
#endif

#ifdef HAVE_MONITOR
namespace Monitor
{
	static HMONITOR last_hm;
	static HMONITOR all_hms[MAX_MONITORS];
	static unsigned num_mons;
}
#endif

static void d3d_deinit_shader(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   (void)d3d;
   (void)data;

#ifdef HAVE_CG
   if (!d3d->cgCtx)
      return;

   cgD3D9UnloadAllPrograms();
   cgD3D9SetDevice(NULL);
   cgDestroyContext(d3d->cgCtx);
   d3d->cgCtx = NULL;
#endif
}

static bool d3d_init_shader(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   (void)d3d;
	(void)data;
#if defined(HAVE_HLSL)
   RARCH_LOG("D3D]: Using HLSL shader backend.\n");
   const shader_backend_t *backend = &hlsl_backend;
   const char *shader_path = g_settings.video.shader_path;
   d3d->shader = backend;
   if (!d3d->shader)
      return false;

   return d3d->shader->init(d3d, shader_path);
#elif defined(HAVE_CG)
   d3d->cgCtx = cgCreateContext();
   if (!d3d->cgCtx)
      return false;

   RARCH_LOG("[D3D]: Created shader context.\n");

   HRESULT ret = cgD3D9SetDevice(d3d->dev);
   if (FAILED(ret))
      return false;
   return true;
#elif defined(_XBOX1)
	return false;
#endif
}

static void d3d_deinit_chain(d3d_video_t *d3d)
{
#ifdef _XBOX
   renderchain_free(d3d);
#else
   if (d3d->chain)
      delete (renderchain_t *)d3d->chain;
   d3d->chain = NULL;
#endif
}

static void d3d_deinitialize(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d->font_ctx && d3d->font_ctx->deinit)
      d3d->font_ctx->deinit(d3d);
   d3d->font_ctx = NULL;
   d3d_deinit_chain(d3d);
#ifdef HAVE_SHADERS
   d3d_deinit_shader(d3d);
#endif

#ifndef _XBOX
   d3d->needs_restore = false;
#endif
}

static bool d3d_init_base(void *data, const video_info_t *info)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   D3DPRESENT_PARAMETERS d3dpp;
   d3d_make_d3dpp(d3d, info, &d3dpp);

   d3d->g_pD3D = D3DCREATE_CTX(D3D_SDK_VERSION);
   if (!d3d->g_pD3D)
   {
      RARCH_ERR("Failed to create D3D interface.\n");
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

static bool d3d_initialize(void *data, const video_info_t *info)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   bool ret = true;
   if (!d3d->g_pD3D)
      ret = d3d_init_base(d3d, info);
   else if (d3d->needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;
      d3d_make_d3dpp(d3d, info, &d3dpp);
      if (d3d->dev->Reset(&d3dpp) != D3D_OK)
      {
         // Try to recreate the device completely ..
#ifndef _XBOX
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
         RARCH_WARN(
               "[D3D]: Attempting to recover from dead state (%s).\n", err);
#else
         RARCH_WARN("[D3D]: Attempting to recover from dead state.\n");
#endif
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

   d3d_calculate_rect(d3d, d3d->screen_width, d3d->screen_height,
         info->force_aspect, g_extern.system.aspect_ratio);

#ifdef HAVE_SHADERS
   if (!d3d_init_shader(d3d))
   {
      RARCH_ERR("Failed to initialize shader subsystem.\n");
      return false;
   }
#endif

   if (!d3d_init_chain(d3d, info))
   {
      RARCH_ERR("Failed to initialize render chain.\n");
      return false;
   }

#if defined(_XBOX360)
   strlcpy(g_settings.video.font_path, "game:\\media\\Arial_12.xpr",
         sizeof(g_settings.video.font_path));
#endif
   d3d->font_ctx = d3d_font_init_first(d3d, g_settings.video.font_path, 0);
   if (!d3d->font_ctx)
   {
      RARCH_ERR("Failed to initialize font.\n");
      return false;
   }

   return true;
}

static void d3d_set_viewport(d3d_video_t *d3d, int x, int y,
      unsigned width, unsigned height)
{
   D3DVIEWPORT viewport;

   /* D3D doesn't support negative X/Y viewports ... */
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

#ifndef _XBOX
   d3d_set_font_rect(d3d, NULL);
#endif
}
bool d3d_restore(d3d_video_t *d3d)
{
   d3d_deinitialize(d3d);
   d3d->needs_restore = !d3d_initialize(d3d, &d3d->video_info);

   if (d3d->needs_restore)
      RARCH_ERR("[D3D]: Restore error.\n");

   return !d3d->needs_restore;
}

static void d3d_calculate_rect(d3d_video_t *d3d,
      unsigned width, unsigned height,
   bool keep, float desired_aspect)
{
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
         const rarch_viewport_t &custom = 
            g_extern.console.screen.viewports.custom_vp;
         d3d_set_viewport(d3d, custom.x, custom.y, 
               custom.width, custom.height);
      }
      else
      {
         float device_aspect = static_cast<float>(width) / 
            static_cast<float>(height);

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
            d3d_set_viewport(d3d, 0, 0, width, height);
         else if (device_aspect > desired_aspect)
         {
            float delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            d3d_set_viewport(d3d, int(roundf(width * (0.5f - delta))),
                  0, unsigned(roundf(2.0f * width * delta)), height);
         }
         else
         {
            float delta = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            d3d_set_viewport(d3d, 0, int(roundf(height * (0.5f - delta))),
                  width, unsigned(roundf(2.0f * height * delta)));
         }
      }
   }
}

static void d3d_set_nonblock_state(void *data, bool state)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->video_info.vsync = !state;

   if (d3d->ctx_driver && d3d->ctx_driver->swap_interval)
      d3d->ctx_driver->swap_interval(d3d, state ? 0 : 1);
}

static bool d3d_alive(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   bool quit, resize;

   quit = false;
   resize = false;

   if (d3d->ctx_driver && d3d->ctx_driver->check_window)
      d3d->ctx_driver->check_window(d3d, &quit, &resize,
            &d3d->screen_width, &d3d->screen_height, g_extern.frame_count);

   if (quit)
      d3d->quitting = quit;
   else if (resize)
      d3d->should_resize = true;

   return !quit;
}

static bool d3d_focus(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   if (d3d && d3d->ctx_driver && d3d->ctx_driver->has_focus)
      return d3d->ctx_driver->has_focus(d3d);
   return false;
}

static bool d3d_has_windowed(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   if (d3d && d3d->ctx_driver && d3d->ctx_driver->has_windowed)
      return d3d->ctx_driver->has_windowed(d3d);
   return true;
}

static void d3d_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         gfx_set_square_pixel_viewport(
               g_extern.system.av_info.geometry.base_width,
               g_extern.system.av_info.geometry.base_height);
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
}

static void d3d_apply_state_changes(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->should_resize = true;
}

static void d3d_set_osd_msg(void *data, const char *msg,
      const struct font_params *params)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

#ifndef _XBOX
   if (params)
      d3d_set_font_rect(d3d, params);
#endif

   if (d3d && d3d->font_ctx && d3d->font_ctx->render_msg)
      d3d->font_ctx->render_msg(d3d, msg, params);
}

/* Delay constructor due to lack of exceptions. */

static bool d3d_construct(d3d_video_t *d3d,
      const video_info_t *info, const input_driver_t **input,
      void **input_data)
{
   d3d->should_resize = false;
#ifndef _XBOX
   gfx_set_dwm();
#endif

#if defined(HAVE_MENU) && defined(HAVE_OVERLAY)
   if (d3d->menu)
      free(d3d->menu);

   d3d->menu = (overlay_t*)calloc(1, sizeof(overlay_t));

   if (!d3d->menu)
      return false;

   d3d->menu->tex_coords.x = 0;
   d3d->menu->tex_coords.y = 0;
   d3d->menu->tex_coords.w = 1;
   d3d->menu->tex_coords.h = 1;
   d3d->menu->vert_coords.x = 0;
   d3d->menu->vert_coords.y = 1;
   d3d->menu->vert_coords.w = 1;
   d3d->menu->vert_coords.h = -1;
#endif

#if defined(HAVE_WINDOW) && !defined(_XBOX)
   memset(&d3d->windowClass, 0, sizeof(d3d->windowClass));
   d3d->windowClass.cbSize        = sizeof(d3d->windowClass);
   d3d->windowClass.style         = CS_HREDRAW | CS_VREDRAW;
   d3d->windowClass.lpfnWndProc   = WindowProc;
   d3d->windowClass.hInstance     = NULL;
   d3d->windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
   d3d->windowClass.lpszClassName = "RetroArch";
   d3d->windowClass.hIcon = LoadIcon(GetModuleHandle(NULL),
         MAKEINTRESOURCE(IDI_ICON));
   d3d->windowClass.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL),
         MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
   if (!info->fullscreen)
      d3d->windowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;

   RegisterClassEx(&d3d->windowClass);
#endif

   unsigned full_x, full_y;
#ifdef HAVE_MONITOR
   RECT mon_rect = d3d_monitor_rect(d3d);

   bool windowed_full = g_settings.video.windowed_fullscreen;

   full_x = (windowed_full || info->width  == 0) ? 
      (mon_rect.right  - mon_rect.left) : info->width;
   full_y = (windowed_full || info->height == 0) ? 
      (mon_rect.bottom - mon_rect.top)  : info->height;
   RARCH_LOG("[D3D]: Monitor size: %dx%d.\n", 
         (int)(mon_rect.right  - mon_rect.left),
         (int)(mon_rect.bottom - mon_rect.top));
#else
   if (d3d->ctx_driver && d3d->ctx_driver->get_video_size)
      d3d->ctx_driver->get_video_size(d3d, &full_x, &full_y);
#endif
   d3d->screen_width  = info->fullscreen ? full_x : info->width;
   d3d->screen_height = info->fullscreen ? full_y : info->height;

#if defined(HAVE_WINDOW) && !defined(_XBOX)
   unsigned win_width  = d3d->screen_width;
   unsigned win_height = d3d->screen_height;

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

   driver.display_type  = RARCH_DISPLAY_WIN32;
   driver.video_display = 0;
   driver.video_window  = (uintptr_t)d3d->hWnd;
#endif

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->show_mouse)
      d3d->ctx_driver->show_mouse(d3d, !info->fullscreen
#ifdef HAVE_OVERLAY
      || d3d->overlays_enabled
#endif
   );

#if defined(HAVE_WINDOW) && !defined(_XBOX)
   ShowWindow(d3d->hWnd, SW_RESTORE);
   UpdateWindow(d3d->hWnd);
   SetForegroundWindow(d3d->hWnd);
   SetFocus(d3d->hWnd);
#endif

#ifndef _XBOX
#ifdef HAVE_SHADERS
   /* This should only be done once here
    * to avoid set_shader() to be overridden
    * later. */
   enum rarch_shader_type type = 
      gfx_shader_parse_type(g_settings.video.shader_path, RARCH_SHADER_NONE);
   if (g_settings.video.shader_enable && type == RARCH_SHADER_CG)
      d3d->cg_shader = g_settings.video.shader_path;

   if (!d3d_process_shader(d3d))
      return false;
#endif
#endif

   d3d->video_info = *info;
   if (!d3d_initialize(d3d, &d3d->video_info))
      return false;

   if (input && input_data &&
      d3d->ctx_driver && d3d->ctx_driver->input_driver)
      d3d->ctx_driver->input_driver(d3d, input, input_data);

   RARCH_LOG("[D3D]: Init complete.\n");
   return true;
}

static void d3d_viewport_info(void *data, struct rarch_viewport *vp)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   vp->x           = d3d->final_viewport.X;
   vp->y           = d3d->final_viewport.Y;
   vp->width       = d3d->final_viewport.Width;
   vp->height      = d3d->final_viewport.Height;

   vp->full_width  = d3d->screen_width;
   vp->full_height = d3d->screen_height;
}

static void d3d_set_rotation(void *data, unsigned rot)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->dev_rotation = rot;
}

static void d3d_show_mouse(void *data, bool state)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->show_mouse)
      d3d->ctx_driver->show_mouse(d3d, state);
}

static const gfx_ctx_driver_t *d3d_get_context(void *data)
{
   /* Default to Direct3D9 for now.
   TODO: GL core contexts through ANGLE? */
   enum gfx_ctx_api api = GFX_CTX_DIRECT3D9_API;
   unsigned major = 9, minor = 0;
   (void)major;
   (void)minor;

#if defined(_XBOX1)
   api = GFX_CTX_DIRECT3D8_API;
   major = 8;
#endif
   return gfx_ctx_init_first(driver.video_data, api,
         major, minor, false);
}

static void *d3d_init(const video_info_t *info,
      const input_driver_t **input, void **input_data)
{
#ifdef _XBOX
   if (driver.video_data)
   {
      d3d_video_t *vid = (d3d_video_t*)driver.video_data;

      /* Reinitialize renderchain as we 
       * might have changed pixel formats.*/
      d3d_reinit_renderchain(vid, info);

      if (input && input_data)
      {
         *input = driver.input;
         *input_data = driver.input_data;
      }

      driver.video_data_own = true;
      driver.input_data_own = true;
      return driver.video_data;
   }
#endif

   d3d_video_t *vid = new d3d_video_t();
   if (!vid)
      return NULL;

   vid->ctx_driver = d3d_get_context(vid);
   if (!vid->ctx_driver)
   {
      delete vid;
      return NULL;
   }

   /* Default values */
   vid->g_pD3D               = NULL;
   vid->dev                  = NULL;
   vid->dev_rotation         = 0;
   vid->needs_restore        = false;
#ifdef HAVE_CG
   vid->cgCtx                = NULL;
#endif
#ifdef HAVE_OVERLAY
   vid->overlays_enabled     = false;
#endif
#ifdef _XBOX
   vid->should_resize        = false;
#else
   vid->menu                 = NULL;
#endif

   if (!d3d_construct(vid, info, input, input_data))
   {
      RARCH_ERR("[D3D]: Failed to init D3D.\n");
      delete vid;
      return NULL;
   }

#ifdef _XBOX
   driver.video_data_own = true;
   driver.input_data_own = true;
#endif

   return vid;
}

static void d3d_free(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (!d3d)
      return;

   d3d_deinitialize(d3d);
#ifdef HAVE_OVERLAY
   d3d_free_overlays(d3d);
#endif
#if defined(HAVE_MENU) && !defined(_XBOX)
   d3d_free_overlay(d3d, d3d->menu);
#endif
#ifdef _XBOX
   if (d3d->ctx_driver && d3d->ctx_driver->destroy)
      d3d->ctx_driver->destroy(d3d);
   d3d->ctx_driver = NULL;
#endif
   if (d3d->dev)
      d3d->dev->Release();
   if (d3d->g_pD3D)
      d3d->g_pD3D->Release();

#ifdef HAVE_MONITOR
   Monitor::last_hm = MonitorFromWindow(d3d->hWnd,
         MONITOR_DEFAULTTONEAREST);
   DestroyWindow(d3d->hWnd);
#endif

   if (d3d)
      delete d3d;

#ifndef _XBOX
   UnregisterClass("RetroArch", GetModuleHandle(NULL));
#endif
}


#ifdef _XBOX
#include "../../xdk/xdk_resources.h"
#include "render_chain_xdk.h"
#endif

#ifdef HAVE_MONITOR
static BOOL CALLBACK d3d_monitor_enum_proc(HMONITOR hMonitor,
      HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
   Monitor::all_hms[Monitor::num_mons++] = hMonitor;
   return TRUE;
}

// Multi-monitor support.
static RECT d3d_monitor_rect(d3d_video_t *d3d)
{
   Monitor::num_mons = 0;
   EnumDisplayMonitors(NULL, NULL, d3d_monitor_enum_proc, 0);

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
#ifndef _XBOX
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

   /* load the existing vertices */
   void *verts = d3d_vertex_buffer_lock(out_img->vertex_buf);

   if (!verts)
      return false;

   /* copy the new verts over the old verts */
   memcpy(verts, newVerts, sizeof(newVerts));
   d3d_vertex_buffer_unlock(out_img->vertex_buf);

   d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
   d3d->dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d->dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

   /* Also blend the texture with the set alpha value. */
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);

   /* Draw the quad. */
   d3d_set_texture(d3dr, 0, out_img->pixels);
   d3d_set_stream_source(d3dr, 0,
         out_img->vertex_buf, 0, sizeof(Vertex));
   d3d_set_vertex_shader(d3dr, D3DFVF_CUSTOMVERTEX, NULL);

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
      gfx_shader_pass *dummy_pass = (gfx_shader_pass*)
         &d3d->shader.pass[d3d->shader.passes - 1];

      dummy_pass->fbo.scale_x = dummy_pass->fbo.scale_y = 1.0f;
      dummy_pass->fbo.type_x  = dummy_pass->fbo.type_y = RARCH_SCALE_VIEWPORT;
      dummy_pass->filter      = RARCH_FILTER_UNSPEC;
   }
   else
   {
      gfx_shader_pass *pass = (gfx_shader_pass*)
         &d3d->shader.pass[d3d->shader.passes - 1];

      pass->fbo.scale_x = pass->fbo.scale_y = 1.0f;
      pass->fbo.type_x  = pass->fbo.type_y = RARCH_SCALE_VIEWPORT;
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
   gfx_shader_pass *pass = (gfx_shader_pass*)&d3d->shader.pass[0];

   pass->fbo.valid = true;
   pass->fbo.scale_x = pass->fbo.scale_y = 1.0;
   pass->fbo.type_x = pass->fbo.type_y = RARCH_SCALE_VIEWPORT;
   strlcpy(pass->source.path, d3d->cg_shader.c_str(),
         sizeof(pass->source.path));
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
static void d3d_overlay_render(void *data, overlay_t *overlay)
{
   void *verts;
   unsigned i;
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (!overlay || !overlay->tex)
      return;

   struct overlay_vertex
   {
      float x, y, z;
      float u, v;
      float r, g, b, a;
   } vert[4];

   if (!overlay->vert_buf)
   {
      overlay->vert_buf = (LPDIRECT3DVERTEXBUFFER)d3d_vertex_buffer_new(
      d3d->dev, sizeof(vert), d3d->dev->GetSoftwareVertexProcessing() ? 
      D3DUSAGE_SOFTWAREPROCESSING : 0, 0, D3DPOOL_MANAGED, NULL);

	  if (!overlay->vert_buf)
		  return;
   }

   for (i = 0; i < 4; i++)
   {
      vert[i].z = 0.5f;
      vert[i].r = vert[i].g = vert[i].b = 1.0f;
      vert[i].a = overlay->alpha_mod;
   }

   float overlay_width = d3d->final_viewport.Width;
   float overlay_height = d3d->final_viewport.Height;

   vert[0].x = overlay->vert_coords.x * overlay_width;
   vert[1].x = (overlay->vert_coords.x + overlay->vert_coords.w)
      * overlay_width;
   vert[2].x = overlay->vert_coords.x * overlay_width;
   vert[3].x = (overlay->vert_coords.x + overlay->vert_coords.w)
      * overlay_width;
   vert[0].y = overlay->vert_coords.y * overlay_height;
   vert[1].y = overlay->vert_coords.y * overlay_height;
   vert[2].y = (overlay->vert_coords.y + overlay->vert_coords.h)
      * overlay_height;
   vert[3].y = (overlay->vert_coords.y + overlay->vert_coords.h)
      * overlay_height;

   vert[0].u = overlay->tex_coords.x;
   vert[1].u = overlay->tex_coords.x + overlay->tex_coords.w;
   vert[2].u = overlay->tex_coords.x;
   vert[3].u = overlay->tex_coords.x + overlay->tex_coords.w;
   vert[0].v = overlay->tex_coords.y;
   vert[1].v = overlay->tex_coords.y;
   vert[2].v = overlay->tex_coords.y + overlay->tex_coords.h;
   vert[3].v = overlay->tex_coords.y + overlay->tex_coords.h;

   /* Align texels and vertices. */
   for (i = 0; i < 4; i++)
   {
      vert[i].x -= 0.5f;
      vert[i].y += 0.5f;
   }

   overlay->vert_buf->Lock(0, sizeof(vert), &verts, 0);
   memcpy(verts, vert, sizeof(vert));
   d3d_vertex_buffer_unlock(overlay->vert_buf);

   // enable alpha
   d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
   d3d->dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   d3d->dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

#ifndef _XBOX1
   // set vertex decl for overlay
   D3DVERTEXELEMENT vElems[4] = {
      {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_POSITION, 0},
      {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_TEXCOORD, 0},
      {0, 20, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_COLOR, 0},
      D3DDECL_END()
   };
   LPDIRECT3DVERTEXDECLARATION vertex_decl;
   d3d->dev->CreateVertexDeclaration(vElems, &vertex_decl);
   d3d->dev->SetVertexDeclaration(vertex_decl);
   vertex_decl->Release();
#endif

   d3d_set_stream_source(d3d->dev, 0, overlay->vert_buf,
         0, sizeof(overlay_vertex));

   if (overlay->fullscreen)
   {
      /* Set viewport to full window. */
      D3DVIEWPORT vp_full;
      vp_full.X = 0;
      vp_full.Y = 0;
      vp_full.Width = d3d->screen_width;
      vp_full.Height = d3d->screen_height;
      vp_full.MinZ = 0.0f;
      vp_full.MaxZ = 1.0f;
      d3d_set_viewport(d3d->dev, &vp_full);
   }

   /* Render overlay. */
   d3d_set_texture(d3d->dev, 0, overlay->tex);
   d3d_set_sampler_address_u(d3d->dev, 0, D3DTADDRESS_BORDER);
   d3d_set_sampler_address_v(d3d->dev, 0, D3DTADDRESS_BORDER);
   d3d_set_sampler_minfilter(d3d->dev, 0, D3DTEXF_LINEAR);
   d3d_set_sampler_magfilter(d3d->dev, 0, D3DTEXF_LINEAR);
   d3d_draw_primitive(d3d->dev, D3DPT_TRIANGLESTRIP, 0, 2);

   /* Restore previous state. */
   d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
   d3d_set_viewport(d3d->dev, &d3d->final_viewport);
}

static void d3d_free_overlay(void *data, overlay_t *overlay)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (!d3d)
      return;

   d3d_texture_free(overlay->tex);
   d3d_vertex_buffer_free(overlay->vert_buf);
}

static void d3d_free_overlays(void *data)
{
   unsigned i;
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays.size(); i++)
      d3d_free_overlay(d3d, &d3d->overlays[i]);
   d3d->overlays.clear();
}

static void d3d_overlay_tex_geom(void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

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
   d3d_video_t *d3d = (d3d_video_t*)data;

   y = 1.0f - y;
   h = -h;
   d3d->overlays[index].vert_coords.x = x;
   d3d->overlays[index].vert_coords.y = y;
   d3d->overlays[index].vert_coords.w = w;
   d3d->overlays[index].vert_coords.h = h;
}

static bool d3d_overlay_load(void *data,
      const texture_image *images, unsigned num_images)
{
   unsigned i, y;
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d_free_overlays(data);
   d3d->overlays.resize(num_images);

   for (i = 0; i < num_images; i++)
   {
      unsigned width = images[i].width;
      unsigned height = images[i].height;
      overlay_t *overlay = (overlay_t*)&d3d->overlays[i];

      overlay->tex = (LPDIRECT3DTEXTURE)
         d3d_texture_new(d3d->dev, NULL,
                  width, height, 1,
                  0,
                  D3DFMT_A8R8G8B8,
                  D3DPOOL_MANAGED, 0, 0, 0, 
                  NULL, NULL);

      if (!overlay->tex)
      {
         RARCH_ERR("[D3D]: Failed to create overlay texture\n");
         return false;
      }

      D3DLOCKED_RECT d3dlr;
      if (SUCCEEDED(overlay->tex->LockRect(0, &d3dlr,
                  NULL, D3DLOCK_NOSYSLOCK)))
      {
         uint32_t *dst = static_cast<uint32_t*>(d3dlr.pBits);
         const uint32_t *src = images[i].pixels;
         unsigned pitch = d3dlr.Pitch >> 2;
         for (y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
         overlay->tex->UnlockRect(0);
      }

      overlay->tex_w = width;
      overlay->tex_h = height;

      /* Default. Stretch to whole screen. */
      d3d_overlay_tex_geom(d3d, i, 0, 0, 1, 1);

      d3d_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d_overlay_enable(void *data, bool state)
{
   unsigned i;
   d3d_video_t *d3d = (d3d_video_t*)data;

   for (i = 0; i < d3d->overlays.size(); i++)
      d3d->overlays_enabled = state;

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->show_mouse)
      d3d->ctx_driver->show_mouse(d3d, state);
}

static void d3d_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d_video_t *d3d = (d3d_video_t*)data;

   for (i = 0; i < d3d->overlays.size(); i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
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

static void d3d_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &d3d_overlay_interface;
}
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
   d3d_clear(d3dr, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);

   /* Insert black frame first, so we 
    * can screenshot, etc. */
   if (g_settings.video.black_frame_insertion)
   {
      d3d_swap(d3d, d3dr);
      if (d3d->needs_restore)
         return true;
      d3d_clear(d3dr, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);
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
      float msg_width  = d3d->resolution_hd_enable ? 160 : 100;
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
   if (g_extern.is_menu 
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
      if (d3d->menu)
	     d3d_texture_free(d3d->menu->tex); 

      d3d->menu->tex = (LPDIRECT3DTEXTURE)
         d3d_texture_new(d3d->dev, NULL,
            width, height, 1,
            0, D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL);

      if (!d3d->menu->tex)
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
   d3d_has_windowed,
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
