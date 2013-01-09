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

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include "../driver.h"
#include "xdk_d3d.h"

#ifdef HAVE_HLSL
#include "../gfx/shader_hlsl.h"
#endif

#include "./../gfx/gfx_context.h"
#include "../general.h"
#include "../message.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../xdk/xdk_resources.h"

#if defined(_XBOX1)
unsigned font_x, font_y;
#elif defined(_XBOX360)
#include "../frontend/menu/rmenu_xui.h"
extern CRetroArch app;
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

static void check_window(void *data)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   bool quit, resize;

   d3d->ctx_driver->check_window(&quit,
         &resize, NULL, NULL, g_extern.frame_count);

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

static void xdk_d3d_free(void *data)
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
   d3d->font_ctx->deinit(d3d);

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
   unsigned width, height;      // Set the viewport based on the current resolution
   int m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp;
   float m_zNear, m_zFar;

   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);

   d3d->ctx_driver->get_video_size(&width, &height);
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

static void xdk_d3d_set_rotation(void *data, unsigned orientation)
{
   (void)data;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
   FLOAT angle = 0;

   switch(orientation)
   {
      case ORIENTATION_NORMAL:
         angle = M_PI * 0 / 180;
         break;
      case ORIENTATION_FLIPPED_ROTATED:
         angle = M_PI * 90 / 180;
         break;
      case ORIENTATION_FLIPPED:
         angle = M_PI * 180 / 180;
         break;
      case ORIENTATION_VERTICAL:
         angle = M_PI * 270 / 180;
         break;
   }

#if defined(HAVE_HLSL)
   /* TODO: Move to D3DXMATRIX here */
   hlsl_set_proj_matrix(XMMatrixRotationZ(angle));
#elif defined(_XBOX1)
   D3DXMATRIX p_out, p_rotate;
   D3DXMatrixIdentity(&p_out);
   D3DXMatrixRotationZ(&p_rotate, angle);

   d3d->d3d_render_device->SetTransform(D3DTS_WORLD, &p_rotate);
   d3d->d3d_render_device->SetTransform(D3DTS_VIEW, &p_out);
   d3d->d3d_render_device->SetTransform(D3DTS_PROJECTION, &p_out);
#endif
}

#ifdef HAVE_FBO
void xdk_d3d_deinit_fbo(void *data)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   if (d3d->fbo_inited)
   {
      RARCH_LOG("[xdk_d3d_deinit_fbo::] Deiniting FBO.\n");
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

      d3d->fbo_inited = false;
   }
}

void xdk_d3d_init_fbo(void *data)
{
   HRESULT ret;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   if(!g_settings.video.render_to_texture)
      return;

   xdk_d3d_deinit_fbo(d3d);

   ret = d3d->d3d_render_device->CreateTexture(d3d->tex_w * g_settings.video.fbo.scale_x, d3d->tex_h * g_settings.video.fbo.scale_y,
         1, 0, g_extern.console.screen.gamma_correction ? ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 ) : D3DFMT_X8R8G8B8,
         0, &d3d->lpTexture_ot, NULL);

   if (ret != S_OK)
   {
      RARCH_ERR("[xdk_d3d_init_fbo::] Failed at CreateTexture.\n");
      return;
   }

   ret = d3d->d3d_render_device->CreateRenderTarget(d3d->tex_w * g_settings.video.fbo.scale_x, d3d->tex_h * g_settings.video.fbo.scale_y,
         g_extern.console.screen.gamma_correction ? ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 ) : D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 
         0, 0, &d3d->lpSurface, NULL);

   if (ret != S_OK)
   {
      RARCH_ERR("[xdk_d3d_init_fbo::] Failed at CreateRenderTarget.\n");
      return;
   }

   d3d->lpTexture_ot_as16srgb = *d3d->lpTexture_ot;
   xdk_convert_texture_to_as16_srgb(d3d->lpTexture);
   xdk_convert_texture_to_as16_srgb(&d3d->lpTexture_ot_as16srgb);

   d3d->fbo_inited = true;
}
#endif

static bool xdk_d3d_set_shader(void *data, enum rarch_shader_type type, const char *path, unsigned index)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   switch (type)
   {
#if defined(HAVE_HLSL)
      case RARCH_SHADER_HLSL:
         if (index == RARCH_SHADER_INDEX_MULTIPASS)
         {
            if (!hlsl_init(path, d3d->d3d_render_device))
               return false;
         }
         else
         {
            if (!hlsl_load_shader(index, path))
               return false;
         }
         break;
#endif
      case RARCH_SHADER_NONE:
      default:
         RARCH_ERR("Invalid shader type in gl_set_shader().\n");
         return false;
   }

   return true;
}

void xdk_d3d_generate_pp(D3DPRESENT_PARAMETERS *d3dpp, const video_info_t *video)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   memset(d3dpp, 0, sizeof(*d3dpp));

   d3d->texture_fmt = video->rgb32 ? D3DFMT_X8R8G8B8 : D3DFMT_LIN_R5G6B5;
   d3d->base_size   = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   unsigned width, height;
   d3d->ctx_driver->get_video_size(&width, &height);

   d3dpp->BackBufferWidth  = d3d->win_width = width;
   d3dpp->BackBufferHeight = d3d->win_height = height;

#if defined(_XBOX1)
   // Get the "video mode"
   DWORD video_mode = XGetVideoFlags();

   // Check if we are able to use progressive mode
   if(video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
   else
      d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;

   // Only valid in PAL mode, not valid for HDTV modes!
   if(XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      if(video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
         d3dpp->FullScreen_RefreshRateInHz = 60;
      else
         d3dpp->FullScreen_RefreshRateInHz = 50;
   }

   if(XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if(video_mode & XC_VIDEO_FLAGS_HDTV_480p)
         d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
      else if(video_mode & XC_VIDEO_FLAGS_HDTV_720p)
         d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
      else if(video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
         d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;
   }

   if(g_extern.console.rmenu.state.rmenu_widescreen.enable)
      d3dpp->Flags |= D3DPRESENTFLAG_WIDESCREEN;

   d3dpp->BackBufferFormat                     = D3DFMT_X8R8G8B8;
   d3dpp->FullScreen_PresentationInterval	   = d3d->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
   d3dpp->SwapEffect                           = D3DSWAPEFFECT_COPY;
#elif defined(_XBOX360)
   if(!g_extern.console.rmenu.state.rmenu_widescreen.enable)
      d3dpp->Flags |= D3DPRESENTFLAG_NO_LETTERBOX;

   if(g_extern.console.screen.gamma_correction)
   {
      d3dpp->BackBufferFormat        = (D3DFORMAT)MAKESRGBFMT(d3d->texture_fmt);
      d3dpp->FrontBufferFormat       = (D3DFORMAT)MAKESRGBFMT(D3DFMT_LE_X8R8G8B8);
   }
   else
   {
      d3dpp->BackBufferFormat        = d3d->texture_fmt;
      d3dpp->FrontBufferFormat       = D3DFMT_LE_X8R8G8B8;
   }
   d3dpp->MultiSampleQuality      = 0;
   d3dpp->PresentationInterval    = d3d->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
   d3dpp->SwapEffect              = D3DSWAPEFFECT_DISCARD;
#endif
   d3dpp->BackBufferCount         = 2;
   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;
}

static void xdk_d3d_init_textures(void *data, const video_info_t *video)
{
   HRESULT ret;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   D3DPRESENT_PARAMETERS d3dpp;
   D3DVIEWPORT vp = {0};
   xdk_d3d_generate_pp(&d3dpp, video);

   d3d->texture_fmt = video->rgb32 ? D3DFMT_LIN_X8R8G8B8 : D3DFMT_LIN_R5G6B5;
   d3d->base_size   = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   if (d3d->lpTexture)
   {
      d3d->lpTexture->Release();
      d3d->lpTexture = NULL;
   }

   ret = d3d->d3d_render_device->CreateTexture(d3d->tex_w, d3d->tex_h, 1, 0, d3d->texture_fmt,
         0, &d3d->lpTexture
#ifdef _XBOX360
         , NULL
#endif
         );

   if (ret != S_OK)
   {
      RARCH_ERR("[xdk_d3d_init_textures::] failed at CreateTexture.\n");
      return;
   }

   D3DLOCKED_RECT d3dlr;
   d3d->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   memset(d3dlr.pBits, 0, d3d->tex_w * d3dlr.Pitch);
   d3d->lpTexture->UnlockRect(0);

   d3d->last_width = d3d->tex_w;
   d3d->last_height = d3d->tex_h;

#if defined(_XBOX1)
   d3d->d3d_render_device->SetRenderState(D3DRS_LIGHTING, FALSE);
#elif defined(_XBOX360)
   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
         0xff000000, 1.0f, 0);
#endif
   vp.Width  = d3d->win_width;
   vp.Height = d3d->win_height;

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

static void xdk_d3d_reinit_textures(void *data, const video_info_t *video)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   unsigned old_base_size = d3d->base_size;
   unsigned old_width     = d3d->tex_w;
   unsigned old_height    = d3d->tex_h;
   d3d->texture_fmt = video->rgb32 ? D3DFMT_LIN_X8R8G8B8 : D3DFMT_LIN_R5G6B5;
   d3d->base_size   = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   //FIXME - temporary hack
   d3d->tex_w = d3d->tex_h = 512;

   if (old_base_size != d3d->base_size || old_width != d3d->tex_w || old_height != d3d->tex_h)
   {
      RARCH_LOG("Reinitializing textures (%u x %u @ %u bpp)\n", d3d->tex_w,
            d3d->tex_h, d3d->base_size * CHAR_BIT);

      xdk_d3d_init_textures(d3d, video);

#ifdef HAVE_FBO
      if (d3d->tex_w > old_width || d3d->tex_h > old_height)
      {
         RARCH_LOG("Reiniting FBO.\n");
         xdk_d3d_init_fbo(d3d);
      }
#endif
   }
   else
      RARCH_LOG("Reinitializing textures skipped.\n");
}

static void *xdk_d3d_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   HRESULT ret;

   if (driver.video_data)
   {
      xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;
      // Reinitialize textures as we might have changed pixel formats.
      xdk_d3d_reinit_textures(d3d, video);
      return driver.video_data;
   }

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
   if (d3d->ctx_driver)
   {
      D3DPRESENT_PARAMETERS d3dpp;
      xdk_d3d_generate_pp(&d3dpp, video);

      ret = d3d->d3d_device->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &d3dpp, &d3d->d3d_render_device);

      if (ret != S_OK)
         RARCH_ERR("Failed at CreateDevice.\n");
      d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
   }
   else
   {
      free(d3d);
      return NULL;
   }

   RARCH_LOG("Found D3D context: %s\n", d3d->ctx_driver->ident);

   xdk_d3d_init_textures(d3d, video);

#if defined(_XBOX1)
   // use an orthogonal matrix for the projection matrix
   D3DXMATRIX mat;
   D3DXMatrixOrthoOffCenterLH(&mat, 0,  d3d->win_width ,  d3d->win_height , 0, 0.0f, 1.0f);

   d3d->d3d_render_device->SetTransform(D3DTS_PROJECTION, &mat);

   // use an identity matrix for the world and view matrices
   D3DXMatrixIdentity(&mat);
   d3d->d3d_render_device->SetTransform(D3DTS_WORLD, &mat);
   d3d->d3d_render_device->SetTransform(D3DTS_VIEW, &mat);

   ret = d3d->d3d_render_device->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats), 
         D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &d3d->vertex_buf);

   if (ret != S_OK)
   {
      RARCH_ERR("[xdk_d3d_init::] Failed at CreateVertexBuffer.\n");
      return NULL;
   }

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
   ret = d3d->d3d_render_device->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats), 
         0, 0, 0, &d3d->vertex_buf, NULL);

   if (ret != S_OK)
   {
      RARCH_ERR("[xdk_d3d_init::] Failed at CreateVertexBuffer.\n");
      return NULL;
   }

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

   ret = d3d->d3d_render_device->CreateVertexDeclaration(VertexElements, &d3d->v_decl);

   if (ret != S_OK)
   {
      RARCH_ERR("[xdk_d3d_init::] Failed at CreateVertexDeclaration.\n");
   }
#endif

   d3d->ctx_driver->get_video_size(&d3d->win_width, &d3d->win_height);
   RARCH_LOG("Detecting screen resolution: %ux%u.\n", d3d->win_width, d3d->win_height);

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
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
#ifdef HAVE_FBO
   D3DSurface* pRenderTarget0;
#endif
   bool fps_enable = g_extern.console.rmenu.state.msg_fps.enable;
#ifdef _XBOX1
   unsigned flicker_filter = g_extern.console.screen.state.flicker_filter.value;
   bool soft_filter_enable = g_extern.console.screen.state.soft_filter.enable;
#endif

   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);

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
      float tex_w = width / ((float)d3d->tex_w);
      float tex_h = height / ((float)d3d->tex_h);

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
         verts[i].x -= 0.5f / ((float)d3d->tex_w);
         verts[i].y += 0.5f / ((float)d3d->tex_h);
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

   d3d->d3d_render_device->SetTexture(0, d3d->lpTexture);

#ifdef HAVE_HLSL
   hlsl_use(1);
#endif

#ifdef HAVE_FBO
   if(d3d->fbo_inited)
   {
#ifdef HAVE_HLSL
      hlsl_set_params(width, height, d3d->tex_w, d3d->tex_h, g_settings.video.fbo.scale_x * width,
            g_settings.video.fbo.scale_y * height, g_extern.frame_count);
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
            d3d->win_height, g_extern.frame_count);
#endif
   }

   if(frame)
   {
      D3DLOCKED_RECT d3dlr;
      d3d->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);

      for (unsigned y = 0; y < height; y++)
      {
         const uint8_t *in = (const uint8_t*)frame + y * pitch;
         uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch;
         memcpy(out, in, width * d3d->base_size);
      }
      d3d->lpTexture->UnlockRect(0);
   }

   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);

#if defined(_XBOX1)
   d3d->d3d_render_device->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX1);

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
            d3d->win_height, g_extern.frame_count);
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
   float mem_width  = font_x + 30;
   float mem_height = font_y + 50;
   float msg_width  = 60;
   float msg_height = 365;
#elif defined(_XBOX360)
   float mem_width  = g_extern.console.rmenu.state.rmenu_hd.enable ? 160 : 100;
   float mem_height = 70;
   float msg_width  = mem_width;
   float msg_height = mem_height + 50;
#endif

   if(fps_enable)
   {
      MEMORYSTATUS stat;
      GlobalMemoryStatus(&stat);

      char fps_txt[128];
      char buf[128];

      snprintf(buf, sizeof(buf), "%.2f MB free / %.2f MB total", stat.dwAvailPhys/(1024.0f*1024.0f), stat.dwTotalPhys/(1024.0f*1024.0f));
      if (d3d->font_ctx)
         d3d->font_ctx->render_msg_place(d3d, mem_width, mem_height, 0, 0, buf);

      gfx_get_fps(fps_txt, sizeof(fps_txt), true);
      if (d3d->font_ctx)
         d3d->font_ctx->render_msg_place(d3d, mem_width, mem_height + 30, 0, 0, fps_txt);
   }

   if (msg)
      d3d->font_ctx->render_msg_place(d3d, msg_width, msg_height, 0.0f, 0, msg);

   if (g_extern.draw_menu)
   {
#ifdef _XBOX360
      app.Render();
      XuiTimersRun();
#endif
   }
   else
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
   video_info.rgb32 = false;

   driver.video_data = xdk_d3d_init(&video_info, NULL, NULL);

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

#if defined(_XBOX1)
   font_x = 0;
   font_y = 0;
#elif defined(_XBOX360)
   snprintf(g_settings.video.font_path, sizeof(g_settings.video.font_path), "game:\\media\\Arial_12.xpr");
#endif
   d3d->font_ctx = d3d_font_init_first(d3d, g_settings.video.font_path, g_settings.video.font_size);
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
   xdk_d3d_set_shader,
   xdk_d3d_free,
   "xdk_d3d",
   xdk_d3d_start,
   xdk_d3d_stop,
   xdk_d3d_restart,
   xdk_d3d_apply_state_changes,
   xdk_d3d_set_aspect_ratio,
   xdk_d3d_set_rotation,
};
