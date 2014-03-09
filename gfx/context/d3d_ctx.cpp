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
#include "../../xdk/xdk_d3d.h"
#else
#include "../d3d9/d3d9.hpp"
#include "win32_common.h"
#endif

#include "../gfx_common.h"

#ifdef _MSC_VER
#ifndef _XBOX
#pragma comment( lib, "d3d9" )
#pragma comment( lib, "d3dx9" )
#pragma comment( lib, "cgd3d9" )
#pragma comment( lib, "dxguid" )
#endif
#endif

#if defined(_XBOX1)
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#define PresentationInterval FullScreen_PresentationInterval
#elif defined(_XBOX360)
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTINTERVAL
#endif

static d3d_video_t *curD3D = NULL;
static bool d3d_quit = false;
static void *dinput;

extern bool d3d_restore(void *data);

static void d3d_resize(unsigned new_width, unsigned new_height)
{
#ifndef _XBOX
   d3d_video_t *d3d = (d3d_video_t*)curD3D;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;
   if (!d3dr)
      return;

   RARCH_LOG("[D3D]: Resize %ux%u.\n", new_width, new_height);

   if (new_width != d3d->video_info.width || new_height != d3d->video_info.height)
   {
      d3d->video_info.width = d3d->screen_width = new_width;
      d3d->video_info.height = d3d->screen_height = new_height;
      d3d_restore(d3d);
   }
#endif
}

#ifdef HAVE_WINDOW
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message,
        WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
			LPCREATESTRUCT p_cs;
			p_cs = (LPCREATESTRUCT)lParam;
			curD3D = (d3d_video_t*)p_cs->lpCreateParams;
			break;

        case WM_CHAR:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
			return win32_handle_keyboard_event(hWnd, message, wParam, lParam);

        case WM_DESTROY:
			d3d_quit = true;
			return 0;
        case WM_SIZE:
			unsigned new_width, new_height;
			new_width = LOWORD(lParam);
			new_height = HIWORD(lParam);

			if (new_width && new_height)
				d3d_resize(new_width, new_height);
			return 0;
    }
    if (dinput_handle_message(dinput, message, wParam, lParam))
        return 0;
    return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif

static void gfx_ctx_d3d_swap_buffers(void)
{
   d3d_video_t *d3d = (d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;

#ifdef _XBOX
   RD3DDevice_Present(d3dr);
#else
   if (d3dr->Present(NULL, NULL, NULL, NULL) != D3D_OK)
   {
      d3d->needs_restore = true;
      RARCH_ERR("[D3D]: Present() failed.\n");
   }
#endif
}

static void gfx_ctx_d3d_update_title(void)
{
   d3d_video_t *d3d = (d3d_video_t*)driver.video_data;
   char buffer[128], buffer_fps[128];
   bool fps_draw = g_settings.fps_show;

   if (gfx_get_fps(buffer, sizeof(buffer), fps_draw ? buffer_fps : NULL, sizeof(buffer_fps)))
   {
#ifndef _XBOX
      std::string title = buffer;
      title += " || Direct3D";
      SetWindowText(d3d->hWnd, title.c_str());
#endif
   }

   if (fps_draw)
   {
#ifdef _XBOX
      char mem[128];
      MEMORYSTATUS stat;
      GlobalMemoryStatus(&stat);
      snprintf(mem, sizeof(mem), "|| MEM: %.2f/%.2fMB", stat.dwAvailPhys/(1024.0f*1024.0f), stat.dwTotalPhys/(1024.0f*1024.0f));
      strlcat(buffer_fps, mem, sizeof(buffer_fps));
#endif
      msg_queue_push(g_extern.msg_queue, buffer_fps, 1, 1);
   }

   g_extern.frame_count++;
}

static void gfx_ctx_d3d_show_mouse(bool state)
{
#ifdef HAVE_WINDOW
   if (state)
      while (ShowCursor(TRUE) < 0);
   else
      while (ShowCursor(FALSE) >= 0);
#endif
}

void d3d_make_d3dpp(void *data, const video_info_t *info, D3DPRESENT_PARAMETERS *d3dpp)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   memset(d3dpp, 0, sizeof(*d3dpp));

#ifdef _XBOX
   d3dpp->Windowed = false;
#else
   d3dpp->Windowed = g_settings.video.windowed_fullscreen || !info->fullscreen;
#endif

   if (info->vsync)
   {
      switch (g_settings.video.swap_interval)
      {
         default:
         case 1: d3dpp->PresentationInterval = D3DPRESENT_INTERVAL_ONE; break;
         case 2: d3dpp->PresentationInterval = D3DPRESENT_INTERVAL_TWO; break;
         case 3: d3dpp->PresentationInterval = D3DPRESENT_INTERVAL_THREE; break;
         case 4: d3dpp->PresentationInterval = D3DPRESENT_INTERVAL_FOUR; break;
      }
   }
   else
      d3dpp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

   d3dpp->SwapEffect = D3DSWAPEFFECT_DISCARD;
   d3dpp->BackBufferCount = 2;
#ifdef _XBOX
   d3dpp->BackBufferFormat = 
#ifdef _XBOX360
      g_extern.console.screen.gamma_correction ? (D3DFORMAT)MAKESRGBFMT(info->rgb32 ? D3DFMT_X8R8G8B8 : D3DFMT_LIN_R5G6B5) :
#endif
      info->rgb32 ? D3DFMT_X8R8G8B8 : D3DFMT_LIN_R5G6B5;
#else
   d3dpp->hDeviceWindow = d3d->hWnd;
   d3dpp->BackBufferFormat = !d3dpp->Windowed ? D3DFMT_X8R8G8B8 : D3DFMT_UNKNOWN;
#endif

   if (!d3dpp->Windowed)
   {
#ifdef _XBOX
      unsigned width, height;
      width = 0;
      height = 0;

      if (d3d->ctx_driver && d3d->ctx_driver->get_video_size)
         d3d->ctx_driver->get_video_size(&width, &height);

      d3dpp->BackBufferWidth  = d3d->screen_width = width;
      d3dpp->BackBufferHeight = d3d->screen_height = height;
#else
      d3dpp->BackBufferWidth = d3d->screen_width;
      d3dpp->BackBufferHeight = d3d->screen_height;
#endif
   }

#ifdef _XBOX
   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;
#if defined(_XBOX1)

   // Get the "video mode"
   DWORD video_mode = XGetVideoFlags();

   // Check if we are able to use progressive mode
   if (video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
   else
      d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;

   // Only valid in PAL mode, not valid for HDTV modes!
   if (XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      if (video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
         d3dpp->FullScreen_RefreshRateInHz = 60;
      else
         d3dpp->FullScreen_RefreshRateInHz = 50;
   }

   if (XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if (video_mode & XC_VIDEO_FLAGS_HDTV_480p)
         d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
      else if (video_mode & XC_VIDEO_FLAGS_HDTV_720p)
         d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
      else if (video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
         d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;
   }

   if (g_extern.lifecycle_state & MODE_MENU_WIDESCREEN)
      d3dpp->Flags |= D3DPRESENTFLAG_WIDESCREEN;
#elif defined(_XBOX360)
   if (!(g_extern.lifecycle_state & (1ULL << MODE_MENU_WIDESCREEN)))
      d3dpp->Flags |= D3DPRESENTFLAG_NO_LETTERBOX;

   if (g_extern.console.screen.gamma_correction)
      d3dpp->FrontBufferFormat       = (D3DFORMAT)MAKESRGBFMT(D3DFMT_LE_X8R8G8B8);
   else
      d3dpp->FrontBufferFormat       = D3DFMT_LE_X8R8G8B8;
   d3dpp->MultiSampleQuality      = 0;
#endif
#endif
}

static void gfx_ctx_d3d_check_window(bool *quit,
   bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   d3d_video_t *d3d = (d3d_video_t*)driver.video_data;
   *quit = false;
   *resize = false;

   if (d3d_quit)
      *quit = true;
#ifdef _XBOX
   if (d3d->should_resize)
      *resize = true;
#else
   MSG msg;

   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
#endif
}

static bool gfx_ctx_d3d_has_focus(void)
{
#ifdef _XBOX
   return true;
#else
   d3d_video_t *d3d = (d3d_video_t*)driver.video_data;
   return GetFocus() == d3d->hWnd;
#endif
}

static bool gfx_ctx_d3d_bind_api(enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)major;
   (void)minor;
   (void)api;
#if defined(_XBOX1)
   return api == GFX_CTX_DIRECT3D8_API;
#else /* As long as we don't have a D3D11 implementation, we default to this */
   return api == GFX_CTX_DIRECT3D9_API;
#endif
}

static bool gfx_ctx_d3d_init(void)
{
   return true;
}

static void gfx_ctx_d3d_destroy(void)
{
#ifdef _XBOX
   d3d_video_t * d3d = (d3d_video_t*)driver.video_data;

   if (d3d->dev)
      d3d->dev->Release();
   d3d->dev = 0;

   if (d3d->g_pD3D)
      d3d->g_pD3D->Release();
   d3d->g_pD3D = 0;
#endif
}

static void gfx_ctx_d3d_input_driver(const input_driver_t **input, void **input_data)
{
#ifdef _XBOX
   void *xinput = input_xinput.init();
   *input = xinput ? (const input_driver_t*)&input_xinput : NULL;
   *input_data = xinput;
#else
   dinput = input_dinput.init();
   *input = dinput ? &input_dinput : NULL;
   *input_data = dinput;
#endif
}

static void gfx_ctx_d3d_get_video_size(unsigned *width, unsigned *height)
{
#ifdef _XBOX
   (void)width;
   (void)height;
#if defined(_XBOX360)
   XVIDEO_MODE video_mode;
   XGetVideoMode(&video_mode);

   *width  = video_mode.dwDisplayWidth;
   *height = video_mode.dwDisplayHeight;

   if(video_mode.fIsHiDef)
   {
      *width = 1280;
      *height = 720;
      g_extern.lifecycle_state |= (1ULL << MODE_MENU_HD);
   }
   else
   {
	   *width = 640;
	   *height = 480;
      g_extern.lifecycle_state &= ~(1ULL << MODE_MENU_HD);
   }

   if(video_mode.fIsWideScreen)
	   g_extern.lifecycle_state |= (1ULL << MODE_MENU_WIDESCREEN);
   else
      g_extern.lifecycle_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
#elif defined(_XBOX1)
   DWORD video_mode = XGetVideoFlags();

    *width  = 640;
    *height = 480;

   // Only valid in PAL mode, not valid for HDTV modes!
   if(XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      // Check for 16:9 mode (PAL REGION)
      if(video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         if(video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
         {	//60 Hz, 720x480i
            *width = 720;
            *height = 480;
         }
         else
         {	//50 Hz, 720x576i
            *width = 720;
            *height = 576;
         }
         g_extern.lifecycle_state |= (1ULL << MODE_MENU_WIDESCREEN);
      }
      else
         g_extern.lifecycle_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
   }
   else
   {
      // Check for 16:9 mode (NTSC REGIONS)
      if(video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width = 720;
         *height = 480;
         g_extern.lifecycle_state |= (1ULL << MODE_MENU_WIDESCREEN);
      }
	  else
       g_extern.lifecycle_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
   }

   if(XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if(video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      {
         *width	= 640;
         *height  = 480;
         g_extern.lifecycle_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
         g_extern.lifecycle_state |= (1ULL << MODE_MENU_HD);
      }
	   else if(video_mode & XC_VIDEO_FLAGS_HDTV_720p)
      {
         *width	= 1280;
         *height  = 720;
         g_extern.lifecycle_state |= (1ULL << MODE_MENU_WIDESCREEN);
         g_extern.lifecycle_state |= (1ULL << MODE_MENU_HD);
      }
	   else if(video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
      {
         *width	= 1920;
         *height  = 1080;
         g_extern.lifecycle_state |= (1ULL << MODE_MENU_WIDESCREEN);
         g_extern.lifecycle_state |= (1ULL << MODE_MENU_HD);
      }
   }
#endif
#endif
}

static void gfx_ctx_d3d_swap_interval(unsigned interval)
{
#ifdef _XBOX
   d3d_video_t *d3d = (d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = d3d->dev;

   if (interval)
      d3dr->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_ONE);
   else
      d3dr->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_IMMEDIATE);
#endif
}

const gfx_ctx_driver_t gfx_ctx_d3d9 = {
   gfx_ctx_d3d_init,
   gfx_ctx_d3d_destroy,
   gfx_ctx_d3d_bind_api,
   gfx_ctx_d3d_swap_interval,
   NULL,
   gfx_ctx_d3d_get_video_size,
   NULL,							
   gfx_ctx_d3d_update_title,
   gfx_ctx_d3d_check_window,
   d3d_resize,
   gfx_ctx_d3d_has_focus,
   gfx_ctx_d3d_swap_buffers,
   gfx_ctx_d3d_input_driver,
   NULL,
   gfx_ctx_d3d_show_mouse,
   "d3d",
};
