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

#include "render_chain_xdk.cpp"

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

#ifdef HAVE_SHADERS
void d3d_deinit_shader(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
#ifdef HAVE_CG
   if (!d3d->cgCtx)
      return;

   cgD3D9UnloadAllPrograms();
   cgD3D9SetDevice(NULL);
   cgDestroyContext(d3d->cgCtx);
   d3d->cgCtx = NULL;
#else
   if (d3d->shader && d3d->shader->deinit)
      d3d->shader->deinit();
   d3d->shader = NULL;
#endif
}
#endif

static void renderchain_clear(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d->tex)
      d3d->tex->Release();
   d3d->tex = NULL;
   if (d3d->vertex_buf)
      d3d->vertex_buf->Release();
   d3d->vertex_buf = NULL;

#ifdef _XBOX360
   if (d3d->vertex_decl)
      d3d->vertex_decl->Release();
   d3d->vertex_decl = NULL;
#endif
}

static void renderchain_free(void *data)
{
   d3d_video_t *chain = (d3d_video_t*)data;

   renderchain_clear(chain);
   //renderchain_destroy_stock_shader(chain);
#ifndef DONT_HAVE_STATE_TRACKER
   if (chain->tracker)
      state_tracker_free(chain->tracker);
#endif
}

static void d3d_deinit_chain(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

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

static void d3d_free(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d_deinitialize(d3d);
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
   Monitor::last_hm = MonitorFromWindow(d3d->hWnd, MONITOR_DEFAULTTONEAREST);
   DestroyWindow(d3d->hWnd);
#endif

   if (d3d)
      delete d3d;

#ifndef _XBOX
   UnregisterClass("RetroArch", GetModuleHandle(NULL));
#endif
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
   (void)data;
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->dev_rotation = rot;
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

bool renderchain_init_shader_fvf(void *data, void *pass_)
{
   d3d_video_t *chain = (d3d_video_t*)data;
   d3d_video_t *pass = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)chain->dev;

#if defined(_XBOX360)
   static const D3DVERTEXELEMENT VertexElements[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      D3DDECL_END()
   };

   if (FAILED(d3dr->CreateVertexDeclaration(VertexElements, &pass->vertex_decl)))
      return false;
#endif

   return true;
}

static bool renderchain_create_first_pass(void *data, const video_info_t *info)
{
   HRESULT ret;
   d3d_video_t *chain = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)chain->dev;

   ret = D3DDevice_CreateVertexBuffers(d3dr, 4 * sizeof(Vertex), 
         D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &chain->vertex_buf, NULL);

   if (FAILED(ret))
      return false;

   ret = d3dr->CreateTexture(chain->tex_w, chain->tex_h, 1, 0, info->rgb32 ? D3DFMT_LIN_X8R8G8B8 : D3DFMT_LIN_R5G6B5,
         0, &chain->tex
#ifdef _XBOX360
         , NULL
#endif
         );

   if (FAILED(ret))
      return false;

#ifdef _XBOX1
   d3dr->SetRenderState(D3DRS_LIGHTING, FALSE);
#endif
   d3dr->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
   d3dr->SetRenderState(D3DRS_ZENABLE, FALSE);

   if (!renderchain_init_shader_fvf(chain, chain))
      return false;

   return true;
}


static bool renderchain_init(void *data, const video_info_t *info)
{
   d3d_video_t *chain = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)chain->dev;

   chain->pixel_size   = info->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

   if (!renderchain_create_first_pass(chain, info))
      return false;

   if (g_extern.console.screen.viewports.custom_vp.width == 0)
      g_extern.console.screen.viewports.custom_vp.width = chain->screen_width;

   if (g_extern.console.screen.viewports.custom_vp.height == 0)
      g_extern.console.screen.viewports.custom_vp.height = chain->screen_height;

   return true;
}

static bool d3d_init_chain(void *data, const video_info_t *info)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d_video_t *link_info = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;
   link_info->tex_w = link_info->tex_h = RARCH_SCALE_BASE * info->input_scale;

   d3d_deinit_chain(d3d);
#ifndef _XBOX
   d3d->chain = new renderchain_t();
   if (!d3d->chain)
      return false;

   if (!renderchain_init(d3d->chain, &d3d->video_info, d3dr, d3d->cgCtx, &d3d->final_viewport, &link_info,
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
#else
 if (!renderchain_init(d3d, info))
   {
      RARCH_ERR("[D3D]: Failed to init render chain.\n");
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

static void d3d_reinit_renderchain(void *data, const video_info_t *video)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   d3d->pixel_size   = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   d3d->tex_w = d3d->tex_h = RARCH_SCALE_BASE * video->input_scale;

   RARCH_LOG("Reinitializing renderchain - and textures (%u x %u @ %u bpp)\n", d3d->tex_w,
            d3d->tex_h, d3d->pixel_size * CHAR_BIT);
   d3d_deinit_chain(d3d);
   d3d_init_chain(d3d, video);
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

   if (d3d->d3d_err = d3d->g_pD3D->CreateDevice(0,
            D3DDEVTYPE_HAL,
            NULL,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &d3dpp,
            &d3d->dev) != S_OK)
   {
      RARCH_ERR("[D3D]: Failed to initialize device.\n");
      return false;
   }


   return true;
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
   Vertex newVerts[] =
   {
      // x,           y,              z,     color, u ,v
      {fX,            fY,             0.0f,  0,     0, 0},
      {fX + w,        fY,             0.0f,  0,     1, 0},
      {fX + w,        fY + h,         0.0f,  0,     1, 1},
      {fX,            fY + h,         0.0f,  0,     0, 1}
   };

   // load the existing vertices
   Vertex *pCurVerts;

   HRESULT ret = out_img->vertex_buf->Lock(0, 0, (unsigned char**)&pCurVerts, 0);

   if (FAILED(ret))
      return false;

   // copy the new verts over the old verts
   memcpy(pCurVerts, newVerts, 4 * sizeof(Vertex));
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
   D3DDevice_SetStreamSources(d3dr, 0, out_img->vertex_buf, 0, sizeof(Vertex));
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
   D3DDevice_DrawPrimitive(d3dr, D3DPT_QUADLIST, 0, 1);

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
      texture_image_render(d3d, menu_texture, menu_texture->x, menu_texture->y,
         d3d->screen_width, d3d->screen_height, true);
      d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
   }
#endif
}
#endif

static void d3d_calculate_rect(void *data, unsigned width, unsigned height,
   bool keep, float desired_aspect);

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
         }.
         RARCH_WARN("[D3D]: Attempting to recover from dead state (%s).\n", err);
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

   d3d_calculate_rect(d3d, d3d->screen_width, d3d->screen_height, info->force_aspect, g_extern.system.aspect_ratio);

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


bool d3d_restore(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
#ifdef _XBOX
   d3d_deinitialize(d3d);
   d3d->needs_restore = !d3d_initialize(d3d, &d3d->video_info);
#else
   d3d->needs_restore = false;
#endif

   if (d3d->needs_restore)
      RARCH_ERR("[D3D]: Restore error.\n");

   return !d3d->needs_restore;
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

#ifndef _XBOX
   d3d_set_font_rect(d3d, NULL);
#endif
}

static void d3d_calculate_rect(void *data, unsigned width, unsigned height,
   bool keep, float desired_aspect)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

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
   // We cannot recover in fullscreen.
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
      d3d_calculate_rect(d3d, d3d->screen_width, d3d->screen_height, d3d->video_info.force_aspect, g_extern.system.aspect_ratio);
#ifndef _XBOX
      renderchain_set_final_viewport(d3d->chain, &d3d->final_viewport);
      d3d_recompute_pass_sizes(d3d);
#endif

      d3d->should_resize = false;
   }

   // render_chain() only clears out viewport, clear out everything
   screen_vp.X = 0;
   screen_vp.Y = 0;
   screen_vp.MinZ = 0;
   screen_vp.MaxZ = 1;
   screen_vp.Width = d3d->screen_width;
   screen_vp.Height = d3d->screen_height;
   d3dr->SetViewport(&screen_vp);
   d3dr->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);

   // Insert black frame first, so we can screenshot, etc.
   if (g_settings.video.black_frame_insertion)
   {
      D3DDevice_Presents(d3d, d3dr);
      if (d3d->needs_restore)
         return true;
      d3dr->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);
   }

#ifdef _XBOX
   renderchain_render_pass(d3d, frame, width, height, pitch, d3d->dev_rotation);
#else
   if (!renderchain_render(d3d->chain, frame, width, height, pitch, d3d->dev_rotation))
   {
      RARCH_ERR("[D3D]: Failed to render scene.\n");
      return false;
   }
#endif

   if (d3d->font_ctx && d3d->font_ctx->render_msg && msg)
   {
#if defined(_XBOX1)
      float msg_width  = 60;
      float msg_height = 365;
#elif defined(_XBOX360)
      float msg_width  = (g_extern.lifecycle_state & (1ULL << MODE_MENU_HD)) ? 160 : 100;
      float msg_height = 90;
#endif
      struct font_params font_parms = {0};
      font_parms.x = msg_width;
      font_parms.y = msg_height;
      font_parms.scale = 21;
      d3d->font_ctx->render_msg(d3d, msg, &font_parms);
   }

#ifdef HAVE_MENU
   if (g_extern.lifecycle_state & (1ULL << MODE_MENU) && driver.menu_ctx && driver.menu_ctx->frame)
      driver.menu_ctx->frame();

   if (d3d && d3d->menu_texture_enable)
      d3d_draw_texture(d3d);
#endif

   RARCH_PERFORMANCE_STOP(d3d_frame);

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->update_window_title)
      d3d->ctx_driver->update_window_title(d3d);

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->swap_buffers)
      d3d->ctx_driver->swap_buffers(d3d);

   return true;
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
      d3d->ctx_driver->check_window(d3d, &quit, &resize, &d3d->screen_width,
      &d3d->screen_height, g_extern.frame_count);

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

static void d3d_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         gfx_set_square_pixel_viewport(g_extern.system.av_info.geometry.base_width, g_extern.system.av_info.geometry.base_height);
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

   g_extern.system.aspect_ratio  = aspectratio_lut[aspect_ratio_idx].value;
   d3d->video_info.force_aspect = true;
   d3d->should_resize = true;
}

static void d3d_apply_state_changes(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
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
   d3d->menu_texture_enable = state;
   d3d->menu_texture_full_screen = full_screen;
}
#endif

static void d3d_set_osd_msg(void *data, const char *msg, const struct font_params *params)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d->font_ctx && d3d->font_ctx->render_msg)
      d3d->font_ctx->render_msg(d3d, msg, params);
}

static const video_poke_interface_t d3d_poke_interface = {
   NULL,
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

// Delay constructor due to lack of exceptions.
static bool d3d_construct(void *data, const video_info_t *info, const input_driver_t **input,
                          void **input_data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->should_resize = false;
#ifndef _XBOX
   gfx_set_dwm();
#endif

#ifdef HAVE_MENU
   if (d3d->menu)
      free(d3d->menu);

   d3d->menu = (overlay_t*)calloc(1, sizeof(overlay_t));
   d3d->menu->tex_coords.x = 0;
   d3d->menu->tex_coords.y = 0;
   d3d->menu->tex_coords.w = 1;
   d3d->menu->tex_coords.h = 1;
   d3d->menu->vert_coords.x = 0;
   d3d->menu->vert_coords.y = 1;
   d3d->menu->vert_coords.w = 1;
   d3d->menu->vert_coords.h = -1;
#endif

#ifdef HAVE_WINDOW
   memset(&d3d->windowClass, 0, sizeof(d3d->windowClass));
   d3d->windowClass.cbSize        = sizeof(d3d->windowClass);
   d3d->windowClass.style         = CS_HREDRAW | CS_VREDRAW;
   d3d->windowClass.lpfnWndProc   = WindowProc;
   d3d->windowClass.hInstance     = NULL;
   d3d->windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
   d3d->windowClass.lpszClassName = "RetroArch";
   d3d->windowClass.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
   d3d->windowClass.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
   if (!info->fullscreen)
      d3d->windowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;

   RegisterClassEx(&d3d->windowClass);
#endif

   unsigned full_x, full_y;
#ifdef HAVE_MONITOR
   RECT mon_rect = d3d_monitor_rect(d3d);

   bool windowed_full = g_settings.video.windowed_fullscreen;

   full_x = (windowed_full || info->width  == 0) ? (mon_rect.right  - mon_rect.left) : info->width;
   full_y = (windowed_full || info->height == 0) ? (mon_rect.bottom - mon_rect.top)  : info->height;
   RARCH_LOG("[D3D]: Monitor size: %dx%d.\n", (int)(mon_rect.right  - mon_rect.left), (int)(mon_rect.bottom - mon_rect.top));
#else
   if (d3d->ctx_driver && d3d->ctx_driver->get_video_size)
      d3d->ctx_driver->get_video_size(d3d, &full_x, &full_y);
#endif

   d3d->screen_width  = info->fullscreen ? full_x : info->width;
   d3d->screen_height = info->fullscreen ? full_y : info->height;

#ifdef HAVE_WINDOW
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

#ifdef HAVE_WINDOW
   ShowWindow(d3d->hWnd, SW_RESTORE);
   UpdateWindow(d3d->hWnd);
   SetForegroundWindow(d3d->hWnd);
   SetFocus(d3d->hWnd);
#endif

#ifdef HAVE_WINDOW
   ShowWindow(d3d->hWnd, SW_RESTORE);
   UpdateWindow(d3d->hWnd);
   SetForegroundWindow(d3d->hWnd);
   SetFocus(d3d->hWnd);
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

static const gfx_ctx_driver_t *d3d_get_context(void *data)
{
   // TODO: GL core contexts through ANGLE?
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
   return gfx_ctx_init_first(driver.video_data, api, major, minor, false);
}

static void *d3d_init(const video_info_t *info, const input_driver_t **input, void **input_data)
{
   if (driver.video_data)
   {
      d3d_video_t *vid = (d3d_video_t*)driver.video_data;
      // Reinitialize renderchain as we might have changed pixel formats.
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

   d3d_video_t *vid = new d3d_video_t();
   if (!vid)
      return NULL;

   vid->ctx_driver = d3d_get_context(vid);
   if (!vid->ctx_driver)
   {
      free(vid);
      return NULL;
   }

   //default values
   vid->g_pD3D               = NULL;
   vid->dev                  = NULL;
#ifndef _XBOX
   vid->font                 = NULL;
#endif
   vid->dev_rotation         = 0;
   vid->needs_restore        = false;
#ifdef HAVE_CG
   vid->cgCtx                = NULL;
#endif
#ifdef HAVE_OVERLAY
   vid->overlays_enabled = false;
#endif
   vid->should_resize        = false;
   vid->vsync                = info->vsync;
   vid->menu                 = NULL;

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

const video_driver_t video_d3d = {
   d3d_init,
   d3d_frame,
   d3d_set_nonblock_state,
   d3d_alive,
   d3d_focus,
   d3d_set_shader,
   d3d_free,
   "d3d",
   d3d_set_rotation,
   d3d_viewport_info,
   NULL, /* read_viewport */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   d3d_get_poke_interface,
};
