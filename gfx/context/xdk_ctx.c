/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include <xgraphics.h>

#include "../../screenshot.h"

#if defined(_XBOX1)
// for Xbox 1
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#else
// for Xbox 360
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTINTERVAL
#endif

static void gfx_ctx_xdk_set_blend(bool enable)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if(enable)
   {
      d3d->d3d_render_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      d3d->d3d_render_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   }
   d3d->d3d_render_device->SetRenderState(D3DRS_ALPHABLENDENABLE, enable);
}

static void gfx_ctx_xdk_set_swap_interval(unsigned interval)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if (interval)
      d3d->d3d_render_device->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_ONE);
   else
      d3d->d3d_render_device->SetRenderState(XBOX_PRESENTATIONINTERVAL, D3DPRESENT_INTERVAL_IMMEDIATE);
}

static void gfx_ctx_xdk_get_available_resolutions (void)
{
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
#ifdef _XBOX1
   d3d->d3d_render_device->EndScene();
#endif
   d3d->d3d_render_device->Present(NULL, NULL, NULL, NULL);
}

static void gfx_ctx_xdk_clear(void)
{
   xdk_d3d_video_t *device_ptr = (xdk_d3d_video_t*)driver.video_data;
#ifdef _XBOX1
   unsigned flicker_filter = g_extern.console.screen.state.flicker_filter.value;
   bool soft_filter_enable = g_extern.console.screen.state.soft_filter.enable;
#endif

   device_ptr->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
      D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
#ifdef _XBOX1
   device_ptr->d3d_render_device->BeginScene();
   device_ptr->d3d_render_device->SetFlickerFilter(flicker_filter);
   device_ptr->d3d_render_device->SetSoftDisplayFilter(soft_filter_enable);
#endif
}

static bool gfx_ctx_xdk_window_has_focus(void)
{
   return true;
}

static bool gfx_ctx_xdk_menu_init(void)
{
   return true;
}

static void gfx_ctx_xdk_update_window_title(bool reset) { }

static void gfx_ctx_xdk_get_video_size(unsigned *width, unsigned *height)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
#if defined(_XBOX360)
   XVIDEO_MODE video_mode;
   XGetVideoMode(&video_mode);

   *width  = video_mode.dwDisplayWidth;
   *height = video_mode.dwDisplayHeight;
#elif defined(_XBOX1)
   DWORD video_mode = XGetVideoFlags();

    *width  = 640;
    *height = 480;

   // Only valid in PAL mode, not valid for HDTV modes!
   if(XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      // Check for 16:9 mode (PAL REGION)
      if(d3d->video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         if(d3d->video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
	      {	//60 Hz, 720x480i
            *width = 720;
	         *height = 480;
	      }
	    else
	      {	//50 Hz, 720x576i
           *width = 720;
           *height = 576;
	      }
      }
   }
   else
   {
      // Check for 16:9 mode (NTSC REGIONS)
      if(d3d->video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width = 720;
	      *height = 480;
      }
   }

   if(XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      {
         *width	= 640;
         *height  = 480;
      }
	   else if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_720p)
	   {
         *width	= 1280;
         *height  = 720;
	   }
	   else if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
	   {
         *width	= 1920;
         *height  = 1080;
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
      free(d3d);
      return NULL;
   }

   memset(&d3d->d3dpp, 0, sizeof(d3d->d3dpp));

#if defined(_XBOX1)
// Get the "video mode"
   d3d->video_mode = XGetVideoFlags();

   // Check if we are able to use progressive mode
   if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      d3d->d3dpp.Flags = D3DPRESENTFLAG_PROGRESSIVE;
   else
      d3d->d3dpp.Flags = D3DPRESENTFLAG_INTERLACED;

    // Safe mode
    d3d->d3dpp.BackBufferWidth = 640;
    d3d->d3dpp.BackBufferHeight = 480;
    g_extern.console.rmenu.state.rmenu_hd.enable = false;

   // Only valid in PAL mode, not valid for HDTV modes!
   if(XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      if(d3d->video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
         d3d->d3dpp.FullScreen_RefreshRateInHz = 60;
      else
         d3d->d3dpp.FullScreen_RefreshRateInHz = 50;

      // Check for 16:9 mode (PAL REGION)
      if(d3d->video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         if(d3d->video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
	      {	//60 Hz, 720x480i
            d3d->d3dpp.BackBufferWidth = 720;
	         d3d->d3dpp.BackBufferHeight = 480;
	      }
	    else
	      {	//50 Hz, 720x576i
           d3d->d3dpp.BackBufferWidth = 720;
           d3d->d3dpp.BackBufferHeight = 576;
	      }
      }
   }
   else
   {
      // Check for 16:9 mode (NTSC REGIONS)
      if(d3d->video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         d3d->d3dpp.BackBufferWidth = 720;
	      d3d->d3dpp.BackBufferHeight = 480;
      }
   }

   if(XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      {
         g_extern.console.rmenu.state.rmenu_hd.enable = false;
         d3d->d3dpp.BackBufferWidth	= 640;
         d3d->d3dpp.BackBufferHeight = 480;
         d3d->d3dpp.Flags = D3DPRESENTFLAG_PROGRESSIVE;
      }
      else if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_720p)
      {
         g_extern.console.rmenu.state.rmenu_hd.enable = true;
	 d3d->d3dpp.BackBufferWidth	= 1280;
	 d3d->d3dpp.BackBufferHeight = 720;
	 d3d->d3dpp.Flags = D3DPRESENTFLAG_PROGRESSIVE;
      }
      else if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
      {
         g_extern.console.rmenu.state.rmenu_hd.enable = true;
	 d3d->d3dpp.BackBufferWidth	= 1920;
	 d3d->d3dpp.BackBufferHeight = 1080;
	 d3d->d3dpp.Flags = D3DPRESENTFLAG_INTERLACED;
      }
   }

   d3d->win_width = d3d->d3dpp.BackBufferWidth;
   d3d->win_height = d3d->d3dpp.BackBufferHeight;

   if(d3d->d3dpp.BackBufferWidth > 640 && ((float)d3d->d3dpp.BackBufferHeight / (float)d3d->d3dpp.BackBufferWidth != 0.75) ||
      ((d3d->d3dpp.BackBufferWidth == 720) && (d3d->d3dpp.BackBufferHeight == 576))) // 16:9
        d3d->d3dpp.Flags |= D3DPRESENTFLAG_WIDESCREEN;
  // no letterboxing in 4:3 mode (if widescreen is unsupported
   d3d->d3dpp.BackBufferFormat                     = D3DFMT_A8R8G8B8;
   d3d->d3dpp.FullScreen_PresentationInterval		= d3d->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
   d3d->d3dpp.MultiSampleType                      = D3DMULTISAMPLE_NONE;
   d3d->d3dpp.BackBufferCount                      = 2;
   d3d->d3dpp.EnableAutoDepthStencil               = FALSE;
   d3d->d3dpp.SwapEffect                           = D3DSWAPEFFECT_COPY;

   d3d->d3d_device->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3d->d3dpp, &d3d->d3d_render_device);

   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);

   // use an orthogonal matrix for the projection matrix
   D3DXMATRIX mat;
   D3DXMatrixOrthoOffCenterLH(&mat, 0,  d3d->d3dpp.BackBufferWidth ,  d3d->d3dpp.BackBufferHeight , 0, 0.0f, 1.0f);

   d3d->d3d_render_device->SetTransform(D3DTS_PROJECTION, &mat);

   // use an identity matrix for the world and view matrices
   D3DXMatrixIdentity(&mat);
   d3d->d3d_render_device->SetTransform(D3DTS_WORLD, &mat);
   d3d->d3d_render_device->SetTransform(D3DTS_VIEW, &mat);

   d3d->d3d_render_device->CreateTexture(512, 512, 1, 0, D3DFMT_LIN_X1R5G5B5, 0, &d3d->lpTexture);
   D3DLOCKED_RECT d3dlr;
   d3d->lpTexture->LockRect(0, &d3dlr, NULL, 0);
   memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
   d3d->lpTexture->UnlockRect(0);

   d3d->last_width = 512;
   d3d->last_height = 512;

   d3d->d3d_render_device->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats), 
	   D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &d3d->vertex_buf);

   const DrawVerticeFormats init_verts[] = {
      { -1.0f, -1.0f, 1.0f, 0.0f, 1.0f },
      {  1.0f, -1.0f, 1.0f, 1.0f, 1.0f },
      { -1.0f,  1.0f, 1.0f, 0.0f, 0.0f },
      {  1.0f,  1.0f, 1.0f, 1.0f, 0.0f },
   };

   BYTE *verts_ptr;
   d3d->vertex_buf->Lock(0, 0, &verts_ptr, 0);
   memcpy(verts_ptr, init_verts, sizeof(init_verts));
   d3d->vertex_buf->Unlock();

   d3d->d3d_render_device->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX1);

   d3d->d3d_render_device->SetRenderState(D3DRS_LIGHTING, FALSE);
   d3d->d3d_render_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
   d3d->d3d_render_device->SetRenderState(D3DRS_ZENABLE, FALSE);

   D3DVIEWPORT vp = {0};
   vp.Width  = d3d->d3dpp.BackBufferWidth;
   vp.Height = d3d->d3dpp.BackBufferHeight;
#elif defined(_XBOX360)
   // no letterboxing in 4:3 mode (if widescreen is
   // unsupported
   // Get video settings
   memset(&d3d->video_mode, 0, sizeof(d3d->video_mode));
   XGetVideoMode(&d3d->video_mode);

   if(!d3d->video_mode.fIsWideScreen)
      d3d->d3dpp.Flags |= D3DPRESENTFLAG_NO_LETTERBOX;

   g_extern.console.rmenu.state.rmenu_hd.enable = d3d->video_mode.fIsHiDef;
   
   d3d->d3dpp.BackBufferWidth         = d3d->video_mode.fIsHiDef ? 1280 : 640;
   d3d->d3dpp.BackBufferHeight        = d3d->video_mode.fIsHiDef ? 720 : 480;

   if(g_extern.console.screen.gamma_correction)
   {
      d3d->d3dpp.BackBufferFormat        = g_settings.video.color_format ? (D3DFORMAT)MAKESRGBFMT(D3DFMT_A8R8G8B8) : (D3DFORMAT)MAKESRGBFMT(D3DFMT_LIN_A1R5G5B5);
      d3d->d3dpp.FrontBufferFormat       = (D3DFORMAT)MAKESRGBFMT(D3DFMT_LE_X8R8G8B8);
   }
   else
   {
      d3d->d3dpp.BackBufferFormat        = g_settings.video.color_format ? D3DFMT_A8R8G8B8 : D3DFMT_LIN_A1R5G5B5;
      d3d->d3dpp.FrontBufferFormat       = D3DFMT_LE_X8R8G8B8;
   }
   d3d->d3dpp.MultiSampleQuality      = 0;
   d3d->d3dpp.PresentationInterval    = d3d->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

   d3d->d3dpp.MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3d->d3dpp.BackBufferCount         = 2;
   d3d->d3dpp.EnableAutoDepthStencil  = FALSE;
   d3d->d3dpp.SwapEffect              = D3DSWAPEFFECT_DISCARD;

   d3d->d3d_device->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
	   &d3d->d3dpp, &d3d->d3d_render_device);

   d3d->d3d_render_device->CreateTexture(512, 512, 1, 0, D3DFMT_LIN_X1R5G5B5,
      0, &d3d->lpTexture
   , NULL
   );

   D3DLOCKED_RECT d3dlr;
   d3d->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
   d3d->lpTexture->UnlockRect(0);

   d3d->last_width = 512;
   d3d->last_height = 512;

   d3d->d3d_render_device->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats), 
	   0, 0, 0, &d3d->vertex_buf, NULL);

   static const DrawVerticeFormats init_verts[] = {
      { -1.0f, -1.0f, 0.0f, 1.0f },
      {  1.0f, -1.0f, 1.0f, 1.0f },
      { -1.0f,  1.0f, 0.0f, 0.0f },
      {  1.0f,  1.0f, 1.0f, 0.0f },
   };
   
   void *verts_ptr;
   d3d->vertex_buf->Lock(0, 0, &verts_ptr, 0);
   memcpy(verts_ptr, init_verts, sizeof(init_verts));
   d3d->vertex_buf->Unlock();

   static const D3DVERTEXELEMENT VertexElements[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      D3DDECL_END()
   };

   d3d->d3d_render_device->CreateVertexDeclaration(VertexElements, &d3d->v_decl);
   
   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
	   0xff000000, 1.0f, 0);

   d3d->d3d_render_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
   d3d->d3d_render_device->SetRenderState(D3DRS_ZENABLE, FALSE);

   D3DVIEWPORT vp = {0};
   vp.Width  = d3d->video_mode.fIsHiDef ? 1280 : 640;
   vp.Height = d3d->video_mode.fIsHiDef ? 720 : 480;
#endif
   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;
   d3d->d3d_render_device->SetViewport(&vp);

   if(g_extern.console.screen.viewports.custom_vp.width == 0)
      g_extern.console.screen.viewports.custom_vp.width = vp.Width;

   if(g_extern.console.screen.viewports.custom_vp.height == 0)
      g_extern.console.screen.viewports.custom_vp.height = vp.Height;

   return true;
}

static bool gfx_ctx_xdk_set_video_mode(
      unsigned width, unsigned height,
      unsigned bits, bool fullscreen)
{
   /* TODO: implement */
   return true;
}

static void gfx_ctx_xdk_destroy(void)
{
   xdk_d3d_video_t * d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d->d3d_render_device->Release();
   d3d->d3d_device->Release();
}

static void gfx_ctx_xdk_input_driver(const input_driver_t **input, void **input_data) { }

static void gfx_ctx_xdk_set_filtering(unsigned index, bool set_smooth)
{
   /* TODO: implement */
}

static void gfx_ctx_xdk_set_fbo(bool enable)
{
#ifdef HAVE_FBO
   /* TODO: implement properly */
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d->fbo_enabled = enable;
#endif
}

void gfx_ctx_xdk_apply_fbo_state_changes(unsigned mode)
{
}

void gfx_ctx_xdk_screenshot_dump(void *data)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   HRESULT ret = S_OK;
   char filename[PATH_MAX];
   char shotname[PATH_MAX];

   screenshot_generate_filename(shotname, sizeof(shotname));
   snprintf(filename, sizeof(filename), "%s\\%s", default_paths.screenshots_dir, shotname);
   
#if defined(_XBOX1)
   D3DSurface *surf = NULL;
   d3d->d3d_render_device->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &surf);
   ret = XGWriteSurfaceToFile(surf, filename);
   surf->Release();
#elif defined(_XBOX360)
   ret = 1; //false
   //ret = D3DXSaveTextureToFile(filename, D3DXIFF_BMP, d3d->lpTexture, NULL);
#endif

   if(ret == S_OK)
   {
      RARCH_LOG("Screenshot saved: %s.\n", filename);
      msg_queue_push(g_extern.msg_queue, "Screenshot saved.", 1, 30);
   }
}

static bool gfx_ctx_xdk_bind_api(enum gfx_ctx_api api)
{
#if defined(_XBOX1)
   return api == GFX_CTX_DIRECT3D8_API;
#elif defined(_XBOX360)
   return api == GFX_CTX_DIRECT3D9_API;
#endif
}

/*============================================================
	MISC
        TODO: Refactor
============================================================ */

void gfx_ctx_set_overscan(void)
{
   /* TODO: implement */
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   if (!d3d)
      return;

   d3d->should_resize = true;
}

int gfx_ctx_xdk_check_resolution(unsigned resolution_id)
{
   /* TODO: implement */
   return 0;
}

bool gfx_ctx_can_egl_image_buffer(void)
{
   return false;
}

bool gfx_ctx_write_egl_image(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, void **image_handle)
{
   return false;
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
   gfx_ctx_can_egl_image_buffer,
   gfx_ctx_write_egl_image,
   "xdk",

   // RARCH_CONSOLE stuff.
   gfx_ctx_xdk_set_filtering,
   gfx_ctx_xdk_get_available_resolutions,
   gfx_ctx_xdk_check_resolution,

   gfx_ctx_xdk_menu_init,

   gfx_ctx_xdk_set_fbo,
   gfx_ctx_xdk_apply_fbo_state_changes,
};
