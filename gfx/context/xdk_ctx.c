/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "../../driver.h"

#ifdef _XBOX
#include "../../xdk/xdk_d3d.h"
#endif

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../fonts/d3d_font.h"

#if defined(_XBOX1)
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#define PresentationInterval FullScreen_PresentationInterval
#elif defined(_XBOX360)
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTINTERVAL
#endif

void xdk_d3d_generate_pp(D3DPRESENT_PARAMETERS *d3dpp, const video_info_t *video)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   memset(d3dpp, 0, sizeof(*d3dpp));

#ifdef _XBOX
   d3dpp->Windowed = false;
#else
   d3dpp->Windowed = g_settings.video.windowed_fullscreen || !video->fullscreen;
#endif

   if (video->vsync)
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

   d3dpp->SwapEffect              = D3DSWAPEFFECT_DISCARD;
#ifdef _XBOX
   d3dpp->hDeviceWindow = 0;
#else
   d3dpp->hDeviceWindow = hWnd;
#endif
   d3dpp->BackBufferCount = 2;
#ifdef _XBOX360
   d3dpp->BackBufferFormat = g_extern.console.screen.gamma_correction ? (D3DFORMAT)MAKESRGBFMT(d3d->texture_fmt)
      : d3d->texture_fmt;
#else
   d3dpp->BackBufferFormat = !d3dpp->Windowed ? D3DFMT_X8R8G8B8 : D3DFMT_UNKNOWN;
#endif

   if (!d3dpp->Windowed)
   {
      unsigned width, height;
      width = 0;
      height = 0;

      if (d3d->ctx_driver && d3d->ctx_driver->get_video_size)
         d3d->ctx_driver->get_video_size(&width, &height);

      d3dpp->BackBufferWidth  = d3d->win_width = width;
      d3dpp->BackBufferHeight = d3d->win_height = height;
   }

   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;

   d3d->texture_fmt = video->rgb32 ? D3DFMT_X8R8G8B8 : D3DFMT_LIN_R5G6B5;
   d3d->base_size   = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

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
}


static void gfx_ctx_xdk_set_swap_interval(unsigned interval)
{
#ifdef _XBOX
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->d3d_render_device;

   if (interval)
      d3dr->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_ONE);
   else
      d3dr->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_IMMEDIATE);
#endif
}

static void gfx_ctx_xdk_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
#ifdef _XBOX
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   *quit = false;
   *resize = false;

   if (d3d->quitting)
      *quit = true;

   if (d3d->should_resize)
      *resize = true;
#else
   MSG msg;
   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
   return !Callback::quit;
#endif
}

static void d3d_restore(void)
{
#ifndef _XBOX
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d_deinit();
   d3d->needs_restore = !d3d_init(&d3d->video_info);

   if (d3d->needs_restore)
      RARCH_ERR("[D3D]: Restore error.\n");

   return !d3d->needs_restore;
#endif
}

static void gfx_ctx_xdk_set_resize(unsigned width, unsigned height)
{
#ifndef _XBOX
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = d3d->d3d_render_device;

   if (!d3dr)
      return;

   RARCH_LOG("[D3D]: Resize %ux%u.\n", new_width, new_height);

   if (d3d->new_width != d3d->video_info.width || d3d->new_height != d3d->video_info.height)
   {
      d3d->video_info.width = d3d->screen_width = d3d->new_width;
      d3d->video_info.height = d3d->screen_height = d3d->new_height;
      d3d_restore();
   }
#endif
}

static void gfx_ctx_xdk_swap_buffers(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = d3d->d3d_render_device;

#ifdef _XBOX
   RD3DDevice_Present(d3dr);
#else
   if (d3dr->Present(NULL, NULL, NULL, NULL) != D3D_OK)
   {
      d3dr->needs_restore = true;
      RARCH_ERR("[D3D]: Present() failed.\n");
   }
#endif
}

static bool gfx_ctx_xdk_window_has_focus(void)
{
#ifdef _XBOX
   return true;
#else
   return GetFocus() == hWnd;
#endif
}

static void gfx_ctx_xdk_update_window_title(void)
{
   char buffer[128], buffer_fps[128];
   bool fps_draw = g_settings.fps_show;

   if (gfx_get_fps(buffer, sizeof(buffer), fps_draw ? buffer_fps : NULL, sizeof(buffer_fps)))
   {
#ifndef _XBOX
      std::string title = buffer;
      title += " || Direct3D9";
      SetWindowText(hWnd, title.c_str());
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

#ifndef _XBOX
   g_extern.frame_count++;
#endif
}

static void gfx_ctx_xdk_get_video_size(unsigned *width, unsigned *height)
{
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
}

static bool gfx_ctx_xdk_init(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d->d3d_device = direct3d_create_ctx(D3D_SDK_VERSION);
   if (!d3d->d3d_device)
   {
      RARCH_ERR("Could not create Direct3D context.\n");
      free(d3d);
      return false;
   }

   return true;
}

static bool gfx_ctx_xdk_set_video_mode(
      unsigned width, unsigned height, bool fullscreen)
{
   (void)width;
   (void)height;
   (void)fullscreen;
   return true;
}

static void gfx_ctx_xdk_destroy(void)
{
   xdk_d3d_video_t * d3d = (xdk_d3d_video_t*)driver.video_data;

   if (d3d->d3d_render_device)
      d3d->d3d_render_device->Release();
   d3d->d3d_render_device = 0;

   if (d3d->d3d_device)
      d3d->d3d_device->Release();
   d3d->d3d_device = 0;
}

static void gfx_ctx_xdk_input_driver(const input_driver_t **input, void **input_data)
{
#ifdef _XBOX
   void *xinput = input_xinput.init();
   *input = xinput ? (const input_driver_t*)&input_xinput : NULL;
   *input_data = xinput;
#endif
}

static bool gfx_ctx_xdk_bind_api(enum gfx_ctx_api api, unsigned major, unsigned minor)
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

const gfx_ctx_driver_t gfx_ctx_xdk = {
   gfx_ctx_xdk_init,
   gfx_ctx_xdk_destroy,
   gfx_ctx_xdk_bind_api,
   gfx_ctx_xdk_set_swap_interval,
   gfx_ctx_xdk_set_video_mode,
   gfx_ctx_xdk_get_video_size,
   NULL,
   gfx_ctx_xdk_update_window_title,
   gfx_ctx_xdk_check_window,
   gfx_ctx_xdk_set_resize,
   gfx_ctx_xdk_window_has_focus,
   gfx_ctx_xdk_swap_buffers,
   gfx_ctx_xdk_input_driver,
   NULL,
   NULL,
   "xdk",
};
