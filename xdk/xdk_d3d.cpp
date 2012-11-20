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

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include "../driver.h"
#include "xdk_d3d.h"

#ifdef HAVE_HLSL
#include "../gfx/shader_hlsl.h"
#endif

#ifdef _XBOX1
#include "./../gfx/fonts/xdk1_xfonts.h"
#endif

#include "./../gfx/gfx_context.h"
#include "../general.h"
#include "../message.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _XBOX360
#include "../gfx/fonts/xdk360_fonts.h"
#endif

#include "../xdk/xdk_resources.h"

#if defined(_XBOX1)
wchar_t strw_buffer[128];
unsigned font_x, font_y;
FLOAT angle;
#elif defined(_XBOX360)
const DWORD g_MapLinearToSrgbGpuFormat[] = 
{
   GPUTEXTUREFORMAT_1_REVERSE,
   GPUTEXTUREFORMAT_1,
   GPUTEXTUREFORMAT_8,
   GPUTEXTUREFORMAT_1_5_5_5,
   GPUTEXTUREFORMAT_5_6_5,
   GPUTEXTUREFORMAT_6_5_5,
   GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16,
   GPUTEXTUREFORMAT_2_10_10_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_8_A,
   GPUTEXTUREFORMAT_8_B,
   GPUTEXTUREFORMAT_8_8,
   GPUTEXTUREFORMAT_Cr_Y1_Cb_Y0_REP,     
   GPUTEXTUREFORMAT_Y1_Cr_Y0_Cb_REP,      
   GPUTEXTUREFORMAT_16_16_EDRAM,          
   GPUTEXTUREFORMAT_8_8_8_8_A,
   GPUTEXTUREFORMAT_4_4_4_4,
   GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16,
   GPUTEXTUREFORMAT_11_11_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16,  
   GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16,
   GPUTEXTUREFORMAT_16_16_16_16_EDRAM,
   GPUTEXTUREFORMAT_24_8,
   GPUTEXTUREFORMAT_24_8_FLOAT,
   GPUTEXTUREFORMAT_16,
   GPUTEXTUREFORMAT_16_16,
   GPUTEXTUREFORMAT_16_16_16_16,
   GPUTEXTUREFORMAT_16_EXPAND,
   GPUTEXTUREFORMAT_16_16_EXPAND,
   GPUTEXTUREFORMAT_16_16_16_16_EXPAND,
   GPUTEXTUREFORMAT_16_FLOAT,
   GPUTEXTUREFORMAT_16_16_FLOAT,
   GPUTEXTUREFORMAT_16_16_16_16_FLOAT,
   GPUTEXTUREFORMAT_32,
   GPUTEXTUREFORMAT_32_32,
   GPUTEXTUREFORMAT_32_32_32_32,
   GPUTEXTUREFORMAT_32_FLOAT,
   GPUTEXTUREFORMAT_32_32_FLOAT,
   GPUTEXTUREFORMAT_32_32_32_32_FLOAT,
   GPUTEXTUREFORMAT_32_AS_8,
   GPUTEXTUREFORMAT_32_AS_8_8,
   GPUTEXTUREFORMAT_16_MPEG,
   GPUTEXTUREFORMAT_16_16_MPEG,
   GPUTEXTUREFORMAT_8_INTERLACED,
   GPUTEXTUREFORMAT_32_AS_8_INTERLACED,
   GPUTEXTUREFORMAT_32_AS_8_8_INTERLACED,
   GPUTEXTUREFORMAT_16_INTERLACED,
   GPUTEXTUREFORMAT_16_MPEG_INTERLACED,
   GPUTEXTUREFORMAT_16_16_MPEG_INTERLACED,
   GPUTEXTUREFORMAT_DXN,
   GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16,
   GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16,
   GPUTEXTUREFORMAT_2_10_10_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16,
   GPUTEXTUREFORMAT_11_11_10_AS_16_16_16_16,
   GPUTEXTUREFORMAT_32_32_32_FLOAT,
   GPUTEXTUREFORMAT_DXT3A,
   GPUTEXTUREFORMAT_DXT5A,
   GPUTEXTUREFORMAT_CTX1,
   GPUTEXTUREFORMAT_DXT3A_AS_1_1_1_1,
   GPUTEXTUREFORMAT_8_8_8_8_GAMMA_EDRAM,
   GPUTEXTUREFORMAT_2_10_10_10_FLOAT_EDRAM,
};
#endif

static void check_window(xdk_d3d_video_t *d3d)
{
   bool quit, resize;

   d3d->ctx_driver->check_window(&quit,
         &resize, NULL, NULL,
         d3d->frame_count);

   if (quit)
      d3d->quitting = true;
   else if (resize)
      d3d->should_resize = true;
}

#ifdef HAVE_HLSL
static bool hlsl_shader_init(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   const char *shader_path = g_settings.video.cg_shader_path;

   return hlsl_init(g_settings.video.cg_shader_path, d3d->d3d_render_device);
}
#endif

static void xdk_d3d_free(void * data)
{
#ifdef RARCH_CONSOLE
   if (driver.video_data)
	   return;
#endif

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_HLSL
   hlsl_deinit();
#endif
#ifdef HAVE_D3D9
   d3d9_deinit_font();
#endif

   d3d->ctx_driver->destroy();

   free(d3d);
}

#ifdef _XBOX360

static void xdk_convert_texture_to_as16_srgb( D3DTexture *pTexture )
{
   pTexture->Format.SignX = GPUSIGN_GAMMA;
   pTexture->Format.SignY = GPUSIGN_GAMMA;
   pTexture->Format.SignZ = GPUSIGN_GAMMA;

   XGTEXTURE_DESC desc;
   XGGetTextureDesc( pTexture, 0, &desc );

   //convert to AS_16_16_16_16 format
   pTexture->Format.DataFormat = g_MapLinearToSrgbGpuFormat[ (desc.Format & D3DFORMAT_TEXTUREFORMAT_MASK) >> D3DFORMAT_TEXTUREFORMAT_SHIFT ];
}
#endif

static void xdk_d3d_set_viewport(bool force_full)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   int width, height;      // Set the viewport based on the current resolution
   int m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp;
   float m_zNear, m_zFar;

   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
#if defined(_XBOX1)
   // Get the "video mode"
   d3d->video_mode = XGetVideoFlags();

   width  = d3d->d3dpp.BackBufferWidth;
   height = d3d->d3dpp.BackBufferHeight;
#elif defined(_XBOX360)
   width = d3d->video_mode.fIsHiDef ? 1280 : 640;
   height = d3d->video_mode.fIsHiDef ? 720 : 480;
#endif
   m_viewport_x_temp = 0;
   m_viewport_y_temp = 0;
   m_viewport_width_temp = width;
   m_viewport_height_temp = height;
   m_zNear = 0.0f;
   m_zFar = 1.0f;

   if (!force_full)
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)width / height;
      float delta;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
      if(g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
      	 m_viewport_x_temp = g_extern.console.screen.viewports.custom_vp.x;
      	 m_viewport_y_temp = g_extern.console.screen.viewports.custom_vp.y;
      	 m_viewport_width_temp = g_extern.console.screen.viewports.custom_vp.width;
      	 m_viewport_height_temp = g_extern.console.screen.viewports.custom_vp.height;
      }
      else if (device_aspect > desired_aspect)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         m_viewport_x_temp = (int)(width * (0.5 - delta));
         m_viewport_width_temp = (int)(2.0 * width * delta);
         width = (unsigned)(2.0 * width * delta);
      }
      else
      {
         delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         m_viewport_y_temp = (int)(height * (0.5 - delta));
         m_viewport_height_temp = (int)(2.0 * height * delta);
         height = (unsigned)(2.0 * height * delta);
      }
   }

   D3DVIEWPORT vp = {0};
   vp.Width  = m_viewport_width_temp;
   vp.Height = m_viewport_height_temp;
   vp.X      = m_viewport_x_temp;
   vp.Y      = m_viewport_y_temp;
   vp.MinZ   = m_zNear;
   vp.MaxZ   = m_zFar;
   d3d->d3d_render_device->SetViewport(&vp);

#ifdef _XBOX1
   font_x = vp.X;
   font_y = vp.Y;
#endif

   //if(gl->overscan_enable && !force_full)
   //{
   //	m_left = -gl->overscan_amount/2;
   //	m_right = 1 + gl->overscan_amount/2;
   //	m_bottom = -gl->overscan_amount/2;
   //}
}

static void xdk_d3d_set_rotation(void * data, unsigned orientation)
{
   (void)data;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
   FLOAT angle;

   switch(orientation)
   {
      case ORIENTATION_NORMAL:
         angle = M_PI * 0 / 180;
	 break;
      case ORIENTATION_VERTICAL:
         angle = M_PI * 270 / 180;
         break;
      case ORIENTATION_FLIPPED:
         angle = M_PI * 180 / 180;
         break;
      case ORIENTATION_FLIPPED_ROTATED:
         angle = M_PI * 90 / 180;
         break;
   }

#ifdef HAVE_HLSL
   /* TODO: Move to D3DXMATRIX here */
   hlsl_set_proj_matrix(XMMatrixRotationZ(angle));
#endif

   d3d->should_resize = TRUE;
}

#ifdef HAVE_FBO
static void xdk_d3d_init_fbo(xdk_d3d_video_t *d3d)
{
   if(!g_settings.video.render_to_texture)
      return;

   if (d3d->lpTexture_ot)
   {
      d3d->lpTexture_ot->Release();
      d3d->lpTexture_ot = NULL;
   }

   if (d3d->lpSurface)
   {
      d3d->lpSurface->Release();
      d3d->lpSurface = NULL;
   }

   d3d->d3d_render_device->CreateTexture(d3d->tex_w * g_settings.video.fbo.scale_x, d3d->tex_h * g_settings.video.fbo.scale_y,
         1, 0, g_extern.console.screen.gamma_correction ? ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ) : D3DFMT_A8R8G8B8,
         0, &d3d->lpTexture_ot
		 , NULL
		 );

   d3d->d3d_render_device->CreateRenderTarget(d3d->tex_w * g_settings.video.fbo.scale_x, d3d->tex_h * g_settings.video.fbo.scale_y,
         g_extern.console.screen.gamma_correction ? ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ) : D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 
	      0, 0, &d3d->lpSurface, NULL);

   d3d->lpTexture_ot_as16srgb = *d3d->lpTexture_ot;
   xdk_convert_texture_to_as16_srgb(d3d->lpTexture);
   xdk_convert_texture_to_as16_srgb(&d3d->lpTexture_ot_as16srgb);
   d3d->fbo_inited = 1;
}
#endif

void xdk_d3d_generate_pp(D3DPRESENT_PARAMETERS *d3dpp)
{
	xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
	
	memset(d3dpp, 0, sizeof(*d3dpp));

#if defined(_XBOX1)
// Get the "video mode"
   d3d->video_mode = XGetVideoFlags();

   // Check if we are able to use progressive mode
   if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      *d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
   else
      *d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;

    // Safe mode
    *d3dpp->BackBufferWidth = 640;
    *d3dpp->BackBufferHeight = 480;
    g_extern.console.rmenu.state.rmenu_hd.enable = false;

   // Only valid in PAL mode, not valid for HDTV modes!
   if(XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      if(d3d->video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
         *d3dpp->FullScreen_RefreshRateInHz = 60;
      else
         *d3dpp->FullScreen_RefreshRateInHz = 50;

      // Check for 16:9 mode (PAL REGION)
      if(d3d->video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         if(d3d->video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
	      {	//60 Hz, 720x480i
            *d3dpp->BackBufferWidth = 720;
	        *d3dpp->BackBufferHeight = 480;
	      }
	    else
	      {	//50 Hz, 720x576i
           *d3dpp->BackBufferWidth = 720;
           *d3dpp->BackBufferHeight = 576;
	      }
      }
   }
   else
   {
      // Check for 16:9 mode (NTSC REGIONS)
      if(d3d->video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *d3dpp->BackBufferWidth = 720;
	     *d3dpp->BackBufferHeight = 480;
      }
   }

   if(XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      {
         g_extern.console.rmenu.state.rmenu_hd.enable = false;
         *d3dpp->BackBufferWidth	= 640;
         *d3dpp->BackBufferHeight = 480;
         *d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
      }
      else if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_720p)
      {
         g_extern.console.rmenu.state.rmenu_hd.enable = true;
		 *d3dpp->BackBufferWidth	= 1280;
		 *d3dpp->BackBufferHeight = 720;
		 *d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
      }
      else if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
      {
         g_extern.console.rmenu.state.rmenu_hd.enable = true;
		 *d3dpp->BackBufferWidth	= 1920;
		 *d3dpp->BackBufferHeight = 1080;
		 *d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;
      }
   }

   if(d3dpp->BackBufferWidth > 640 && ((float)d3dpp->BackBufferHeight / (float)d3dpp->BackBufferWidth != 0.75) ||
      ((d3dpp->BackBufferWidth == 720) && (d3dpp->BackBufferHeight == 576))) // 16:9
        *d3dpp->Flags |= D3DPRESENTFLAG_WIDESCREEN;
  // no letterboxing in 4:3 mode (if widescreen is unsupported
   *d3dpp->BackBufferFormat                     = D3DFMT_A8R8G8B8;
   *d3dpp->FullScreen_PresentationInterval		= d3d->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
   *d3dpp->MultiSampleType                      = D3DMULTISAMPLE_NONE;
   *d3dpp->BackBufferCount                      = 2;
   *d3dpp->EnableAutoDepthStencil               = FALSE;
   *d3dpp->SwapEffect                           = D3DSWAPEFFECT_COPY;
#elif defined(_XBOX360)
   // no letterboxing in 4:3 mode (if widescreen is
   // unsupported
   // Get video settings
   memset(&d3d->video_mode, 0, sizeof(d3d->video_mode));
   XGetVideoMode(&d3d->video_mode);

   if(!d3d->video_mode.fIsWideScreen)
      d3dpp->Flags |= D3DPRESENTFLAG_NO_LETTERBOX;

   g_extern.console.rmenu.state.rmenu_hd.enable = d3d->video_mode.fIsHiDef;
   
   d3dpp->BackBufferWidth         = d3d->video_mode.fIsHiDef ? 1280 : 640;
   d3dpp->BackBufferHeight        = d3d->video_mode.fIsHiDef ? 720 : 480;

   if(g_extern.console.screen.gamma_correction)
   {
      d3dpp->BackBufferFormat        = g_settings.video.color_format ? (D3DFORMAT)MAKESRGBFMT(D3DFMT_A8R8G8B8) : (D3DFORMAT)MAKESRGBFMT(D3DFMT_LIN_R5G6B5);
      d3dpp->FrontBufferFormat       = (D3DFORMAT)MAKESRGBFMT(D3DFMT_LE_X8R8G8B8);
   }
   else
   {
      d3dpp->BackBufferFormat        = g_settings.video.color_format ? D3DFMT_A8R8G8B8 : D3DFMT_LIN_R5G6B5;
      d3dpp->FrontBufferFormat       = D3DFMT_LE_X8R8G8B8;
   }
   d3dpp->MultiSampleQuality      = 0;
   d3dpp->PresentationInterval    = d3d->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->BackBufferCount         = 2;
   d3dpp->EnableAutoDepthStencil  = FALSE;
   d3dpp->SwapEffect              = D3DSWAPEFFECT_DISCARD;
#endif

   d3d->win_width  = d3dpp->BackBufferWidth;
   d3d->win_height = d3dpp->BackBufferHeight;
}

static void xdk_d3d_init_textures(xdk_d3d_video_t *d3d, const video_info_t *video)
{
	D3DPRESENT_PARAMETERS d3dpp;
	D3DVIEWPORT vp = {0};
	xdk_d3d_generate_pp(&d3dpp);
	
	d3d->d3d_render_device->CreateTexture(d3d->tex_w, d3d->tex_h, 1, 0,
	   g_settings.video.color_format ? D3DFMT_LIN_A8R8G8B8 : D3DFMT_LIN_R5G6B5,
	   0, &d3d->lpTexture
#ifdef _XBOX360
	   , NULL
#endif
	   );
	
	D3DLOCKED_RECT d3dlr;
	d3d->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
	memset(d3dlr.pBits, 0, d3d->tex_w * d3dlr.Pitch);
	d3d->lpTexture->UnlockRect(0);
	
	d3d->last_width = d3d->tex_w;
	d3d->last_height = d3d->tex_h;

#if defined(_XBOX1)
   d3d->d3d_render_device->SetRenderState(D3DRS_LIGHTING, FALSE);

   vp.Width  = d3d->d3dpp.BackBufferWidth;
   vp.Height = d3d->d3dpp.BackBufferHeight;
#elif defined(_XBOX360)
   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
	   0xff000000, 1.0f, 0);

   vp.Width  = d3d->video_mode.fIsHiDef ? 1280 : 640;
   vp.Height = d3d->video_mode.fIsHiDef ? 720 : 480;
#endif

   d3d->d3d_render_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
   d3d->d3d_render_device->SetRenderState(D3DRS_ZENABLE, FALSE);

   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;
   d3d->d3d_render_device->SetViewport(&vp);

   if(g_extern.console.screen.viewports.custom_vp.width == 0)
      g_extern.console.screen.viewports.custom_vp.width = vp.Width;

   if(g_extern.console.screen.viewports.custom_vp.height == 0)
      g_extern.console.screen.viewports.custom_vp.height = vp.Height;
}

static void *xdk_d3d_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   if (driver.video_data)
      return driver.video_data;

   //we'll just use driver.video_data throughout here because it needs to
   //exist when we delegate initing to the context file
   driver.video_data = (xdk_d3d_video_t*)calloc(1, sizeof(xdk_d3d_video_t));
   if (!driver.video_data)
      return NULL;

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d->vsync = video->vsync;
   /* FIXME: Hack */
   d3d->tex_w = 512;
   d3d->tex_h = 512;

#if defined(_XBOX1)
   d3d->ctx_driver = gfx_ctx_init_first(GFX_CTX_DIRECT3D8_API);
#elif defined(_XBOX360)
   d3d->ctx_driver = gfx_ctx_init_first(GFX_CTX_DIRECT3D9_API);
#endif
   if (!d3d->ctx_driver)
   {
      free(d3d);
      return NULL;
   }

   RARCH_LOG("Found D3D context: %s\n", d3d->ctx_driver->ident);

   xdk_d3d_init_textures(d3d, video);

#if defined(_XBOX1)
   // use an orthogonal matrix for the projection matrix
   D3DXMATRIX mat;
   D3DXMatrixOrthoOffCenterLH(&mat, 0,  d3d->d3dpp.BackBufferWidth ,  d3d->d3dpp.BackBufferHeight , 0, 0.0f, 1.0f);

   d3d->d3d_render_device->SetTransform(D3DTS_PROJECTION, &mat);

   // use an identity matrix for the world and view matrices
   D3DXMatrixIdentity(&mat);
   d3d->d3d_render_device->SetTransform(D3DTS_WORLD, &mat);
   d3d->d3d_render_device->SetTransform(D3DTS_VIEW, &mat);

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
#elif defined(_XBOX360)
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
#endif

   d3d->ctx_driver->get_video_size(&d3d->full_x, &d3d->full_y);
   RARCH_LOG("Detecting screen resolution: %ux%u.\n", d3d->full_x, d3d->full_y);

   d3d->ctx_driver->swap_interval(d3d->vsync ? 1 : 0);

#ifdef HAVE_HLSL
   if (!hlsl_shader_init())
   {
      RARCH_ERR("Shader init failed.\n");
	  d3d->ctx_driver->destroy();
	  free(d3d);
	  return NULL;
   }

   RARCH_LOG("D3D: Loaded %u program(s).\n", d3d_hlsl_num());
#endif

#ifdef HAVE_FBO
   xdk_d3d_init_fbo(d3d);
#endif

   xdk_d3d_set_rotation(d3d, g_extern.console.screen.orientation);

   //really returns driver.video_data to driver.video_data - see comment above
   return d3d;
}

static bool xdk_d3d_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   if (!frame)
      return true;

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
#ifdef HAVE_FBO
   D3DSurface* pRenderTarget0;
#endif
   bool menu_enabled = g_extern.console.rmenu.state.rmenu.enable;
   bool fps_enable = g_extern.console.rmenu.state.msg_fps.enable;
#ifdef _XBOX1
   unsigned flicker_filter = g_extern.console.screen.state.flicker_filter.value;
   bool soft_filter_enable = g_extern.console.screen.state.soft_filter.enable;
#endif

   if (d3d->last_width != width || d3d->last_height != height)
   {
      D3DLOCKED_RECT d3dlr;

      d3d->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
      memset(d3dlr.pBits, 0, d3d->tex_w * d3dlr.Pitch);
      d3d->lpTexture->UnlockRect(0);

#if defined(_XBOX1)
      float tex_w = width;  // / 512.0f;
      float tex_h = height; // / 512.0f;

      DrawVerticeFormats verts[] = {
         { -1.0f, -1.0f, 1.0f, 0.0f,  tex_h },
         {  1.0f, -1.0f, 1.0f, tex_w, tex_h },
         { -1.0f,  1.0f, 1.0f, 0.0f,  0.0f },
         {  1.0f,  1.0f, 1.0f, tex_w, 0.0f },
      };
#elif defined(_XBOX360)
      float tex_w = width / (float)d3d->tex_w;
      float tex_h = height / (float)d3d->tex_h;

      DrawVerticeFormats verts[] = {
         { -1.0f, -1.0f, 0.0f,  tex_h },
         {  1.0f, -1.0f, tex_w, tex_h },
         { -1.0f,  1.0f, 0.0f,  0.0f },
         {  1.0f,  1.0f, tex_w, 0.0f },
      };
#endif

      // Align texels and vertices (D3D9 quirk).
      for (unsigned i = 0; i < 4; i++)
      {
         verts[i].x -= 0.5f / (float)d3d->tex_w;
         verts[i].y += 0.5f / (float)d3d->tex_h;
      }

#if defined(_XBOX1)
      BYTE *verts_ptr;
#elif defined(_XBOX360)
      void *verts_ptr;
#endif
      d3d->vertex_buf->Lock(0, 0, &verts_ptr, 0);
      memcpy(verts_ptr, verts, sizeof(verts));
      d3d->vertex_buf->Unlock();

      d3d->last_width = width;
      d3d->last_height = height;
   }

#ifdef HAVE_FBO
   if (d3d->fbo_inited)
   {
      d3d->d3d_render_device->GetRenderTarget(0, &pRenderTarget0);
      d3d->d3d_render_device->SetRenderTarget(0, d3d->lpSurface);
   }
#endif

   if (d3d->should_resize)
      xdk_d3d_set_viewport(false);

   d3d->frame_count++;
#ifdef _XBOX360
   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
         0xff000000, 1.0f, 0);
#endif

   d3d->d3d_render_device->SetTexture(0, d3d->lpTexture);

#ifdef HAVE_HLSL
   hlsl_use(1);
#endif

#ifdef HAVE_FBO
   if(d3d->fbo_inited)
   {
#ifdef HAVE_HLSL
      hlsl_set_params(width, height, d3d->tex_w, d3d->tex_h, g_settings.video.fbo.scale_x * width,
            g_settings.video.fbo.scale_y * height, d3d->frame_count);
#endif
      D3DVIEWPORT vp = {0};
      vp.Width  = g_settings.video.fbo.scale_x * width;
      vp.Height = g_settings.video.fbo.scale_y * height;
      vp.X      = 0;
      vp.Y      = 0;
      vp.MinZ   = 0.0f;
      vp.MaxZ   = 1.0f;
      d3d->d3d_render_device->SetViewport(&vp);
   }
   else
#endif
   {
#ifdef HAVE_HLSL
      hlsl_set_params(width, height, d3d->tex_w, d3d->tex_h, d3d->win_width,
            d3d->win_height, d3d->frame_count);
#endif
   }

   D3DLOCKED_RECT d3dlr;
   d3d->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   size_t size_screen = g_settings.video.color_format ? sizeof(uint32_t) : sizeof(uint16_t);
   for (unsigned y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch;
      memcpy(out, in, width * size_screen);
   }
   d3d->lpTexture->UnlockRect(0);

   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);

#if defined(_XBOX1)
   d3d->d3d_render_device->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX1);

   D3DXMATRIX p_out, p_rotate;
   D3DXMatrixIdentity(&p_out);
   D3DXMatrixRotationZ(&p_rotate, angle);

   d3d->d3d_render_device->SetTransform(D3DTS_WORLD, &p_rotate);
   d3d->d3d_render_device->SetTransform(D3DTS_VIEW, &p_out);
   d3d->d3d_render_device->SetTransform(D3DTS_PROJECTION, &p_out);

   d3d->d3d_render_device->SetStreamSource(0, d3d->vertex_buf, sizeof(DrawVerticeFormats));
   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);

   d3d->d3d_render_device->BeginScene();
   d3d->d3d_render_device->SetFlickerFilter(flicker_filter);
   d3d->d3d_render_device->SetSoftDisplayFilter(soft_filter_enable);
   d3d->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   d3d->d3d_render_device->EndScene();
#elif defined(_XBOX360)

   d3d->d3d_render_device->SetVertexDeclaration(d3d->v_decl);
   d3d->d3d_render_device->SetStreamSource(0, d3d->vertex_buf,
	   0,
	   sizeof(DrawVerticeFormats));

   d3d->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
#endif

#ifdef HAVE_FBO
   if(d3d->fbo_inited)
   {
      d3d->d3d_render_device->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, d3d->lpTexture_ot,
         NULL, 0, 0, NULL, 0, 0, NULL);

      d3d->d3d_render_device->SetRenderTarget(0, pRenderTarget0);
      pRenderTarget0->Release();
      d3d->d3d_render_device->SetTexture(0, &d3d->lpTexture_ot_as16srgb);

#ifdef HAVE_HLSL
      hlsl_use(2);
      hlsl_set_params(g_settings.video.fbo.scale_x * width, g_settings.video.fbo.scale_y * height, g_settings.video.fbo.scale_x * d3d->tex_w, g_settings.video.fbo.scale_y * d3d->tex_h, d3d->win_width,
            d3d->win_height, d3d->frame_count);
#endif
      xdk_d3d_set_viewport(false);

      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.second_pass_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.second_pass_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      d3d->d3d_render_device->SetVertexDeclaration(d3d->v_decl);
      d3d->d3d_render_device->SetStreamSource(0, d3d->vertex_buf,
		  0,
		  sizeof(DrawVerticeFormats));
      d3d->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   }
#endif

#if defined(_XBOX1)
   if(fps_enable)
   {
      MEMORYSTATUS stat;
      GlobalMemoryStatus(&stat);

      //Output memory usage

      char fps_txt[128];
      char buf[128];
      bool ret = false;
      snprintf(buf, sizeof(buf), "%.2f MB free / %.2f MB total", stat.dwAvailPhys/(1024.0f*1024.0f), stat.dwTotalPhys/(1024.0f*1024.0f));
      xfonts_render_msg_place(d3d, font_x + 30, font_y + 50, 0 /* scale */, buf);

      gfx_fps_title(fps_txt, sizeof(fps_txt));
      xfonts_render_msg_place(d3d, font_x + 30, font_y + 70, 0 /* scale */, fps_txt);
   }

   if (msg)
      xfonts_render_msg_place(d3d, 60, 365, 0, msg); //TODO: dehardcode x/y here for HD (720p) mode
#elif defined(_XBOX360)
   if (msg && !menu_enabled)
   {
	   xdk_render_msg(d3d, msg);
   }
#endif

   if(!d3d->block_swap)
      gfx_ctx_xdk_swap_buffers();

   return true;
}

static void xdk_d3d_set_nonblock_state(void *data, bool state)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   if(d3d->vsync)
   {
      RARCH_LOG("D3D Vsync => %s\n", state ? "off" : "on");
      gfx_ctx_xdk_set_swap_interval(state ? 0 : 1);
   }
}

static bool xdk_d3d_alive(void *data)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
   check_window(d3d);
   return !d3d->quitting;
}

static bool xdk_d3d_focus(void *data)
{
   (void)data;
   return gfx_ctx_window_has_focus();
}

static void xdk_d3d_start(void)
{
   video_info_t video_info = {0};

   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;
   video_info.fullscreen = true;
   if(g_settings.video.force_16bit)
	   video_info.rgb32 = false;

   driver.video_data = xdk_d3d_init(&video_info, NULL, NULL);

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

#if defined(_XBOX1)
   /* load debug fonts */
   XFONT_OpenDefaultFont(&d3d->debug_font);
   d3d->debug_font->SetBkMode(XFONT_TRANSPARENT);
   d3d->debug_font->SetBkColor(D3DCOLOR_ARGB(100,0,0,0));
   d3d->debug_font->SetTextHeight(14);
   d3d->debug_font->SetTextAntialiasLevel(d3d->debug_font->GetTextAntialiasLevel());

   font_x = 0;
   font_y = 0;
#elif defined(_XBOX360)
   HRESULT hr = d3d9_init_font("game:\\media\\Arial_12.xpr");

   if(hr < 0)
      RARCH_ERR("Couldn't initialize HLSL shader fonts.\n");
#endif
}

static void xdk_d3d_restart(void)
{
}

static void xdk_d3d_stop(void)
{
   void *data = driver.video_data;

   xdk_d3d_free(data);

   driver.video_data = NULL;
}

static void xdk_d3d_apply_state_changes(void)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
   d3d->should_resize = true;
}

static void xdk_d3d_set_aspect_ratio(void *data, unsigned aspectratio_index)
{
   (void)data;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   if(g_settings.video.aspect_ratio_idx == ASPECT_RATIO_AUTO)
      rarch_set_auto_viewport(g_extern.frame_cache.width, g_extern.frame_cache.height);
   else if(g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CORE)
      rarch_set_core_viewport();

   g_settings.video.aspect_ratio = aspectratio_lut[g_settings.video.aspect_ratio_idx].value;
   g_settings.video.force_aspect = false;
   d3d->should_resize = true;
}

const video_driver_t video_xdk_d3d = {
   xdk_d3d_init,
   xdk_d3d_frame,
   xdk_d3d_set_nonblock_state,
   xdk_d3d_alive,
   xdk_d3d_focus,
   NULL,
   xdk_d3d_free,
   "xdk_d3d",
   xdk_d3d_start,
   xdk_d3d_stop,
   xdk_d3d_restart,
   xdk_d3d_apply_state_changes,
   xdk_d3d_set_aspect_ratio,
   xdk_d3d_set_rotation,
};
