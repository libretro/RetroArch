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

#include "../../driver.h"
#include "xdk_d3d.h"

#ifdef HAVE_HLSL
#include "../../gfx/shader_hlsl.h"
#endif

#include "./../../gfx/gfx_context.h"
#include "../../general.h"
#include "../../message_queue.h"

#include "../../xdk/xdk_resources.h"

#include "render_chain_xdk.h"

static bool d3d_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
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

static bool d3d_init_chain(d3d_video_t *d3d, const video_info_t *info)
{
   d3d_video_t *link_info = (d3d_video_t*)d3d;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;
   link_info->tex_w = link_info->tex_h = RARCH_SCALE_BASE * info->input_scale;

   d3d_deinit_chain(d3d);
#ifndef _XBOX
   d3d->chain = new renderchain_t();
   if (!d3d->chain)
      return false;

   if (!renderchain_init(d3d->chain, &d3d->video_info, d3dr, d3d->cgCtx,
            &d3d->final_viewport, &link_info,
            d3d->video_info.rgb32 ? ARGB : RGB565))
   {
      RARCH_ERR("[D3D]: Failed to init render chain.\n");
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
         RARCH_ERR("[D3D]: Failed to add pass.\n");
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
      RARCH_ERR("[D3D]: Failed to init imports.\n");
      return false;
   }
#endif
#endif

   return true;
}

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

   // load the existing vertices
   Vertex *pCurVerts;

   HRESULT ret = out_img->vertex_buf->Lock(0, 0,
         (unsigned char**)&pCurVerts, 0);

   if (FAILED(ret))
      return false;

   // copy the new verts over the old verts
   memcpy(pCurVerts, newVerts, 4 * sizeof(Vertex));
   out_img->vertex_buf->Unlock();

   d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
   d3d->dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d->dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

   /* Also blend the texture with the set alpha value. */
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
   d3d->dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);

   /* Draw the quad. */
   d3dr->SetTexture(0, out_img->pixels);
   D3DDevice_SetStreamSources(d3dr, 0,
         out_img->vertex_buf, 0, sizeof(Vertex));
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
      texture_image_render(d3d, menu_texture,
            menu_texture->x, menu_texture->y,
         d3d->screen_width, d3d->screen_height, true);
      d3d->dev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
   }
#endif
}
#endif

static void d3d_calculate_rect(void *data, unsigned width,
      unsigned height, bool keep, float desired_aspect);

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
      d3d_calculate_rect(d3d, d3d->screen_width, d3d->screen_height,
            d3d->video_info.force_aspect, g_extern.system.aspect_ratio);
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
   d3dr->SetViewport(&screen_vp);
   d3dr->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);

   /* Insert black frame first, so we can screenshot, etc. */
   if (g_settings.video.black_frame_insertion)
   {
      D3DDevice_Presents(d3d, d3dr);
      if (d3d->needs_restore)
         return true;
      d3dr->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);
   }

#ifdef _XBOX
   renderchain_render_pass(d3d, frame, width, height,
         pitch, d3d->dev_rotation);
#else
   if (!renderchain_render(d3d->chain, frame, width, height,
            pitch, d3d->dev_rotation))
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
   if (g_extern.lifecycle_state & (1ULL << MODE_MENU)
         && driver.menu_ctx && driver.menu_ctx->frame)
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

   d3d_show_mouse,
};

static void d3d_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &d3d_poke_interface;
}

static bool d3d_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return false;
}

video_driver_t video_d3d = {
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
   d3d_read_viewport,
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   d3d_get_poke_interface,
};
