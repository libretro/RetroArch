/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "../../console/rarch_console.h"

#include "../image.h"

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../fonts/d3d_font.h"

#if defined(_XBOX1)
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#elif defined(_XBOX360)
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTINTERVAL
#endif

static void gfx_ctx_xdk_set_swap_interval(unsigned interval)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->d3d_render_device;

   if (interval)
      d3dr->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_ONE);
   else
      d3dr->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_IMMEDIATE);
}

static void gfx_ctx_xdk_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   *quit = false;
   *resize = false;

   if (d3d->quitting)
      *quit = true;

   if (d3d->should_resize)
      *resize = true;
}

static void gfx_ctx_xdk_set_resize(unsigned width, unsigned height) { }

static void gfx_ctx_xdk_swap_buffers(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = d3d->d3d_render_device;
   RD3DDevice_Present(d3dr);
}

static bool gfx_ctx_xdk_window_has_focus(void)
{
   return true;
}

static void gfx_ctx_xdk_update_window_title(void)
{
   char buf[128];
   gfx_get_fps(buf, sizeof(buf), false);
}

static void gfx_ctx_xdk_get_video_size(unsigned *width, unsigned *height)
{
   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;
#if defined(_XBOX360)
   XVIDEO_MODE video_mode;
   XGetVideoMode(&video_mode);

   *width  = video_mode.dwDisplayWidth;
   *height = video_mode.dwDisplayHeight;

   if(video_mode.fIsHiDef)
   {
      *width = 1280;
      *height = 720;
      g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_HD);
   }
   else
   {
	   *width = 640;
	   *height = 480;
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_HD);
   }

   if(video_mode.fIsWideScreen)
	   g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_WIDESCREEN);
   else
      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
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
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_WIDESCREEN);
      }
      else
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
   }
   else
   {
      // Check for 16:9 mode (NTSC REGIONS)
      if(video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width = 720;
         *height = 480;
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_WIDESCREEN);
      }
	  else
       g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
   }

   if(XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if(video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      {
         *width	= 640;
         *height  = 480;
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_WIDESCREEN);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_HD);
      }
	   else if(video_mode & XC_VIDEO_FLAGS_HDTV_720p)
      {
         *width	= 1280;
         *height  = 720;
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_WIDESCREEN);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_HD);
      }
	   else if(video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
      {
         *width	= 1920;
         *height  = 1080;
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_WIDESCREEN);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_HD);
      }
   }
#else
   /* TODO: implement */
   (void)width;
   (void)height;
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
      return NULL;
   }

   return true;
}

static bool gfx_ctx_xdk_set_video_mode(
      unsigned width, unsigned height, bool fullscreen)
{
   return true;
}

static void gfx_ctx_xdk_destroy(void)
{
   xdk_d3d_video_t * d3d = (xdk_d3d_video_t*)driver.video_data;

#ifdef HAVE_RGUI
   texture_image_free(&d3d->rgui_texture);
#endif

   if (d3d->d3d_render_device)
   {
      d3d->d3d_render_device->Release();
      d3d->d3d_render_device = 0;
   }

   if (d3d->d3d_device)
   {
      d3d->d3d_device->Release();
      d3d->d3d_device = 0;
   }
}

static void gfx_ctx_xdk_input_driver(const input_driver_t **input, void **input_data) { }

static bool gfx_ctx_xdk_bind_api(enum gfx_ctx_api api)
{
#if defined(_XBOX1)
   return api == GFX_CTX_DIRECT3D8_API;
#elif defined(_XBOX360)
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
