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
#endif

#include "../driver.h"
#include "xdk_d3d.h"

#ifdef HAVE_HLSL
#include "../gfx/shader_hlsl.h"
#endif

#include "./../gfx/gfx_context.h"
#include "../console/retroarch_console.h"
#include "../general.h"
#include "../message.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _XBOX360
#include "xdk360_video_resources.h"
#endif

static void check_window(xdk_d3d_video_t *d3d)
{
   bool quit, resize;

   gfx_ctx_check_window(&quit,
         &resize, NULL, NULL,
         d3d->frame_count);

   if (quit)
      d3d->quitting = true;
   else if (resize)
      d3d->should_resize = true;
}

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
   d3d->d3d_render_device->Release();
   d3d->d3d_device->Release();

   free(d3d);
}

static void xdk_d3d_set_viewport(bool force_full)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
      0xff000000, 1.0f, 0);

   int width = d3d->video_mode.fIsHiDef ? 1280 : 640;
   int height = d3d->video_mode.fIsHiDef ? 720 : 480;
   int m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp;
   float m_zNear, m_zFar;

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
      if(g_console.aspect_ratio_index == ASPECT_RATIO_CUSTOM)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
      	 m_viewport_x_temp = g_console.viewports.custom_vp.x;
      	 m_viewport_y_temp = g_console.viewports.custom_vp.y;
      	 m_viewport_width_temp = g_console.viewports.custom_vp.width;
      	 m_viewport_height_temp = g_console.viewports.custom_vp.height;
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

   /* TODO: Move to D3DXMATRIX here */
   hlsl_set_proj_matrix(XMMatrixRotationZ(angle));

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

   d3d->d3d_render_device->CreateTexture(512 * g_settings.video.fbo_scale_x, 512 * g_settings.video.fbo_scale_y,
         1, 0, g_console.gamma_correction_enable ? ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ) : D3DFMT_A8R8G8B8,
         0, &d3d->lpTexture_ot
#ifdef _XBOX360
		 , NULL
#endif
		 );

   d3d->d3d_render_device->CreateRenderTarget(512 * g_settings.video.fbo_scale_x, 512 * g_settings.video.fbo_scale_y,
         g_console.gamma_correction_enable ? ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ) : D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 
	      0, 0, &d3d->lpSurface, NULL);

#ifdef _XBOX360
   d3d->lpTexture_ot_as16srgb = *d3d->lpTexture_ot;
   xdk360_convert_texture_to_as16_srgb(d3d->lpTexture);
   xdk360_convert_texture_to_as16_srgb(&d3d->lpTexture_ot_as16srgb);
#endif
   d3d->fbo_enabled = 1;
}
#endif

static void *xdk_d3d_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   if (driver.video_data)
      return driver.video_data;

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)calloc(1, sizeof(xdk_d3d_video_t));
   if (!d3d)
      return NULL;

   d3d->d3d_device = direct3d_create_ctx(D3D_SDK_VERSION);
   if (!d3d->d3d_device)
   {
      free(d3d);
      return NULL;
   }

   // Get video settings

   memset(&d3d->video_mode, 0, sizeof(d3d->video_mode));

   XGetVideoMode(&d3d->video_mode);

   memset(&d3d->d3dpp, 0, sizeof(d3d->d3dpp));

   // no letterboxing in 4:3 mode (if widescreen is
   // unsupported
   if(!d3d->video_mode.fIsWideScreen)
      d3d->d3dpp.Flags |= D3DPRESENTFLAG_NO_LETTERBOX;

   g_console.menus_hd_enable = d3d->video_mode.fIsHiDef;
   
   d3d->d3dpp.BackBufferWidth         = d3d->video_mode.fIsHiDef ? 1280 : 640;
   d3d->d3dpp.BackBufferHeight        = d3d->video_mode.fIsHiDef ? 720 : 480;

   if(g_console.gamma_correction_enable)
   {
      d3d->d3dpp.BackBufferFormat        = g_console.color_format ? (D3DFORMAT)MAKESRGBFMT(D3DFMT_A8R8G8B8) : (D3DFORMAT)MAKESRGBFMT(D3DFMT_LIN_A1R5G5B5);
#ifdef _XBOX360
      d3d->d3dpp.FrontBufferFormat       = (D3DFORMAT)MAKESRGBFMT(D3DFMT_LE_X8R8G8B8);
#endif
   }
   else
   {
      d3d->d3dpp.BackBufferFormat        = g_console.color_format ? D3DFMT_A8R8G8B8 : D3DFMT_LIN_A1R5G5B5;
#ifdef _XBOX360
      d3d->d3dpp.FrontBufferFormat       = D3DFMT_LE_X8R8G8B8;
#endif
   }

   d3d->d3dpp.MultiSampleType         = D3DMULTISAMPLE_NONE;
#ifdef _XBOX360
   d3d->d3dpp.MultiSampleQuality      = 0;
   d3d->d3dpp.PresentationInterval    = video->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
#else
   d3d->d3dpp.FullScreen_PresentationInterval    = video->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
#endif
   d3d->d3dpp.BackBufferCount         = 2;
   d3d->d3dpp.EnableAutoDepthStencil  = FALSE;
   d3d->d3dpp.SwapEffect              = D3DSWAPEFFECT_DISCARD;

   d3d->d3d_device->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
	   &d3d->d3dpp, &d3d->d3d_render_device);

#ifdef HAVE_HLSL
   hlsl_init(g_settings.video.cg_shader_path, d3d->d3d_render_device);
#endif

   d3d->d3d_render_device->CreateTexture(512, 512, 1, 0, D3DFMT_LIN_X1R5G5B5,
      0, &d3d->lpTexture, NULL);

#ifdef HAVE_FBO
   xdk_d3d_init_fbo(d3d);
#endif

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
   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;
   d3d->d3d_render_device->SetViewport(&vp);

   if(g_console.viewports.custom_vp.width == 0)
      g_console.viewports.custom_vp.width = vp.Width;

   if(g_console.viewports.custom_vp.height == 0)
      g_console.viewports.custom_vp.height = vp.Height;

   xdk_d3d_set_rotation(d3d, g_console.screen_orientation);

   d3d->vsync = video->vsync;

   return d3d;
}

static bool xdk_d3d_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   if (!frame)
      return true;

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
   D3DSurface* pRenderTarget0;
   bool menu_enabled = g_console.menu_enable;

   if (d3d->last_width != width || d3d->last_height != height)
   {
      D3DLOCKED_RECT d3dlr;

      d3d->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
      memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
      d3d->lpTexture->UnlockRect(0);

      float tex_w = width / 512.0f;
      float tex_h = height / 512.0f;

      DrawVerticeFormats verts[] = {
         { -1.0f, -1.0f, 0.0f,  tex_h },
         {  1.0f, -1.0f, tex_w, tex_h },
         { -1.0f,  1.0f, 0.0f,  0.0f },
         {  1.0f,  1.0f, tex_w, 0.0f },
      };

      // Align texels and vertices (D3D9 quirk).
      for (unsigned i = 0; i < 4; i++)
      {
         verts[i].x -= 0.5f / 512.0f;
         verts[i].y += 0.5f / 512.0f;
      }

      void *verts_ptr;
      d3d->vertex_buf->Lock(0, 0, &verts_ptr, 0);
      memcpy(verts_ptr, verts, sizeof(verts));
      d3d->vertex_buf->Unlock();

      d3d->last_width = width;
      d3d->last_height = height;
   }

   if (d3d->fbo_enabled)
   {
      d3d->d3d_render_device->GetRenderTarget(0, &pRenderTarget0);
      d3d->d3d_render_device->SetRenderTarget(0, d3d->lpSurface);
   }

   if (d3d->should_resize)
      xdk_d3d_set_viewport(false);

   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET,
         0xff000000, 1.0f, 0);
   d3d->frame_count++;

   d3d->d3d_render_device->SetTexture(0, d3d->lpTexture);

#ifdef HAVE_HLSL
   hlsl_use(1);
#endif

#ifdef HAVE_FBO
   if(d3d->fbo_enabled)
   {
#ifdef HAVE_HLSL
      hlsl_set_params(width, height, 512, 512, g_settings.video.fbo_scale_x * width,
            g_settings.video.fbo_scale_y * height, d3d->frame_count);
#endif
      D3DVIEWPORT vp = {0};
      vp.Width  = g_settings.video.fbo_scale_x * width;
      vp.Height = g_settings.video.fbo_scale_y * height;
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
      hlsl_set_params(width, height, 512, 512, d3d->d3dpp.BackBufferWidth,
            d3d->d3dpp.BackBufferHeight, d3d->frame_count);
#endif
   }

   D3DLOCKED_RECT d3dlr;
   d3d->lpTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   for (unsigned y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch;
      memcpy(out, in, width * sizeof(uint16_t));
   }
   d3d->lpTexture->UnlockRect(0);

   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);

   d3d->d3d_render_device->SetVertexDeclaration(d3d->v_decl);
   d3d->d3d_render_device->SetStreamSource(0, d3d->vertex_buf,
#ifdef _XBOX360
	   0,
#endif
	   sizeof(DrawVerticeFormats));

   d3d->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

#ifdef HAVE_FBO
   if(d3d->fbo_enabled)
   {
      d3d->d3d_render_device->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, d3d->lpTexture_ot,
         NULL, 0, 0, NULL, 0, 0, NULL);

      d3d->d3d_render_device->SetRenderTarget(0, pRenderTarget0);
      pRenderTarget0->Release();
      d3d->d3d_render_device->SetTexture(0, &d3d->lpTexture_ot_as16srgb);

#ifdef HAVE_HLSL
      hlsl_use(2);
      hlsl_set_params(g_settings.video.fbo_scale_x * width, g_settings.video.fbo_scale_y * height, g_settings.video.fbo_scale_x * 512, g_settings.video.fbo_scale_y * 512, d3d->d3dpp.BackBufferWidth,
            d3d->d3dpp.BackBufferHeight, d3d->frame_count);
#endif
      xdk_d3d_set_viewport(false);

      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.second_pass_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.second_pass_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      d3d->d3d_render_device->SetVertexDeclaration(d3d->v_decl);
      d3d->d3d_render_device->SetStreamSource(0, d3d->vertex_buf,
#ifdef _XBOX360
		  0,
#endif
		  sizeof(DrawVerticeFormats));
      d3d->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   }
#endif

#ifdef _XBOX360
   /* XBox 360 specific font code */
   if (msg && !menu_enabled)
   {
      if(IS_TIMER_EXPIRED(d3d))
      {
         xdk360_console_format(msg);
         SET_TIMER_EXPIRATION(d3d, 30);
      }

      xdk360_console_draw();
   }
#endif

   if(!d3d->block_swap)
      gfx_ctx_swap_buffers();

   return true;
}

static void xdk_d3d_set_nonblock_state(void *data, bool state)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   if(d3d->vsync)
   {
      RARCH_LOG("D3D Vsync => %s\n", state ? "off" : "on");
      gfx_ctx_set_swap_interval(state ? 0 : 1, TRUE);
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
   video_info.fullscreen = true;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;

   driver.video_data = xdk_d3d_init(&video_info, NULL, NULL);

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   gfx_ctx_set_swap_interval(d3d->vsync ? 1 : 0, false);

#ifdef _XBOX360
   HRESULT hr = d3d9_init_font("game:\\media\\Arial_12.xpr");

   if(hr < 0)
   {
      RARCH_ERR("Couldn't create debug console.\n");
   }
#endif
}

static void xdk_d3d_restart(void)
{
}

static void xdk_d3d_stop(void)
{
   void *data = driver.video_data;
   driver.video_data = NULL;
#ifdef _XBOX360
   d3d9_deinit_font();
#endif
   xdk_d3d_free(data);
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
   xdk_d3d_set_rotation,
};
