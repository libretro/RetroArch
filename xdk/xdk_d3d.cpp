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
#include "../message_queue.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../xdk/xdk_resources.h"

static bool d3d_init_shader(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   const gl_shader_backend_t *backend = NULL;

   const char *shader_path = g_settings.video.shader_path;
   enum rarch_shader_type type = gfx_shader_parse_type(shader_path, DEFAULT_SHADER_TYPE);
   
   switch (type)
   {
      case RARCH_SHADER_HLSL:
#ifdef HAVE_HLSL
         RARCH_LOG("[D3D]: Using HLSL shader backend.\n");
         backend = &hlsl_backend;
#endif
         break;
   }

   if (!backend)
   {
      RARCH_ERR("[GL]: Didn't find valid shader backend. Continuing without shaders.\n");
      return true;
   }


   d3d->shader = backend;
   return d3d->shader->init(d3d, shader_path);
}

static void d3d_free(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (!d3d)
      return;

   if (d3d->font_ctx && d3d->font_ctx->deinit)
      d3d->font_ctx->deinit(d3d);
   d3d->font_ctx = NULL;

   if (d3d->shader && d3d->shader->deinit)
      d3d->shader->deinit();
   d3d->shader = NULL;

   if (d3d->ctx_driver && d3d->ctx_driver->destroy)
      d3d->ctx_driver->destroy(d3d);
   d3d->ctx_driver = NULL;

   if (d3d->g_pD3D)
      d3d->g_pD3D->Release();
   d3d->g_pD3D = NULL;

   free(d3d);
}

static void d3d_set_viewport(void *data, int x, int y, unsigned width, unsigned height)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   D3DVIEWPORT viewport;

   // D3D doesn't support negative X/Y viewports ...
   if (x < 0)
      x = 0;
   if (y < 0)
      y = 0;

   viewport.Width  = width;
   viewport.Height = height;
   viewport.X      = x;
   viewport.Y      = y;
   viewport.MinZ   = 0.0f;
   viewport.MaxZ   = 1.0f;
   d3d->final_viewport = viewport;
}

static void d3d_calculate_rect(void *data, unsigned width, unsigned height,
   bool keep, float desired_aspect)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = d3d->dev;

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
         const rarch_viewport_t &custom = g_extern.console.screen.viewports.custom_vp;
         d3d_set_viewport(d3d, custom.x, custom.y, custom.width, custom.height);
      }
      else
      {
         float device_aspect = static_cast<float>(width) / static_cast<float>(height);
         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
            d3d_set_viewport(d3d, 0, 0, width, height);
         else if (device_aspect > desired_aspect)
         {
            float delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            d3d_set_viewport(d3d, int(roundf(width * (0.5f - delta))), 0, unsigned(roundf(2.0f * width * delta)), height);
         }
         else
         {
            float delta = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            d3d_set_viewport(d3d, 0, int(roundf(height * (0.5f - delta))), width, unsigned(roundf(2.0f * height * delta)));
         }
      }
   }
}

static void d3d_set_rotation(void *data, unsigned rot)
{
   (void)data;
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->dev_rotation = rot;
}

static void set_mvp(void *data, unsigned vp_width, unsigned vp_height, unsigned rotation)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;
#if defined(_XBOX360) && defined(HAVE_HLSL)
   hlsl_set_proj_matrix(XMMatrixRotationZ(rotation * (M_PI / 2.0)));
   if (d3d->shader && d3d->shader->set_mvp)
      d3d->shader->set_mvp(d3d, NULL);
#elif defined(_XBOX1)
   D3DXMATRIX p_out, p_rotate, mat;
   D3DXMatrixOrthoOffCenterLH(&mat, 0, vp_width,  vp_height, 0, 0.0f, 1.0f);
   D3DXMatrixIdentity(&p_out);
   D3DXMatrixRotationZ(&p_rotate, rotation * (M_PI / 2.0));

   RD3DDevice_SetTransform(d3dr, D3DTS_WORLD, &p_rotate);
   RD3DDevice_SetTransform(d3dr, D3DTS_VIEW, &p_out);
   RD3DDevice_SetTransform(d3dr, D3DTS_PROJECTION, &p_out);
#endif
}

static bool d3d_set_shader(void *data, enum rarch_shader_type type, const char *path)
{
   /* TODO - stub */
   d3d_video_t *d3d = (d3d_video_t*)data;

   switch (type)
   {
      case RARCH_SHADER_CG:
#ifdef HAVE_HLSL
         d3d->shader = &hlsl_backend;
         break;
#endif
      default:
         d3d->shader = NULL;
         break;
   }

   if (!d3d->shader)
   {
      RARCH_ERR("[D3D]: Cannot find shader core for path: %s.\n", path);
      return false;
   }

   return true;
}

static void d3d_init_textures(void *data, const video_info_t *video)
{
   HRESULT ret;
   d3d_video_t *d3d = (d3d_video_t*)data;

   D3DPRESENT_PARAMETERS d3dpp;
   D3DVIEWPORT vp = {0};
   d3d_make_d3dpp(d3d, video, &d3dpp);

   d3d->texture_fmt = video->rgb32 ? D3DFMT_LIN_X8R8G8B8 : D3DFMT_LIN_R5G6B5;
   d3d->base_size   = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   if (d3d->lpTexture)
   {
      d3d->lpTexture->Release();
      d3d->lpTexture = NULL;
   }

   ret = d3d->dev->CreateTexture(d3d->tex_w, d3d->tex_h, 1, 0, d3d->texture_fmt,
         0, &d3d->lpTexture
#ifdef _XBOX360
         , NULL
#endif
         );

   if (ret != S_OK)
   {
      RARCH_ERR("[d3d_init_textures::] failed at CreateTexture.\n");
      return;
   }

   D3DLOCKED_RECT d3dlr;
   D3DTexture_LockRect(d3d->lpTexture, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   memset(d3dlr.pBits, 0, d3d->tex_w * d3dlr.Pitch);

   d3d->last_width = d3d->tex_w;
   d3d->last_height = d3d->tex_h;

#ifdef _XBOX1
   d3d->dev->SetRenderState(D3DRS_LIGHTING, FALSE);
#endif

   vp.Width  = d3d->screen_width;
   vp.Height = d3d->screen_height;

   d3d->dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
   d3d->dev->SetRenderState(D3DRS_ZENABLE, FALSE);

   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;
   RD3DDevice_SetViewport(d3d->dev, &vp);

   if (g_extern.console.screen.viewports.custom_vp.width == 0)
      g_extern.console.screen.viewports.custom_vp.width = vp.Width;

   if (g_extern.console.screen.viewports.custom_vp.height == 0)
      g_extern.console.screen.viewports.custom_vp.height = vp.Height;
}

static const gfx_ctx_driver_t *d3d_get_context(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   enum gfx_ctx_api api;
   unsigned major, minor;
#if defined(_XBOX1)
   api = GFX_CTX_DIRECT3D8_API;
   major = 8;
#elif defined(_XBOX360)
   api = GFX_CTX_DIRECT3D9_API;
   major = 9;
#endif
   minor = 0;
   return gfx_ctx_init_first(d3d, api, major, minor, false);
}

static bool d3d_init_base(void *data, const video_info_t *info)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   D3DPRESENT_PARAMETERS d3dpp;
   d3d_make_d3dpp(d3d, info, &d3dpp);

   d3d->g_pD3D = direct3d_create_ctx(D3D_SDK_VERSION);
   if (!d3d->g_pD3D)
   {
      RARCH_ERR("Failed to create D3D interface.\n");
      return false;
   }

   if ((d3d->d3d_err = d3d->g_pD3D->CreateDevice(0,
            D3DDEVTYPE_HAL,
            NULL,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &d3dpp,
            &d3d->dev)) != S_OK)
   {
      RARCH_ERR("[D3D]: Failed to initialize device.\n");
      return false;
   }


   return true;
}

static bool d3d_init_chain(void *data, const video_info_t *info)
{
   HRESULT ret;
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = d3d->dev;

   d3d_init_textures(d3d, info);

   ret = d3d->dev->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats), 
         D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &d3d->vertex_buf
#ifdef _XBOX360
         ,NULL
#endif
         );

   if (ret != S_OK)
   {
      RARCH_ERR("[d3d_init::] Failed at CreateVertexBuffer.\n");
      return false;
   }
#if defined(_XBOX1)
   const DrawVerticeFormats init_verts[] = {
      { -1.0f, -1.0f, 1.0f, 0.0f, 1.0f },
      {  1.0f, -1.0f, 1.0f, 1.0f, 1.0f },
      { -1.0f,  1.0f, 1.0f, 0.0f, 0.0f },
      {  1.0f,  1.0f, 1.0f, 1.0f, 0.0f },
   };

   BYTE *verts_ptr;
#elif defined(_XBOX360)
   static const DrawVerticeFormats init_verts[] = {
      { -1.0f, -1.0f, 0.0f, 1.0f },
      {  1.0f, -1.0f, 1.0f, 1.0f },
      { -1.0f,  1.0f, 0.0f, 0.0f },
      {  1.0f,  1.0f, 1.0f, 0.0f },
   };

   void *verts_ptr;
#endif

   d3d->vertex_buf->Lock(0, 0, &verts_ptr, 0);
   memcpy(verts_ptr, init_verts, sizeof(init_verts));
   d3d->vertex_buf->Unlock();

#if defined(_XBOX1)
   d3d->dev->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX1);
#elif defined(_XBOX360)
   static const D3DVERTEXELEMENT VertexElements[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      D3DDECL_END()
   };

   ret = d3d->dev->CreateVertexDeclaration(VertexElements, &d3d->v_decl);

   if (ret != S_OK)
   {
      RARCH_ERR("[d3d_init::] Failed at CreateVertexDeclaration.\n");
   }
#endif

   return true;
}

static bool d3d_initialize(void *data, const video_info_t *info)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   bool ret = true;

   if (!d3d->g_pD3D)
      ret = d3d_init_base(d3d, info);
   else
   {
   }

   if (!ret)
      return ret;

   d3d_calculate_rect(d3d, d3d->screen_width, d3d->screen_height, info->force_aspect,g_extern.system.aspect_ratio);

#ifdef HAVE_HLSL
   if (!d3d_init_shader(d3d))
   {
      RARCH_ERR("Failed to initialize HLSL.\n");
      return false;
   }
#endif

   if (!d3d_init_chain(d3d, info))
   {
      RARCH_ERR("Failed to initialize render chain.\n");
      return false;
   }

#if defined(_XBOX360)
   strlcpy(g_settings.video.font_path, "game:\\media\\Arial_12.xpr", sizeof(g_settings.video.font_path));
#endif
   d3d->font_ctx = d3d_font_init_first(d3d, g_settings.video.font_path, 0);
   if (!d3d->font_ctx)
   {
      RARCH_ERR("Failed to initialize font.\n");
      return false;
   }

   return true;
}

// Delay constructor due to lack of exceptions.
static bool d3d_construct(void *data, const video_info_t *info, const input_driver_t **input,
                          void **input_data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->should_resize = false;

   unsigned full_x, full_y;

   if (d3d->ctx_driver && d3d->ctx_driver->get_video_size)
      d3d->ctx_driver->get_video_size(d3d, &full_x, &full_y);

   d3d->screen_width  = info->fullscreen ? full_x : info->width;
   d3d->screen_height = info->fullscreen ? full_y : info->height;

   d3d->video_info = *info;
   if (!d3d_initialize(data, &d3d->video_info))
      return false;

   if (input && input_data)
      d3d->ctx_driver->input_driver(d3d, input, input_data);

   RARCH_LOG("[D3D]: Init complete.\n");
   return true;
}

static void *d3d_init(const video_info_t *vid, const input_driver_t **input, void **input_data)
{
   d3d_video_t *d3d = (d3d_video_t*)calloc(1, sizeof(d3d_video_t));
   if (!d3d)
      return NULL;

   d3d->ctx_driver = d3d_get_context(d3d);
   if (!d3d->ctx_driver)
   {
      free(d3d);
      return NULL;
   }

   //default values
   d3d->g_pD3D               = NULL;
   d3d->dev                  = NULL;
   d3d->dev_rotation         = 0;
   d3d->should_resize        = false;
   d3d->vsync                = vid->vsync;
   d3d->tex_w                = RARCH_SCALE_BASE * vid->input_scale;
   d3d->tex_h                = RARCH_SCALE_BASE * vid->input_scale;

   if (!d3d_construct(d3d, vid, input, input_data))
   {
      RARCH_ERR("[D3D]: Failed to init D3D.\n");
      delete d3d;
      return NULL;
   }

   return d3d;
}

#ifdef HAVE_RMENU
extern struct texture_image *menu_texture;
#endif

#ifdef _XBOX1
static bool texture_image_render(void *data, struct texture_image *out_img,
                          int x, int y, int w, int h, bool force_fullscreen)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;

   if (out_img->pixels == NULL || out_img->vertex_buf == NULL)
      return false;

   float fX = static_cast<float>(x);
   float fY = static_cast<float>(y);

   // create the new vertices
   DrawVerticeFormats newVerts[] =
   {
      // x,           y,              z,     color, u ,v
      {fX,            fY,             0.0f,  0,     0, 0},
      {fX + w,        fY,             0.0f,  0,     1, 0},
      {fX + w,        fY + h,         0.0f,  0,     1, 1},
      {fX,            fY + h,         0.0f,  0,     0, 1}
   };

   // load the existing vertices
   DrawVerticeFormats *pCurVerts;

   HRESULT ret = out_img->vertex_buf->Lock(0, 0, (unsigned char**)&pCurVerts, 0);

   if (FAILED(ret))
   {
      RARCH_ERR("Error occurred during m_pVertexBuffer->Lock().\n");
      return false;
   }

   // copy the new verts over the old verts
   memcpy(pCurVerts, newVerts, 4 * sizeof(DrawVerticeFormats));
   out_img->vertex_buf->Unlock();

   d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
   d3d->dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d->dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

   // also blend the texture with the set alpha value
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);

   // draw the quad
   d3dr->SetTexture(0, out_img->pixels);
   d3dr->SetStreamSource(0, out_img->vertex_buf, sizeof(DrawVerticeFormats));
   d3dr->SetVertexShader(D3DFVF_CUSTOMVERTEX);

   if (force_fullscreen)
   {
      D3DVIEWPORT vp = {0};
      vp.Width  = w;
      vp.Height = h;
      vp.X      = 0;
      vp.Y      = 0;
      vp.MinZ   = 0.0f;
      vp.MaxZ   = 1.0f;
      d3dr->SetViewport(&vp);
   }
   d3dr->DrawPrimitive(D3DPT_QUADLIST, 0, 1);

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

   if (d3d->rgui_texture_enable)
   {
      d3d->dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      d3d->dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
      d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
      texture_image_render(d3d, menu_texture, menu_texture->x, menu_texture->y,
         d3d->screen_width, d3d->screen_height, true);
      d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
   }
#endif
}
#endif

static void clear_texture(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = d3d->dev;
   D3DLOCKED_RECT d3dlr;

   D3DTexture_LockRect(d3d->lpTexture, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   memset(d3dlr.pBits, 0, d3d->tex_w * d3dlr.Pitch);
}

static void blit_to_texture(void *data, const void *frame,
   unsigned width, unsigned height, unsigned pitch)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d->last_width != width || d3d->last_height != height)
      clear_texture(data);

   D3DLOCKED_RECT d3dlr;
   D3DTexture_LockRect(d3d->lpTexture, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);

#if defined(_XBOX360)
   D3DSURFACE_DESC desc;
   d3d->lpTexture->GetLevelDesc(0, &desc);
      XGCopySurface(d3dlr.pBits, d3dlr.Pitch, width, height, desc.Format, NULL, frame,
                                        pitch, desc.Format, NULL, 0, 0);
#elif defined(_XBOX1)
   for (unsigned y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch;
      memcpy(out, in, width * d3d->base_size);
   }
#endif
}

static void set_vertices(void *data, unsigned pass, unsigned width, unsigned height)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d->last_width != width || d3d->last_height != height)
   {
#if defined(_XBOX1)
      float tex_w = width;
      float tex_h = height;

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
      RD3DVertexBuffer_Lock(d3d->vertex_buf, 0, 0, &verts_ptr, 0);
      memcpy(verts_ptr, verts, sizeof(verts));
      RD3DVertexBuffer_Unlock(d3d->vertex_buf);

      d3d->last_width = width;
      d3d->last_height = height;
   }

   if (d3d->shader)
   {
      set_mvp(d3d, d3d->screen_width, d3d->screen_height, d3d->dev_rotation);
      if (d3d->shader->use)
         d3d->shader->use(d3d, pass);
      if (d3d->shader->set_params)
         d3d->shader->set_params(d3d, width, height, d3d->tex_w, d3d->tex_h, d3d->screen_width,
               d3d->screen_height, g_extern.frame_count,
               NULL, NULL, NULL, 0);
   }
}

static void render_pass(void *data, const void *frame, unsigned width, unsigned height,
                        unsigned pitch, unsigned rotation)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;
#ifndef _XBOX1
   DWORD fetchConstant;
   UINT64 pendingMask3;
#endif
#ifdef _XBOX1
   d3dr->SetFlickerFilter(g_extern.console.screen.flicker_filter_index);
   d3dr->SetSoftDisplayFilter(g_extern.lifecycle_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE));
#endif
   blit_to_texture(d3d, frame, width, height, pitch);
   set_vertices(d3d, 1, width, height);

   if (g_extern.frame_count)
#ifdef _XBOX1
      d3dr->SwitchTexture(0, d3d->lpTexture);
#elif defined _XBOX360
      d3dr->SetTextureFetchConstant(0, d3d->lpTexture);
#endif
   else if (d3d->lpTexture) 
      RD3DDevice_SetTexture(d3dr, 0, d3d->lpTexture);
   RD3DDevice_SetViewport(d3d->dev, &d3d->final_viewport);
   RD3DDevice_SetSamplerState_MinFilter(d3dr, 0, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   RD3DDevice_SetSamplerState_MagFilter(d3dr, 0, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   RD3DDevice_SetSamplerState_AddressU(d3dr, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   RD3DDevice_SetSamplerState_AddressV(d3dr, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);

#if defined(_XBOX1)
   RD3DDevice_SetVertexShader(d3dr, D3DFVF_XYZ | D3DFVF_TEX1);
   IDirect3DDevice8_SetStreamSource(d3dr, 0, d3d->vertex_buf, sizeof(DrawVerticeFormats));
#elif defined(_XBOX360)
   D3DDevice_SetVertexDeclaration(d3dr, d3d->v_decl);
   D3DDevice_SetStreamSource_Inline(d3dr, 0, d3d->vertex_buf, 0, sizeof(DrawVerticeFormats));
#endif
   d3dr->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

   g_extern.frame_count++;

   set_mvp(d3d, d3d->screen_width, d3d->screen_height, d3d->dev_rotation);
}

static bool d3d_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   LPDIRECT3DDEVICE d3dr;
   D3DVIEWPORT screen_vp;
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (!d3d || !frame)
      return true;

   if (d3d && d3d->should_resize)
   {
      d3d_calculate_rect(d3d, d3d->screen_width, d3d->screen_height, d3d->video_info.force_aspect, g_extern.system.aspect_ratio);
      d3d->should_resize = false;
   }

   // render_chain() only clears out viewport, clear out everything
   screen_vp.X = 0;
   screen_vp.Y = 0;
   screen_vp.MinZ = 0;
   screen_vp.MaxZ = 1;
   if (d3d)
   {
      screen_vp.Width = d3d->screen_width;
      screen_vp.Height = d3d->screen_height;
   }

   d3dr = (LPDIRECT3DDEVICE)d3d->dev;

   if (d3dr)
   {
      d3dr->SetViewport(&screen_vp);
      d3dr->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);

      // Insert black frame first, so we can screenshot, etc.
      if (g_settings.video.black_frame_insertion)
      {
         d3dr->Present(NULL, NULL, NULL, NULL);
         d3dr->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);
      }
   }

   render_pass(d3d, frame, width, height, pitch, d3d->dev_rotation);

#ifdef HAVE_MENU
   if (g_extern.lifecycle_state & (1ULL << MODE_MENU) && driver.menu_ctx && driver.menu_ctx->frame)
      driver.menu_ctx->frame(d3d);

   if (d3d && d3d->rgui_texture_enable)
      d3d_draw_texture(d3d);
#endif

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->update_window_title)
      d3d->ctx_driver->update_window_title(d3d);

   if (msg)
   {
#if defined(_XBOX1)
      float msg_width  = 60;
      float msg_height = 365;
#elif defined(_XBOX360)
      float msg_width  = (g_extern.lifecycle_state & (1ULL << MODE_MENU_HD)) ? 160 : 100;
      float msg_height = 90;
#endif
      font_params_t font_parms = {0};
      font_parms.x = msg_width;
      font_parms.y = msg_height;
      font_parms.scale = 21;
      d3d->font_ctx->render_msg(d3d, msg, &font_parms);
   }

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->swap_buffers)
      d3d->ctx_driver->swap_buffers(d3d);

   return true;
}

static void d3d_set_nonblock_state(void *data, bool state)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d)
   {
      d3d->video_info.vsync = !state;
      if (d3d->ctx_driver && d3d->ctx_driver->swap_interval)
         d3d->ctx_driver->swap_interval(d3d, state ? 0 : 1);
   }
}

static bool d3d_alive(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   bool quit, resize;

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->check_window)
      d3d->ctx_driver->check_window(d3d, &quit,
            &resize, NULL, NULL, g_extern.frame_count);

   if (quit && d3d)
      d3d->quitting = quit;
   else if (resize && d3d)
      d3d->should_resize = true;
   return !quit;
}

static bool d3d_focus(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   if (!d3d || !d3d->ctx_driver)
      return true;
   return d3d->ctx_driver->has_focus(d3d);
}

static void d3d_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (aspect_ratio_idx == ASPECT_RATIO_SQUARE)
      gfx_set_square_pixel_viewport(g_extern.system.av_info.geometry.base_width, g_extern.system.av_info.geometry.base_height);
   else if (aspect_ratio_idx == ASPECT_RATIO_CORE)
      gfx_set_core_viewport();
   else if (aspect_ratio_idx == ASPECT_RATIO_CONFIG)
      gfx_set_config_viewport();

   g_extern.system.aspect_ratio  = aspectratio_lut[aspect_ratio_idx].value;

   if (d3d)
   {
      d3d->video_info.force_aspect = true;
      d3d->should_resize = true;
   }
}

static void d3d_set_filtering(void *data, unsigned index, bool set_smooth) { }

static void d3d_apply_state_changes(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d)
      d3d->should_resize = true;
}

#ifdef HAVE_MENU
static void d3d_set_texture_frame(void *data,
   const void *frame, bool rgb32, unsigned width, unsigned height,
   float alpha)
{
   (void)frame;
   (void)rgb32;
   (void)width;
   (void)height;
   (void)alpha;
}

static void d3d_set_texture_enable(void *data, bool state, bool full_screen)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d)
   {
      d3d->rgui_texture_enable = state;
      d3d->rgui_texture_full_screen = full_screen;
   }
}
#endif

static void d3d_set_osd_msg(void *data, const char *msg, void *userdata)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   font_params_t *params = (font_params_t*)userdata;

   if (d3d && d3d->font_ctx && d3d->font_ctx->render_msg)
      d3d->font_ctx->render_msg(d3d, msg, params);
}

static const video_poke_interface_t d3d_poke_interface = {
   d3d_set_filtering,
#ifdef HAVE_FBO
   NULL,
   NULL,
#endif
   d3d_set_aspect_ratio,
   d3d_apply_state_changes,
#ifdef HAVE_MENU
   d3d_set_texture_frame,
   d3d_set_texture_enable,
#endif
   d3d_set_osd_msg,
};

static void d3d_get_poke_interface(void *data, const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &d3d_poke_interface;
}

static void d3d_restart(void)
{
   d3d_video_t *d3d = (d3d_video_t*)driver.video_data;
   LPDIRECT3DDEVICE d3dr = d3d->dev;

   if (!d3d)
      return;

   D3DPRESENT_PARAMETERS d3dpp;
   video_info_t video_info = {0};

   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;
   video_info.fullscreen = true;
   video_info.rgb32 = (d3d->base_size == sizeof(uint32_t)) ? true : false;
   d3d_make_d3dpp(d3d, &video_info, &d3dpp);

   d3dr->Reset(&d3dpp);
}

const video_driver_t video_d3d = {
   d3d_init,
   d3d_frame,
   d3d_set_nonblock_state,
   d3d_alive,
   d3d_focus,
   d3d_set_shader,
   d3d_free,
   "d3d",
   d3d_restart,
   d3d_set_rotation,
   NULL, /* viewport_info */
   NULL, /* read_viewport */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   d3d_get_poke_interface,
};
