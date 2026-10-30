/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* D3D9's views, shared by its HLSL and Cg drivers, which differ in how
 * a pixel shader is made and bound, and in whether a preset draws views.
 * The stock chain draws each view straight from the frame's texture, a
 * preset each through a chain of its own; anaglyph and interlaced blend
 * a canvas of both eyes; the UI is drawn once into a layer placed in
 * each eye. The views' own quads are pretransformed, so no vertex shader
 * is bound for them.
 *
 * Define before including, then include once per driver:
 *   D3D9V_FN(name)             the driver's prefix for name
 *   D3D9V_TAG                  its log tag, as "[D3D9 HLSL]"
 *   D3D9V_PS_MAKE(d3d, i, src) views.ps[i] and views.ps_reg[i], the
 *                              register of its views_c, from @src's
 *                              main_fragment at shader model 2; false
 *                              if it can't be made
 *   D3D9V_PS_FREE(d3d, i)      views.ps[i] let go
 *   D3D9V_PS_BIND(d3d, i)      views.ps[i] bound
 *   D3D9V_PRESETS              defined where a preset draws views:
 *   D3D9V_CHAINS_BUILD(d3d, map)   a preset chain in views.chains for
 *                              each of the map's views that has none
 *   D3D9V_CHAINS_FREE(d3d, first)  the preset chains from @first on
 *                              let go
 *   D3D9V_PASS(d3d, d, c, dst, cut, again)  as video_views_draw.h's
 *                              VV_PASS
 * The macros are undefined again at the end. */

#include <string.h>

#include "../video_views.h"
#include "../video_defines.h"
#include "../drivers/d3d_shaders/views_sm2.hlsl.h"

/* What a frame's placement draws read: the frame, @pitch bytes a row,
 * its texture, that's size, filter and rotation, and the rectangle each
 * view's chain is sized for */
struct d3d9_views_draw
{
   const video_frame_info_t *video_info;
   const void *frame;
   const video_views_rect_t *rects;
   LPDIRECT3DTEXTURE9 tex;
   unsigned pitch;
   unsigned tex_dims;
   DWORD filter;
   unsigned rotation;
};

struct d3d9_views_vertex
{
   float x, y, z, rhw;
   float u, v;
};

static void D3D9V_FN(views_draw_direct)(d3d9_video_t *d3d,
      const struct d3d9_views_draw *d, unsigned view,
      const video_views_rect_t *dst, const video_views_rect_t *cut);
static bool D3D9V_FN(views_target_make)(d3d9_video_t *d3d,
      d3d9_views_target_t *t, unsigned dims);
static bool D3D9V_FN(views_target_retire)(d3d9_video_t *d3d,
      d3d9_views_target_t *t);

/* The views compositor, with these calls bound in. The stock chain's
 * views are drawn straight from the frame, so their chain is the driver
 * itself; a preset's chains are the driver's. Parameters are read from
 * the preset as each pass draws. */
#define VV_FN(name)                     D3D9V_FN(vv_##name)
#define VV_DRV                          d3d9_video_t
#define VV_CHAIN                        void
#define VV_CREATE_STOCK(drv)            ((void*)(drv))
#define VV_SYNC_PARAMS(drv, c)          ((void)0)
#define VV_DRAW                         struct d3d9_views_draw
#define VV_DIRECT(p, d, v, r, c)        D3D9V_FN(views_draw_direct)(p, d, v, r, c)
#define VV_PASS(p, d, f, r, c, a)       D3D9V_PASS(p, d, f, r, c, a)
#define VV_TARGET                       d3d9_views_target_t
#define VV_TARGET_MAKE(drv, t, dims)    D3D9V_FN(views_target_make)(drv, t, dims)
#define VV_TARGET_RETIRE(drv, t)        D3D9V_FN(views_target_retire)(drv, t)
#include "../video_views_draw.h"

/* Whether a preset is loaded */
static bool D3D9V_FN(views_preset)(const d3d9_video_t *d3d)
{
   return d3d->shader_path && *d3d->shader_path;
}

/* Not once the shaders couldn't be made, nor with a preset where a
 * preset draws the frame whole */
static bool D3D9V_FN(views_allowed)(const d3d9_video_t *d3d)
{
#ifdef D3D9V_PRESETS
   return !d3d->views.failed;
#else
   return !d3d->views.failed && !D3D9V_FN(views_preset)(d3d);
#endif
}

static void D3D9V_FN(views_publish)(d3d9_video_t *d3d)
{
   retro_atomic_store_release_int(&d3d->views.allowed,
         D3D9V_FN(views_allowed)(d3d) ? 1 : 0);
}

static bool D3D9V_FN(views_target_retire)(d3d9_video_t *d3d,
      d3d9_views_target_t *t)
{
   if (t->surf)
      IDirect3DSurface9_Release(t->surf);
   if (t->tex)
      IDirect3DTexture9_Release(t->tex);
   t->surf = NULL;
   t->tex  = NULL;
   return true;
}

/* A render target of ARGB8 sized @dims */
static bool D3D9V_FN(views_target_make)(d3d9_video_t *d3d,
      d3d9_views_target_t *t, unsigned dims)
{
   if (     SUCCEEDED(IDirect3DDevice9_CreateTexture(d3d->dev,
               VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), 1,
               D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
               &t->tex, NULL))
         && SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(t->tex, 0,
               &t->surf)))
      return true;
   RARCH_WARN(D3D9V_TAG " Views can't draw into a %ux%u image. "
         "Drawing without it.\n", VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
   D3D9V_FN(views_target_retire)(d3d, t);
   return false;
}

/* Everything views made, let go before the device is reset or freed:
 * made again on the next frame with views */
static void D3D9V_FN(views_release)(d3d9_video_t *d3d)
{
   unsigned i;
   D3D9V_CHAINS_FREE(d3d, 0);
   D3D9V_FN(vv_target_drop)(d3d, &d3d->views.canvas_target,
         &d3d->views.canvas);
   D3D9V_FN(vv_target_drop)(d3d, &d3d->views.ui_target, &d3d->views.ui);
   for (i = 0; i < D3D9_VIEWS_PS_COUNT; i++)
   {
      if (d3d->views.ps[i])
         D3D9V_PS_FREE(d3d, i);
      d3d->views.ps[i]     = NULL;
      d3d->views.ps_reg[i] = -1;
   }
   d3d->views.tried  = false;
   d3d->views.failed      = false;
   d3d->views.blend       = false;
   d3d->views.layer       = false;
}

/* The shaders views draw with, made on first use: most sessions never
 * draw views. Without the rect shader views aren't drawn; without the
 * others, not into the canvas or UI layer. */
static void D3D9V_FN(views_init)(d3d9_video_t *d3d)
{
   D3DCAPS9 caps;
   unsigned i;

   d3d->views.tried = true;
   for (i = 0; i < D3D9_VIEWS_PS_COUNT; i++)
   {
      d3d->views.ps[i]     = NULL;
      d3d->views.ps_reg[i] = -1;
   }
   memset(&caps, 0, sizeof(caps));
   IDirect3DDevice9_GetDeviceCaps(d3d->dev, &caps);
   if (     caps.PixelShaderVersion < D3DPS_VERSION(2, 0)
         || !D3D9V_PS_MAKE(d3d, D3D9_VIEWS_PS_RECT, d3d9_views_ps_rect)
         || d3d->views.ps_reg[D3D9_VIEWS_PS_RECT] < 0)
   {
      RARCH_WARN(D3D9V_TAG " Views can't be drawn: no pixel shader "
            "for them.\n");
      d3d->views.failed = true;
      return;
   }
   d3d->views.blend = D3D9V_PS_MAKE(d3d, D3D9_VIEWS_PS_BLEND,
         d3d9_views_ps_blend)
      && d3d->views.ps_reg[D3D9_VIEWS_PS_BLEND] >= 0;
   d3d->views.layer = (caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND)
      && D3D9V_PS_MAKE(d3d, D3D9_VIEWS_PS_LAYER, d3d9_views_ps_layer);
}

/* A scissor rectangle from a top-left one */
static void D3D9V_FN(views_cut)(d3d9_video_t *d3d,
      const video_views_rect_t *r)
{
   RECT rect;
   rect.left   = VIDEO_POS_X(r->pos);
   rect.top    = VIDEO_POS_Y(r->pos);
   rect.right  = rect.left + (LONG)VIDEO_SCALE_W(r->dims);
   rect.bottom = rect.top  + (LONG)VIDEO_SCALE_H(r->dims);
   IDirect3DDevice9_SetScissorRect(d3d->dev, &rect);
}

/* Texture coordinates @uv, left, top, right and bottom, over @r of the
 * target, turned @rotation quarter turns as the frame is. Pixel centres
 * are a half pixel in from a pretransformed quad's edges. */
static void D3D9V_FN(views_quad)(d3d9_video_t *d3d,
      const video_views_rect_t *r, const float *uv, unsigned rotation)
{
   unsigned k;
   struct d3d9_views_vertex v[4];
   /* The texture's corners clockwise from its top left, and the quad's
    * in the same order: top left, top right, bottom right, bottom left */
   float s[4][2];
   static const unsigned strip[4] = { 0, 1, 3, 2 };
   float x0 = (float)VIDEO_POS_X(r->pos) - 0.5f;
   float y0 = (float)VIDEO_POS_Y(r->pos) - 0.5f;
   float x1 = x0 + (float)VIDEO_SCALE_W(r->dims);
   float y1 = y0 + (float)VIDEO_SCALE_H(r->dims);

   s[0][0] = uv[0]; s[0][1] = uv[1];
   s[1][0] = uv[2]; s[1][1] = uv[1];
   s[2][0] = uv[2]; s[2][1] = uv[3];
   s[3][0] = uv[0]; s[3][1] = uv[3];
   for (k = 0; k < 4; k++)
   {
      unsigned c = strip[k];
      unsigned t = (c + rotation) & 3;
      v[k].x     = (c == 0 || c == 3) ? x0 : x1;
      v[k].y     = (c < 2) ? y0 : y1;
      v[k].z     = 0.0f;
      v[k].rhw   = 1.0f;
      v[k].u     = s[t][0];
      v[k].v     = s[t][1];
   }
   IDirect3DDevice9_SetFVF(d3d->dev, D3DFVF_XYZRHW | D3DFVF_TEX1);
   IDirect3DDevice9_DrawPrimitiveUP(d3d->dev, D3DPT_TRIANGLESTRIP, 2, v,
         sizeof(v[0]));
}

/* @tex sampled with @filter, clamped */
static void D3D9V_FN(views_sample)(d3d9_video_t *d3d,
      LPDIRECT3DTEXTURE9 tex, DWORD filter)
{
   IDirect3DDevice9_SetTexture(d3d->dev, 0, (IDirect3DBaseTexture9*)tex);
   IDirect3DDevice9_SetSamplerState(d3d->dev, 0, D3DSAMP_MINFILTER, filter);
   IDirect3DDevice9_SetSamplerState(d3d->dev, 0, D3DSAMP_MAGFILTER, filter);
   IDirect3DDevice9_SetSamplerState(d3d->dev, 0, D3DSAMP_ADDRESSU,
         D3DTADDRESS_CLAMP);
   IDirect3DDevice9_SetSamplerState(d3d->dev, 0, D3DSAMP_ADDRESSV,
         D3DTADDRESS_CLAMP);
}

/* View @view over @dst, cut to @cut, straight from the frame */
static void D3D9V_FN(views_draw_direct)(d3d9_video_t *d3d,
      const struct d3d9_views_draw *d, unsigned view,
      const video_views_rect_t *dst, const video_views_rect_t *cut)
{
   float uv[4], c[4];
   const struct retro_video_view *v = &d->video_info->views.views[view];
   float tw = (float)VIDEO_SCALE_W(d->tex_dims);
   float th = (float)VIDEO_SCALE_H(d->tex_dims);

   uv[0] = (float)v->x / tw;
   uv[1] = (float)v->y / th;
   uv[2] = (float)(v->x + v->width)  / tw;
   uv[3] = (float)(v->y + v->height) / th;
   c[0]  = ((float)v->x + 0.5f) / tw;
   c[1]  = ((float)v->y + 0.5f) / th;
   c[2]  = ((float)(v->x + v->width)  - 0.5f) / tw;
   c[3]  = ((float)(v->y + v->height) - 0.5f) / th;
   IDirect3DDevice9_SetPixelShaderConstantF(d3d->dev,
         d3d->views.ps_reg[D3D9_VIEWS_PS_RECT], c, 1);
   D3D9V_FN(views_cut)(d3d, cut);
   D3D9V_FN(views_quad)(d3d, dst, uv, d->rotation);
}

/* Whether this frame draws its views, with each drawn view's chain
 * picked and the rectangle its first placement takes, in *blend
 * whether into the canvas, and in *direct whether straight from the
 * frame. Drawn whole when the stereo mode or the UI would need a canvas
 * or layer that can't be had. @menu: the menu is up. */
static bool D3D9V_FN(views_setup)(d3d9_video_t *d3d,
      video_frame_info_t *video_info, const char *msg, bool menu,
      bool *blend, bool *direct, void **chains, video_views_rect_t *rects)
{
   bool views, per_eye, ui;
   bool stock = !D3D9V_FN(views_preset)(d3d);
   const video_views_layout_t *layout = &video_info->views_layout;
   bool offscreen = video_info->views.num_views && layout->offscreen;

   if (video_info->views.num_views && !d3d->views.tried)
      D3D9V_FN(views_init)(d3d);
   *blend = offscreen && d3d->views.blend
      && D3D9V_FN(vv_target_ensure)(d3d, &d3d->views.canvas_target,
            &d3d->views.canvas, layout->canvas_dims);
   /* A frame without the blend modes frees the canvas. */
   if (!offscreen)
      D3D9V_FN(vv_target_drop)(d3d, &d3d->views.canvas_target,
            &d3d->views.canvas);
   per_eye = video_views_ui_per_eye(&video_info->views, layout,
         D3D9V_FN(views_allowed)(d3d));
   ui = VIDEO_VIEWS_UI_WANTED(per_eye, menu, msg,
         video_info->statistics_show,
         video_info->widgets_visible);
   views = video_info->views.num_views
      && D3D9V_FN(views_allowed)(d3d)
      && (!offscreen || *blend)
      && (!ui || d3d->views.layer)
      && video_views_fit_frame(&video_info->views, d3d->views.frame_dims);
#ifdef D3D9V_PRESETS
   if (views && !stock)
      D3D9V_CHAINS_BUILD(d3d, &video_info->views);
#endif
   views = views
      && D3D9V_FN(vv_pick_chains)(d3d, &d3d->views.chains,
            &video_info->views, layout, stock, *blend || !layout->offscreen,
            *blend ? layout->canvas_dims : video_info->dims, chains, rects);
   *blend  = views && *blend;
   *direct = views && stock;
   if (*direct && !d3d->views.direct_seen)
   {
      d3d->views.direct_seen = true;
      RARCH_LOG(D3D9V_TAG " Views drawn straight from the frame.\n");
   }
   if (views && !stock && !d3d->views.preset_seen)
   {
      d3d->views.preset_seen = true;
      RARCH_LOG(D3D9V_TAG " Views drawn through the preset, each by a "
            "chain of its own.\n");
      RARCH_LOG(D3D9V_TAG " Views' presets have each view of a software "
            "frame written straight in.\n");
   }
   return views;
}

/* A viewport over @dims */
static void D3D9V_FN(views_viewport)(d3d9_video_t *d3d, unsigned dims)
{
   D3DVIEWPORT9 vp;
   vp.X      = 0;
   vp.Y      = 0;
   vp.Width  = VIDEO_SCALE_W(dims);
   vp.Height = VIDEO_SCALE_H(dims);
   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;
   IDirect3DDevice9_SetViewport(d3d->dev, &vp);
}

/* Draws the views into the back buffer, or with @blend into the canvas,
 * whose halves one pass then blends into the back buffer: with @direct
 * each straight from the frame, else through its chain. Leaves the
 * driver's shaders to be bound again. */
static void D3D9V_FN(views_render)(d3d9_video_t *d3d,
      const struct d3d9_views_draw *d, void **chains, bool blend,
      bool direct)
{
   video_views_rect_t whole;
   LPDIRECT3DSURFACE9 back            = NULL;
   const video_frame_info_t *info     = d->video_info;
   const video_views_layout_t *layout = &info->views_layout;
   unsigned dims = blend ? layout->canvas_dims : info->dims;

   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, FALSE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ZENABLE, FALSE);
   IDirect3DDevice9_SetVertexShader(d3d->dev, NULL);
   if (blend)
   {
      IDirect3DDevice9_GetRenderTarget(d3d->dev, 0, &back);
      IDirect3DDevice9_SetRenderTarget(d3d->dev, 0, d3d->views.canvas.surf);
   }
   D3D9V_FN(views_viewport)(d3d, dims);
   whole.pos  = 0;
   whole.dims = dims;
   D3D9V_FN(views_cut)(d3d, &whole);
   /* The back buffer was cleared for the frame. */
   if (blend)
      IDirect3DDevice9_Clear(d3d->dev, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);

   if (direct)
   {
      IDirect3DDevice9_BeginScene(d3d->dev);
      D3D9V_PS_BIND(d3d, D3D9_VIEWS_PS_RECT);
      D3D9V_FN(views_sample)(d3d, d->tex, d->filter);
   }
   D3D9V_FN(vv_draw_placements)(d3d, d, layout, chains,
         blend || !layout->offscreen, dims, direct);
   if (direct)
      IDirect3DDevice9_EndScene(d3d->dev);

   if (blend)
   {
      float c[4];
      float uv[4];
      c[0]  = (layout->stereo_mode == VIDEO_STEREO_MODE_INTERLACED)
         ? 1.0f : 0.0f;
      c[1]  = (float)VIDEO_SCALE_H(layout->canvas_dims);
      c[2]  = 0.0f;
      c[3]  = 0.0f;
      uv[0] = 0.0f;
      uv[1] = 0.0f;
      uv[2] = 1.0f;
      uv[3] = 1.0f;
      IDirect3DDevice9_SetRenderTarget(d3d->dev, 0, back);
      IDirect3DSurface9_Release(back);
      IDirect3DDevice9_SetVertexShader(d3d->dev, NULL);
      IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE,
            FALSE);
      IDirect3DDevice9_BeginScene(d3d->dev);
      D3D9V_FN(views_viewport)(d3d, info->dims);
      whole.dims = info->dims;
      D3D9V_FN(views_cut)(d3d, &whole);
      D3D9V_PS_BIND(d3d, D3D9_VIEWS_PS_BLEND);
      IDirect3DDevice9_SetPixelShaderConstantF(d3d->dev,
            d3d->views.ps_reg[D3D9_VIEWS_PS_BLEND], c, 1);
      D3D9V_FN(views_sample)(d3d, d3d->views.canvas.tex, D3DTEXF_POINT);
      D3D9V_FN(views_quad)(d3d, &whole, uv, 0);
      IDirect3DDevice9_EndScene(d3d->dev);
   }
   whole.dims = info->dims;
   D3D9V_FN(views_cut)(d3d, &whole);
   IDirect3DDevice9_SetTexture(d3d->dev, 0, NULL);
   IDirect3DDevice9_SetPixelShader(d3d->dev, NULL);
}

/* Moves UI drawing into the UI layer, laid out at one eye's size */
static void D3D9V_FN(views_ui_begin)(d3d9_video_t *d3d,
      video_frame_info_t *video_info)
{
   video_views_rect_t whole;
   unsigned ui_dims = video_info->views_layout.ui_dims;

   IDirect3DDevice9_GetRenderTarget(d3d->dev, 0, &d3d->views.back);
   IDirect3DDevice9_SetRenderTarget(d3d->dev, 0, d3d->views.ui.surf);
   whole.pos  = 0;
   whole.dims = ui_dims;
   D3D9V_FN(views_cut)(d3d, &whole);
   IDirect3DDevice9_Clear(d3d->dev, 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
   /* Menus and widgets size from video_info->dims, fonts from
    * vp.full_dims. */
   video_views_ui_enter(&d3d->views.saved, &video_info->dims,
         &d3d->vp.full_dims, ui_dims);
   /* Drawn over transparent black, the layer's colour premultiplied */
   IDirect3DDevice9_SetRenderState(d3d->dev,
         D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLENDALPHA,
         D3DBLEND_ONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLENDALPHA,
         D3DBLEND_INVSRCALPHA);
}

/* The out viewport is the whole window while views draw, so a menu not
 * drawn full screen is fitted to its target, of @dims, here. */
static void D3D9V_FN(views_menu_viewport)(d3d9_video_t *d3d, unsigned dims)
{
   struct video_viewport vp;
   D3DVIEWPORT9 out;
   memset(&vp, 0, sizeof(vp));
   vp.full_dims = dims;
   video_driver_update_viewport(&vp, false, d3d->keep_aspect, true);
   out.X        = (VIDEO_POS_X(vp.pos) > 0) ? VIDEO_POS_X(vp.pos) : 0;
   out.Y        = (VIDEO_POS_Y(vp.pos) > 0) ? VIDEO_POS_Y(vp.pos) : 0;
   out.Width    = VIDEO_SCALE_W(vp.dims);
   out.Height   = VIDEO_SCALE_H(vp.dims);
   out.MinZ     = 0.0f;
   out.MaxZ     = 1.0f;
   IDirect3DDevice9_SetViewport(d3d->dev, &out);
}

/* Back onto the back buffer, and the UI layer into each eye's area */
static void D3D9V_FN(views_ui_end)(d3d9_video_t *d3d,
      video_frame_info_t *video_info)
{
   unsigned a;
   video_views_rect_t whole;
   float uv[4];
   const video_views_layout_t *layout = &video_info->views_layout;
   /* Only full side by side places it unscaled. */
   DWORD filter = (layout->stereo_mode == VIDEO_STEREO_MODE_SBS_FULL)
      ? D3DTEXF_POINT : D3DTEXF_LINEAR;

   IDirect3DDevice9_SetRenderState(d3d->dev,
         D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
   video_views_ui_leave(&d3d->views.saved, &video_info->dims,
         &d3d->vp.full_dims);
   IDirect3DDevice9_SetRenderTarget(d3d->dev, 0, d3d->views.back);
   IDirect3DSurface9_Release(d3d->views.back);
   d3d->views.back = NULL;

   uv[0] = 0.0f;
   uv[1] = 0.0f;
   uv[2] = 1.0f;
   uv[3] = 1.0f;
   D3D9V_FN(views_viewport)(d3d, video_info->dims);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ZENABLE, FALSE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, TRUE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLEND, D3DBLEND_ONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLEND,
         D3DBLEND_INVSRCALPHA);
   IDirect3DDevice9_SetVertexShader(d3d->dev, NULL);
   D3D9V_PS_BIND(d3d, D3D9_VIEWS_PS_LAYER);
   D3D9V_FN(views_sample)(d3d, d3d->views.ui.tex, filter);
   IDirect3DDevice9_BeginScene(d3d->dev);
   for (a = 0; a < layout->num_areas; a++)
   {
      D3D9V_FN(views_cut)(d3d, &layout->areas[a]);
      D3D9V_FN(views_quad)(d3d, &layout->areas[a], uv, 0);
   }
   IDirect3DDevice9_EndScene(d3d->dev);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLEND,
         D3DBLEND_SRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, FALSE);
   whole.pos  = 0;
   whole.dims = video_info->dims;
   D3D9V_FN(views_cut)(d3d, &whole);
   IDirect3DDevice9_SetTexture(d3d->dev, 0, NULL);
   IDirect3DDevice9_SetPixelShader(d3d->dev, NULL);
}

static void D3D9V_FN(set_view_count)(void *data, unsigned count)
{
   unsigned i;
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (!d3d)
      return;
   d3d->views.chains.count = MIN(count, RETRO_VIDEO_VIEWS_MAX);
   /* Views read the frame where the core left it: no loan */
   if (count)
      d3d9_sw_fb_drop(&d3d->sw_fb);
   D3D9V_CHAINS_FREE(d3d, d3d->views.chains.count);
   for (i = d3d->views.chains.count; i < RETRO_VIDEO_VIEWS_MAX; i++)
      d3d->views.chains.stock[i] = NULL;
}

#undef D3D9V_FN
#undef D3D9V_TAG
#undef D3D9V_PS_MAKE
#undef D3D9V_PS_FREE
#undef D3D9V_PS_BIND
#undef D3D9V_PRESETS
#undef D3D9V_CHAINS_BUILD
#undef D3D9V_CHAINS_FREE
#undef D3D9V_PASS
