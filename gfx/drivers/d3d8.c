/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - OV2
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

#define CINTERFACE

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include <formats/image.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_math.h>

#include <d3d8.h>

#include "../../defines/d3d_defines.h"
#include "../common/d3d8_common.h"
#include "../common/d3d_common.h"
#include "../video_coord_array.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../dynamic.h"
#include "../../frontend/frontend_driver.h"

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../common/win32_common.h"

#ifdef _XBOX
#define D3D8_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../core.h"
#include "../../verbosity.h"

#ifdef __WINRT__
#error "UWP does not support D3D8"
#endif

static LPDIRECT3D8 g_pD3D8;

#ifdef _XBOX
static bool d3d8_widescreen_mode = false;
#endif

void *dinput;

typedef struct d3d8_renderchain
{
   unsigned pixel_size;
   LPDIRECT3DDEVICE8 dev;
   const video_info_t *video_info;
   LPDIRECT3DTEXTURE8 tex;
   LPDIRECT3DVERTEXBUFFER8 vertex_buf;
   unsigned last_width;
   unsigned last_height;
   void *vertex_decl;
   unsigned tex_w;
   unsigned tex_h;
   uint64_t frame_count;
} d3d8_renderchain_t;

void d3d8_set_mvp(void *data, const void *mat_data)
{
   struct d3d_matrix matrix;
   LPDIRECT3DDEVICE8 d3dr     = (LPDIRECT3DDEVICE8)data;

   d3d_matrix_identity(&matrix);

   d3d8_set_transform(d3dr, D3DTS_PROJECTION, (D3DMATRIX*)&matrix);
   d3d8_set_transform(d3dr, D3DTS_VIEW, (D3DMATRIX*)&matrix);

   if (mat_data)
      d3d_matrix_transpose(&matrix, mat_data);

   d3d8_set_transform(d3dr, D3DTS_WORLD, (D3DMATRIX*)&matrix);
}

static bool d3d8_renderchain_create_first_pass(
      d3d8_video_t *d3d,
      d3d8_renderchain_t *chain,
      const video_info_t *info)
{
   LPDIRECT3DDEVICE8 d3dr     = (LPDIRECT3DDEVICE8)d3d->dev;

   chain->vertex_buf         = (LPDIRECT3DVERTEXBUFFER8)d3d8_vertex_buffer_new(d3dr, 4 * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
         D3DPOOL_MANAGED,
         NULL);

   if (!chain->vertex_buf)
      return false;

   chain->tex = (LPDIRECT3DTEXTURE8)d3d8_texture_new(d3dr, NULL,
         chain->tex_w, chain->tex_h, 1, 0,
         info->rgb32
         ?
         d3d8_get_xrgb8888_format() : d3d8_get_rgb565_format(),
         D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL,
         false);

   if (!chain->tex)
      return false;

   d3d8_set_sampler_address_u(d3dr, 0, D3DTADDRESS_BORDER);
   d3d8_set_sampler_address_v(d3dr, 0, D3DTADDRESS_BORDER);
   d3d8_set_render_state(d3dr, D3DRS_LIGHTING, 0);
   d3d8_set_render_state(d3dr, D3DRS_CULLMODE, D3DCULL_NONE);
   d3d8_set_render_state(d3dr, D3DRS_ZENABLE, FALSE);

   return true;
}

static void d3d8_renderchain_set_vertices(
      d3d8_video_t *d3d,
      d3d8_renderchain_t *chain,
      unsigned pass,
      unsigned vert_width, unsigned vert_height, uint64_t frame_count)
{
   unsigned width, height;

   video_driver_get_size(&width, &height);

   if (chain->last_width != vert_width || chain->last_height != vert_height)
   {
      Vertex vert[4];
      float tex_w, tex_h;
      void *verts        = NULL;

      chain->last_width  = vert_width;
      chain->last_height = vert_height;

      tex_w              = vert_width;
      tex_h              = vert_height;

      vert[0].x        =  0.0f;
      vert[0].y        =  1.0f;
      vert[0].z        =  1.0f;

      vert[1].x        =  1.0f;
      vert[1].y        =  1.0f;
      vert[1].z        =  1.0f;

      vert[2].x        =  0.0f;
      vert[2].y        =  0.0f;
      vert[2].z        =  1.0f;

      vert[3].x        =  1.0f;
      vert[3].y        =  0.0f;
      vert[3].z        =  1.0f;

      vert[0].u        = 0.0f;
      vert[0].v        = 0.0f;
      vert[1].v        = 0.0f;
      vert[2].u        = 0.0f;
      vert[1].u        = tex_w;
      vert[2].v        = tex_h;
      vert[3].u        = tex_w;
      vert[3].v        = tex_h;
#ifndef _XBOX
      vert[1].u       /= chain->tex_w;
      vert[2].v       /= chain->tex_h;
      vert[3].u       /= chain->tex_w;
      vert[3].v       /= chain->tex_h;
#endif

      vert[0].color    = 0xFFFFFFFF;
      vert[1].color    = 0xFFFFFFFF;
      vert[2].color    = 0xFFFFFFFF;
      vert[3].color    = 0xFFFFFFFF;

      verts = d3d8_vertex_buffer_lock(chain->vertex_buf);
      memcpy(verts, vert, sizeof(vert));
      d3d8_vertex_buffer_unlock(chain->vertex_buf);
   }
}

static void d3d8_renderchain_blit_to_texture(
      d3d8_renderchain_t *chain,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   D3DLOCKED_RECT d3dlr;
   LPDIRECT3DDEVICE8 d3dr     = (LPDIRECT3DDEVICE8)chain->dev;

   d3d8_frame_postprocess(chain);

   if (chain->last_width != width || chain->last_height != height)
   {
      d3d8_lock_rectangle(chain->tex,
            0, &d3dlr, NULL, chain->tex_h, D3DLOCK_NOSYSLOCK);
      d3d8_lock_rectangle_clear(chain->tex,
            0, &d3dlr, NULL, chain->tex_h, D3DLOCK_NOSYSLOCK);
   }

   /* Set the texture to NULL so D3D doesn't complain about it being in use... */
   d3d8_set_texture(d3dr, 0, NULL);

   if (d3d8_lock_rectangle(chain->tex, 0, &d3dlr, NULL, 0, 0))
   {
      d3d8_texture_blit(chain->pixel_size, chain->tex,
            &d3dlr, frame, width, height, pitch);
      d3d8_unlock_rectangle(chain->tex);
   }
}

static void d3d8_renderchain_free(void *data)
{
   d3d8_renderchain_t *chain = (d3d8_renderchain_t*)data;

   if (!chain)
      return;

   if (chain->tex)
      d3d8_texture_free(chain->tex);
   d3d8_vertex_buffer_free(chain->vertex_buf, chain->vertex_decl);

   free(chain);
}

static void d3d8_renderchain_viewport_info(
      d3d8_video_t *d3d,
      struct video_viewport *vp)
{
   unsigned width, height;

   if (!d3d || !vp)
      return;

   video_driver_get_size(&width, &height);

   vp->x            = d3d->final_viewport.X;
   vp->y            = d3d->final_viewport.Y;
   vp->width        = d3d->final_viewport.Width;
   vp->height       = d3d->final_viewport.Height;

   vp->full_width   = width;
   vp->full_height  = height;
}

static void d3d8_renderchain_render_pass(
      d3d8_video_t *d3d, LPDIRECT3DDEVICE8 d3dr,
      d3d8_renderchain_t *chain,
      unsigned pass_index,
      unsigned rotation)
{
   settings_t *settings      = config_get_ptr();
   bool video_smooth         = settings->bools.video_smooth;

   d3d8_set_texture(d3dr, 0, chain->tex);
   d3d8_set_sampler_magfilter(d3dr, pass_index, video_smooth ?
         D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d8_set_sampler_minfilter(d3dr, pass_index, video_smooth ?
         D3DTEXF_LINEAR : D3DTEXF_POINT);

   d3d8_set_viewports(chain->dev, &d3d->final_viewport);
   d3d8_set_vertex_shader(d3dr,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
         NULL);
   d3d8_set_stream_source(d3dr, 0, chain->vertex_buf, 0, sizeof(Vertex));
   d3d8_set_mvp(d3d->dev, &d3d->mvp_rotate);
   d3d8_draw_primitive(d3dr, D3DPT_TRIANGLESTRIP, 0, 2);
}

static bool d3d8_renderchain_render(
      d3d8_video_t *d3d,
      const void *frame,
      unsigned frame_width, unsigned frame_height,
      unsigned pitch, unsigned rotation)
{
   LPDIRECT3DDEVICE8 d3dr     = (LPDIRECT3DDEVICE8)d3d->dev;
   d3d8_renderchain_t *chain = (d3d8_renderchain_t*)d3d->renderchain_data;

   d3d8_renderchain_blit_to_texture(chain, frame, frame_width, frame_height, pitch);
   d3d8_renderchain_set_vertices(d3d, chain, 1, frame_width, frame_height, chain->frame_count);

   d3d8_renderchain_render_pass(d3d, d3dr, chain, 0, rotation);

   chain->frame_count++;

   return true;
}

static bool d3d8_renderchain_init(void *data,
      const void *_video_info,
      void *dev_data,
      const void *info_data,
      bool rgb32
      )
{
   unsigned width, height;
   d3d8_video_t *d3d                       = (d3d8_video_t*)data;
   LPDIRECT3DDEVICE8 d3dr                  = (LPDIRECT3DDEVICE8)d3d->dev;
   const video_info_t *video_info         = (const video_info_t*)_video_info;
   const struct LinkInfo *link_info       = (const struct LinkInfo*)info_data;
   d3d8_renderchain_t *chain              = (d3d8_renderchain_t*)d3d->renderchain_data;
   unsigned fmt                           = (rgb32) ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   struct video_viewport *custom_vp       = video_viewport_get_custom();

   video_driver_get_size(&width, &height);

   chain->dev                   = dev_data;
   chain->pixel_size            = (fmt == RETRO_PIXEL_FORMAT_RGB565) ? 2 : 4;
   chain->tex_w                 = link_info->tex_w;
   chain->tex_h                 = link_info->tex_h;

   if (!d3d8_renderchain_create_first_pass(d3d, chain, video_info))
      return false;

   /* FIXME */
   if (custom_vp->width == 0)
      custom_vp->width = width;

   if (custom_vp->height == 0)
      custom_vp->height = height;

   return true;
}

static void *d3d8_renderchain_new(void)
{
   d3d8_renderchain_t *renderchain = (d3d8_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   return renderchain;
}

static bool d3d8_init_chain(d3d8_video_t *d3d, const video_info_t *video_info)
{
   struct LinkInfo link_info;
   unsigned current_width, current_height, out_width, out_height;
   unsigned i                   = 0;

   (void)i;
   (void)current_width;
   (void)current_height;
   (void)out_width;
   (void)out_height;

   /* Setup information for first pass. */
   link_info.pass  = NULL;
   link_info.tex_w = video_info->input_scale * RARCH_SCALE_BASE;
   link_info.tex_h = video_info->input_scale * RARCH_SCALE_BASE;
   link_info.pass  = &d3d->shader.pass[0];

   d3d->renderchain_data = d3d8_renderchain_new();

   if (
         !d3d8_renderchain_init(
            d3d,
            &d3d->video_info,
            d3d->dev, &link_info,
            d3d->video_info.rgb32)
      )
   {
      RARCH_ERR("[D3D8]: Failed to init render chain.\n");
      return false;
   }

   RARCH_LOG("[D3D8]: Renderchain driver: %s\n", "d3d8");

   return true;
}

static bool d3d8_init_singlepass(d3d8_video_t *d3d)
{
   struct video_shader_pass *pass = NULL;

   if (!d3d)
      return false;

   memset(&d3d->shader, 0, sizeof(d3d->shader));
   d3d->shader.passes                    = 1;

   pass                                  = (struct video_shader_pass*)
      &d3d->shader.pass[0];

   pass->fbo.valid                       = true;
   pass->fbo.scale_y                     = 1.0;
   pass->fbo.type_y                      = RARCH_SCALE_VIEWPORT;
   pass->fbo.scale_x                     = pass->fbo.scale_y;
   pass->fbo.type_x                      = pass->fbo.type_y;

   if (!string_is_empty(d3d->shader_path))
      strlcpy(pass->source.path, d3d->shader_path,
            sizeof(pass->source.path));

   return true;
}

static void d3d8_viewport_info(void *data, struct video_viewport *vp)
{
   d3d8_video_t *d3d   = (d3d8_video_t*)data;

   if (d3d)
      d3d8_renderchain_viewport_info(d3d, vp);
}

static void d3d8_overlay_render(d3d8_video_t *d3d,
      video_frame_info_t *video_info,
      overlay_t *overlay, bool force_linear)
{
   D3DVIEWPORT8 vp_full;
   struct video_viewport vp;
   void *verts;
   unsigned i;
   Vertex vert[4];
   enum D3DTEXTUREFILTERTYPE filter_type = D3DTEXF_LINEAR;
   unsigned width                        = video_info->width;
   unsigned height                       = video_info->height;

   if (!d3d || !overlay || !overlay->tex)
      return;

   if (!overlay->vert_buf)
   {
      overlay->vert_buf = d3d8_vertex_buffer_new(
      d3d->dev, sizeof(vert), D3DUSAGE_WRITEONLY,
      D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
      D3DPOOL_MANAGED, NULL);

	  if (!overlay->vert_buf)
		  return;
   }

   for (i = 0; i < 4; i++)
   {
      vert[i].z    = 0.5f;
      vert[i].color   = (((uint32_t)(overlay->alpha_mod * 0xFF)) << 24) | 0xFFFFFF;
   }

   d3d8_viewport_info(d3d, &vp);

   vert[0].x      = overlay->vert_coords[0];
   vert[1].x      = overlay->vert_coords[0] + overlay->vert_coords[2];
   vert[2].x      = overlay->vert_coords[0];
   vert[3].x      = overlay->vert_coords[0] + overlay->vert_coords[2];
   vert[0].y      = overlay->vert_coords[1];
   vert[1].y      = overlay->vert_coords[1];
   vert[2].y      = overlay->vert_coords[1] + overlay->vert_coords[3];
   vert[3].y      = overlay->vert_coords[1] + overlay->vert_coords[3];

   vert[0].u      = overlay->tex_coords[0];
   vert[1].u      = overlay->tex_coords[0] + overlay->tex_coords[2];
   vert[2].u      = overlay->tex_coords[0];
   vert[3].u      = overlay->tex_coords[0] + overlay->tex_coords[2];
   vert[0].v      = overlay->tex_coords[1];
   vert[1].v      = overlay->tex_coords[1];
   vert[2].v      = overlay->tex_coords[1] + overlay->tex_coords[3];
   vert[3].v      = overlay->tex_coords[1] + overlay->tex_coords[3];

   verts = d3d8_vertex_buffer_lock(overlay->vert_buf);
   memcpy(verts, vert, sizeof(vert));
   d3d8_vertex_buffer_unlock(overlay->vert_buf);

   d3d8_enable_blend_func(d3d->dev);
   d3d8_set_vertex_shader(d3d->dev,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
         NULL);

   d3d8_set_stream_source(d3d->dev, 0, overlay->vert_buf,
         0, sizeof(*vert));

   if (overlay->fullscreen)
   {

      vp_full.X      = 0;
      vp_full.Y      = 0;
      vp_full.Width  = width;
      vp_full.Height = height;
      vp_full.MinZ   = 0.0f;
      vp_full.MaxZ   = 1.0f;
      d3d8_set_viewports(d3d->dev, &vp_full);
   }

   if (!force_linear)
   {
      settings_t *settings = config_get_ptr();
      if (!settings->bools.menu_linear_filter)
         filter_type       = D3DTEXF_POINT;
   }

   /* Render overlay. */
   d3d8_set_texture(d3d->dev, 0, overlay->tex);
   d3d8_set_sampler_address_u(d3d->dev, 0, D3DTADDRESS_BORDER);
   d3d8_set_sampler_address_v(d3d->dev, 0, D3DTADDRESS_BORDER);
   d3d8_set_sampler_minfilter(d3d->dev, 0, filter_type);
   d3d8_set_sampler_magfilter(d3d->dev, 0, filter_type);
   d3d8_draw_primitive(d3d->dev, D3DPT_TRIANGLESTRIP, 0, 2);

   /* Restore previous state. */
   d3d8_disable_blend_func(d3d->dev);

   d3d8_set_viewports(d3d->dev, &d3d->final_viewport);
}

static void d3d8_free_overlay(d3d8_video_t *d3d, overlay_t *overlay)
{
   if (!d3d)
      return;

   d3d8_texture_free(overlay->tex);
   d3d8_vertex_buffer_free(overlay->vert_buf, NULL);
}

static void d3d8_deinit_chain(d3d8_video_t *d3d)
{
   d3d8_renderchain_free(d3d->renderchain_data);

   d3d->renderchain_data   = NULL;
}

static void d3d8_deinitialize(d3d8_video_t *d3d)
{
   if (!d3d)
      return;

   font_driver_free_osd();

   d3d8_deinit_chain(d3d);
   d3d8_vertex_buffer_free(d3d->menu_display.buffer, d3d->menu_display.decl);
   d3d->menu_display.buffer = NULL;
   d3d->menu_display.decl = NULL;
}

#define FS_PRESENTINTERVAL(pp) ((pp)->FullScreen_PresentationInterval)

static D3DFORMAT d3d8_get_color_format_backbuffer(bool rgb32, bool windowed)
{
   D3DFORMAT fmt = D3DFMT_X8R8G8B8;
#ifdef _XBOX
   if (!rgb32)
      fmt        = d3d8_get_rgb565_format();
#else
   if (windowed)
   {
      D3DDISPLAYMODE display_mode;
      if (d3d8_get_adapter_display_mode(g_pD3D8, 0, &display_mode))
         fmt = display_mode.Format;
   }
#endif
   return fmt;
}

static bool d3d8_is_windowed_enable(bool info_fullscreen)
{
#ifndef _XBOX
   settings_t *settings = config_get_ptr();
   if (!info_fullscreen)
      return true;
   if (settings)
      return settings->bools.video_windowed_fullscreen;
#endif
   return false;
}

#ifdef _XBOX
static void d3d8_get_video_size(d3d8_video_t *d3d,
      unsigned *width, unsigned *height)
{
   DWORD video_mode      = XGetVideoFlags();

   *width                = 640;
   *height               = 480;
   d3d8_widescreen_mode  = false;

   /* Only valid in PAL mode, not valid for HDTV modes! */

   if(XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      /* Check for 16:9 mode (PAL REGION) */
      if(video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width = 720;
         //60 Hz, 720x480i
         if(video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
            *height = 480;
         else //50 Hz, 720x576i
            *height = 576;
         d3d8_widescreen_mode = true;
      }
   }
   else
   {
      /* Check for 16:9 mode (NTSC REGIONS) */
      if(video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width                    = 720;
         *height                   = 480;
         d3d8_widescreen_mode      = true;
      }
   }

   if(XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if(video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      {
         *width                    = 640;
         *height                   = 480;
         d3d8_widescreen_mode      = false;
         d3d->resolution_hd_enable = true;
      }
      else if(video_mode & XC_VIDEO_FLAGS_HDTV_720p)
      {
         *width                    = 1280;
         *height                   = 720;
         d3d8_widescreen_mode      = true;
         d3d->resolution_hd_enable = true;
      }
      else if(video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
      {
         *width                    = 1920;
         *height                   = 1080;
         d3d8_widescreen_mode      = true;
         d3d->resolution_hd_enable = true;
      }
   }
}
#endif

static void d3d8_make_d3dpp(void *data,
      const video_info_t *info, void *_d3dpp)
{
   d3d8_video_t *d3d               = (d3d8_video_t*)data;
   D3DPRESENT_PARAMETERS *d3dpp   = (D3DPRESENT_PARAMETERS*)_d3dpp;
   bool windowed_enable           = d3d8_is_windowed_enable(info->fullscreen);

   memset(d3dpp, 0, sizeof(*d3dpp));

   d3dpp->Windowed                = windowed_enable;
   FS_PRESENTINTERVAL(d3dpp)      = D3DPRESENT_INTERVAL_IMMEDIATE;

   if (info->vsync)
   {
      settings_t *settings        = config_get_ptr();

      switch (settings->uints.video_swap_interval)
      {
         default:
         case 1:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_ONE;
            break;
         case 2:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_TWO;
            break;
         case 3:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_THREE;
            break;
         case 4:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_FOUR;
            break;
      }
   }

   /* PresentationInterval must be zero for windowed mode on DX8. */
   if (d3dpp->Windowed)
      FS_PRESENTINTERVAL(d3dpp)   = D3DPRESENT_INTERVAL_DEFAULT;

   d3dpp->SwapEffect              = D3DSWAPEFFECT_DISCARD;
   d3dpp->BackBufferCount         = 2;
   d3dpp->BackBufferFormat        = d3d8_get_color_format_backbuffer(
         info->rgb32, windowed_enable);
#ifndef _XBOX
   d3dpp->hDeviceWindow           = win32_get_window();
#endif

   if (!windowed_enable)
   {
#ifdef _XBOX
      unsigned width              = 0;
      unsigned height             = 0;

      d3d8_get_video_size(d3d, &width, &height);
      video_driver_set_size(&width, &height);
#endif
      video_driver_get_size(&d3dpp->BackBufferWidth,
            &d3dpp->BackBufferHeight);
   }

#ifdef _XBOX
   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;
   {
      /* Get the "video mode" */
      DWORD video_mode            = XGetVideoFlags();

      /* Check if we are able to use progressive mode. */
      if (video_mode & XC_VIDEO_FLAGS_HDTV_480p)
         d3dpp->Flags = D3DPRESENTFLAG_PROGRESSIVE;
      else
         d3dpp->Flags = D3DPRESENTFLAG_INTERLACED;

      /* Only valid in PAL mode, not valid for HDTV modes. */
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

#if 0
      if (d3d8_widescreen_mode)
         d3dpp->Flags |= D3DPRESENTFLAG_WIDESCREEN;
#endif
   }
#endif
}

static bool d3d8_init_base(void *data, const video_info_t *info)
{
   D3DPRESENT_PARAMETERS d3dpp;
   HWND focus_window = NULL;
   d3d8_video_t *d3d  = (d3d8_video_t*)data;

#ifndef _XBOX
   focus_window      = win32_get_window();
#endif

   memset(&d3dpp, 0, sizeof(d3dpp));

   g_pD3D8            = (LPDIRECT3D8)d3d8_create();

   /* this needs g_pD3D created first */
   d3d8_make_d3dpp(d3d, info, &d3dpp);

   if (!g_pD3D8)
   {
      RARCH_ERR("[D3D8]: Failed to create D3D interface.\n");
      return false;
   }

   if (!d3d8_create_device(&d3d->dev, &d3dpp,
            g_pD3D8,
            focus_window,
            d3d->cur_mon_id)
      )
   {
      RARCH_ERR("[D3D8]: Failed to initialize device.\n");
      return false;
   }

   return true;
}

static void d3d8_calculate_rect(void *data,
      unsigned *width, unsigned *height,
      int *x, int *y,
      bool force_full,
      bool allow_rotate)
{
   float device_aspect  = (float)*width / *height;
   d3d8_video_t *d3d     = (d3d8_video_t*)data;
   settings_t *settings = config_get_ptr();

   video_driver_get_size(width, height);

   *x                   = 0;
   *y                   = 0;

   if (settings->bools.video_scale_integer && !force_full)
   {
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

      video_viewport_get_scaled_integer(&vp,
            *width,
            *height,
            video_driver_get_aspect_ratio(),
            d3d->keep_aspect);

      *x                          = vp.x;
      *y                          = vp.y;
      *width                      = vp.width;
      *height                     = vp.height;
   }
   else if (d3d->keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         video_viewport_t *custom = video_viewport_get_custom();

         *x          = custom->x;
         *y          = custom->y;
         *width      = custom->width;
         *height     = custom->height;
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta        = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            *x           = (int)(roundf(*width * (0.5f - delta)));
            *width       = (unsigned)(roundf(2.0f * (*width) * delta));
         }
         else
         {
            delta        = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            *y           = (int)(roundf(*height * (0.5f - delta)));
            *height      = (unsigned)(roundf(2.0f * (*height) * delta));
         }
      }
   }
}

static void d3d8_set_viewport(void *data,
      unsigned width, unsigned height,
      bool force_full,
      bool allow_rotate)
{
   struct d3d_matrix proj, ortho, rot, matrix;
   int x               = 0;
   int y               = 0;
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   d3d8_calculate_rect(data, &width, &height, &x, &y,
         force_full, allow_rotate);

   /* D3D doesn't support negative X/Y viewports ... */
   if (x < 0)
      x = 0;
   if (y < 0)
      y = 0;

   d3d->final_viewport.X      = x;
   d3d->final_viewport.Y      = y;
   d3d->final_viewport.Width  = width;
   d3d->final_viewport.Height = height;
   d3d->final_viewport.MinZ   = 0.0f;
   d3d->final_viewport.MaxZ   = 0.0f;

   d3d_matrix_ortho_off_center_lh(&ortho, 0, 1, 0, 1, 0.0f, 1.0f);
   d3d_matrix_identity(&rot);
   d3d_matrix_rotation_z(&rot, d3d->dev_rotation * (M_PI / 2.0));
   d3d_matrix_multiply(&proj, &ortho, &rot);
   d3d_matrix_transpose(&d3d->mvp, &ortho);
   d3d_matrix_transpose(&d3d->mvp_rotate, &matrix);
}

static bool d3d8_initialize(d3d8_video_t *d3d, const video_info_t *info)
{
   unsigned width, height;
   bool ret             = true;
   settings_t *settings = config_get_ptr();

   if (!d3d)
      return false;

   if (!g_pD3D8)
      ret = d3d8_init_base(d3d, info);
   else if (d3d->needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;

      d3d8_make_d3dpp(d3d, info, &d3dpp);

      /* the D3DX font driver uses POOL_DEFAULT resources
       * and will prevent a clean reset here
       * another approach would be to keep track of all created D3D
       * font objects and free/realloc them around the d3d_reset call  */
#ifdef HAVE_MENU
      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
#endif

      if (!d3d8_reset(d3d->dev, &d3dpp))
      {
         d3d8_deinitialize(d3d);
         d3d8_device_free(NULL, g_pD3D8);
         g_pD3D8 = NULL;

         ret = d3d8_init_base(d3d, info);
         if (ret)
            RARCH_LOG("[D3D8]: Recovered from dead state.\n");
      }

#ifdef HAVE_MENU
      menu_driver_init(info->is_threaded);
#endif
   }

   if (!ret)
      return ret;

   if (!d3d8_init_chain(d3d, info))
   {
      RARCH_ERR("[D3D8]: Failed to initialize render chain.\n");
      return false;
   }

   video_driver_get_size(&width, &height);
   d3d8_set_viewport(d3d,
	   width, height, false, true);

   font_driver_init_osd(d3d, false,
         info->is_threaded,
         FONT_DRIVER_RENDER_D3D8_API);

   d3d->menu_display.offset = 0;
   d3d->menu_display.size   = 1024;
   d3d->menu_display.buffer = d3d8_vertex_buffer_new(
         d3d->dev, d3d->menu_display.size * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
         D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
         D3DPOOL_DEFAULT,
         NULL);

   if (!d3d->menu_display.buffer)
      return false;

   d3d_matrix_ortho_off_center_lh(&d3d->mvp_transposed, 0, 1, 0, 1, 0, 1);
   d3d_matrix_transpose(&d3d->mvp, &d3d->mvp_transposed);

   d3d8_set_render_state(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);

   return true;
}

static bool d3d8_restore(void *data)
{
   d3d8_video_t            *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return false;

   d3d8_deinitialize(d3d);

   if (!d3d8_initialize(d3d, &d3d->video_info))
   {
      RARCH_ERR("[D3D8]: Restore error.\n");
      return false;
   }

   d3d->needs_restore = false;

   return true;
}

static void d3d8_set_nonblock_state(void *data, bool state)
{
   int      interval            = 0;
   d3d8_video_t            *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   if (!state)
      interval                  = 1;
   d3d->video_info.vsync        = !state;

#ifdef _XBOX
   d3d8_set_render_state(d3d->dev, D3D8_PRESENTATIONINTERVAL,
         interval ?
         D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE
         );
#else
   d3d->needs_restore = true;
   d3d8_restore(d3d);
#endif
}

static bool d3d8_set_resize(d3d8_video_t *d3d,
      unsigned new_width, unsigned new_height)
{
   /* No changes? */
   if (     (new_width  == d3d->video_info.width)
         && (new_height == d3d->video_info.height))
      return false;

   RARCH_LOG("[D3D8]: Resize %ux%u.\n", new_width, new_height);
   d3d->video_info.width  = new_width;
   d3d->video_info.height = new_height;
   video_driver_set_size(&new_width, &new_height);

   return true;
}

static bool d3d8_alive(void *data)
{
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool ret             = false;
   d3d8_video_t *d3d     = (d3d8_video_t*)data;
   bool        quit     = false;
   bool        resize   = false;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

#ifndef _XBOX
   win32_check_window(&quit, &resize, &temp_width, &temp_height);
#endif

   if (quit)
      d3d->quitting = quit;

   if (resize)
   {
      d3d->should_resize = true;
      d3d8_set_resize(d3d, temp_width, temp_height);
      d3d8_restore(d3d);
   }

   ret = !quit;

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return ret;
}

static bool d3d8_suppress_screensaver(void *data, bool enable)
{
#ifdef _XBOX
   return true;
#else
   return win32_suppress_screensaver(data, enable);
#endif
}

static void d3d8_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   d3d->keep_aspect   = true;
   d3d->should_resize = true;
}

static void d3d8_apply_state_changes(void *data)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   if (d3d)
      d3d->should_resize = true;
}

static void d3d8_set_osd_msg(void *data,
      video_frame_info_t *video_info,
      const char *msg,
      const void *params, void *font)
{
   d3d8_video_t          *d3d = (d3d8_video_t*)data;

   d3d8_begin_scene(d3d->dev);
   font_driver_render_msg(d3d, video_info, msg, params, font);
   d3d8_end_scene(d3d->dev);
}

static bool d3d8_init_internal(d3d8_video_t *d3d,
      const video_info_t *info, input_driver_t **input,
      void **input_data)
{
#ifdef HAVE_MONITOR
   bool windowed_full;
   RECT mon_rect;
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use;
#endif
#ifdef HAVE_WINDOW
   DWORD style;
   unsigned win_width        = 0;
   unsigned win_height       = 0;
   RECT rect                 = {0};
#endif
   unsigned full_x           = 0;
   unsigned full_y           = 0;
   settings_t    *settings   = config_get_ptr();
   overlay_t *menu           = (overlay_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return false;

   d3d->menu                 = menu;
   d3d->cur_mon_id           = 0;
   d3d->menu->tex_coords[0]  = 0;
   d3d->menu->tex_coords[1]  = 0;
   d3d->menu->tex_coords[2]  = 1;
   d3d->menu->tex_coords[3]  = 1;
   d3d->menu->vert_coords[0] = 0;
   d3d->menu->vert_coords[1] = 1;
   d3d->menu->vert_coords[2] = 1;
   d3d->menu->vert_coords[3] = -1;

#ifdef HAVE_WINDOW
   memset(&d3d->windowClass, 0, sizeof(d3d->windowClass));
   d3d->windowClass.lpfnWndProc = WndProcD3D;
   win32_window_init(&d3d->windowClass, true, NULL);
#endif

#ifdef HAVE_MONITOR
   win32_monitor_info(&current_mon, &hm_to_use, &d3d->cur_mon_id);

   mon_rect              = current_mon.rcMonitor;
   g_win32_resize_width  = info->width;
   g_win32_resize_height = info->height;

   windowed_full   = settings->bools.video_windowed_fullscreen;

   full_x          = (windowed_full || info->width  == 0) ?
      (mon_rect.right  - mon_rect.left) : info->width;
   full_y          = (windowed_full || info->height == 0) ?
      (mon_rect.bottom - mon_rect.top)  : info->height;

   RARCH_LOG("[D3D8]: Monitor size: %dx%d.\n",
         (int)(mon_rect.right  - mon_rect.left),
         (int)(mon_rect.bottom - mon_rect.top));
#else
   d3d8_get_video_size(d3d, &full_x, &full_y);
#endif
   {
      unsigned new_width  = info->fullscreen ? full_x : info->width;
      unsigned new_height = info->fullscreen ? full_y : info->height;
      video_driver_set_size(&new_width, &new_height);
   }

#ifdef HAVE_WINDOW
   video_driver_get_size(&win_width, &win_height);

   win32_set_style(&current_mon, &hm_to_use, &win_width, &win_height,
         info->fullscreen, windowed_full, &rect, &mon_rect, &style);

   win32_window_create(d3d, style, &mon_rect, win_width,
         win_height, info->fullscreen);

   win32_set_window(&win_width, &win_height, info->fullscreen,
	   windowed_full, &rect);
#endif

   if (!d3d8_init_singlepass(d3d))
      return false;

   d3d->video_info = *info;
   if (!d3d8_initialize(d3d, &d3d->video_info))
      return false;

   d3d_input_driver(settings->arrays.input_driver, settings->arrays.input_joypad_driver, input, input_data);

   RARCH_LOG("[D3D8]: Init complete.\n");
   return true;
}

static void d3d8_set_rotation(void *data, unsigned rot)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   d3d->dev_rotation  = rot;
   d3d->should_resize = true;
}

static void *d3d8_init(const video_info_t *info,
      input_driver_t **input, void **input_data)
{
   d3d8_video_t *d3d = (d3d8_video_t*)calloc(1, sizeof(*d3d));

   if (!d3d)
      return NULL;

   if (!d3d8_initialize_symbols(GFX_CTX_DIRECT3D8_API))
   {
      free(d3d);
      return NULL;
   }

#ifndef _XBOX
   win32_window_reset();
   win32_monitor_init();
#endif

   /* Default values */
   d3d->dev                  = NULL;
   d3d->dev_rotation         = 0;
   d3d->needs_restore        = false;
#ifdef HAVE_OVERLAY
   d3d->overlays_enabled     = false;
#endif
   d3d->should_resize        = false;
   d3d->menu                 = NULL;

   if (!d3d8_init_internal(d3d, info, input, input_data))
   {
      RARCH_ERR("[D3D8]: Failed to init D3D.\n");
      free(d3d);
      return NULL;
   }

   d3d->keep_aspect       = info->force_aspect;

   return d3d;
}

#ifdef HAVE_OVERLAY
static void d3d8_free_overlays(d3d8_video_t *d3d)
{
   unsigned i;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d8_free_overlay(d3d, &d3d->overlays[i]);
   free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
}
#endif

static void d3d8_free(void *data)
{
   d3d8_video_t   *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_OVERLAY
   d3d8_free_overlays(d3d);
   if (d3d->overlays)
      free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
#endif

   d3d8_free_overlay(d3d, d3d->menu);
   if (d3d->menu)
      free(d3d->menu);
   d3d->menu          = NULL;

   d3d8_deinitialize(d3d);

   if (!string_is_empty(d3d->shader_path))
      free(d3d->shader_path);

   d3d->shader_path = NULL;
   d3d8_device_free(d3d->dev, g_pD3D8);
   d3d->dev         = NULL;
   g_pD3D8          = NULL;

   d3d8_deinitialize_symbols();

#ifndef _XBOX
   win32_monitor_from_window();
   win32_destroy_window();
#endif
   free(d3d);
}

#ifdef HAVE_OVERLAY
static void d3d8_overlay_tex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   if (!d3d)
      return;

   d3d->overlays[index].tex_coords[0] = x;
   d3d->overlays[index].tex_coords[1] = y;
   d3d->overlays[index].tex_coords[2] = w;
   d3d->overlays[index].tex_coords[3] = h;
#ifdef _XBOX
   d3d->overlays[index].tex_coords[0] *= d3d->overlays[index].tex_w;
   d3d->overlays[index].tex_coords[1] *= d3d->overlays[index].tex_h;
   d3d->overlays[index].tex_coords[2] *= d3d->overlays[index].tex_w;
   d3d->overlays[index].tex_coords[3] *= d3d->overlays[index].tex_h;
#endif
}

static void d3d8_overlay_vertex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   if (!d3d)
      return;

   y                                   = 1.0f - y;
   h                                   = -h;
   d3d->overlays[index].vert_coords[0] = x;
   d3d->overlays[index].vert_coords[1] = y;
   d3d->overlays[index].vert_coords[2] = w;
   d3d->overlays[index].vert_coords[3] = h;
}

static bool d3d8_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, y;
   overlay_t *new_overlays            = NULL;
   d3d8_video_t *d3d                   = (d3d8_video_t*)data;
   const struct texture_image *images = (const struct texture_image*)
      image_data;

   if (!d3d)
	   return false;

   d3d8_free_overlays(d3d);
   d3d->overlays      = (overlay_t*)calloc(num_images, sizeof(*d3d->overlays));
   d3d->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      D3DLOCKED_RECT d3dlr;
      unsigned width     = images[i].width;
      unsigned height    = images[i].height;
      overlay_t *overlay = (overlay_t*)&d3d->overlays[i];

      overlay->tex       = d3d8_texture_new(d3d->dev, NULL,
                  width, height, 1,
                  0,
                  d3d8_get_argb8888_format(),
                  D3DPOOL_MANAGED, 0, 0, 0,
                  NULL, NULL, false);

      if (!overlay->tex)
      {
         RARCH_ERR("[D3D8]: Failed to create overlay texture\n");
         return false;
      }

      if (d3d8_lock_rectangle(overlay->tex, 0, &d3dlr,
               NULL, 0, D3DLOCK_NOSYSLOCK))
      {
         uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
         const uint32_t *src = images[i].pixels;
         unsigned      pitch = d3dlr.Pitch >> 2;

         for (y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
         d3d8_unlock_rectangle(overlay->tex);
      }

      overlay->tex_w         = width;
      overlay->tex_h         = height;

      /* Default. Stretch to whole screen. */
      d3d8_overlay_tex_geom(d3d, i, 0, 0, 1, 1);
      d3d8_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d8_overlay_enable(void *data, bool state)
{
   unsigned i;
   d3d8_video_t            *d3d = (d3d8_video_t*)data;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays_enabled = state;

#ifndef _XBOX
   win32_show_cursor(d3d, state);
#endif
}

static void d3d8_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d8_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;
   if (d3d)
      d3d->overlays[index].alpha_mod = mod;
}

static const video_overlay_interface_t d3d8_overlay_interface = {
   d3d8_overlay_enable,
   d3d8_overlay_load,
   d3d8_overlay_tex_geom,
   d3d8_overlay_vertex_geom,
   d3d8_overlay_full_screen,
   d3d8_overlay_set_alpha,
};

static void d3d8_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   (void)data;
   *iface = &d3d8_overlay_interface;
}
#endif

static void d3d8_update_title(video_frame_info_t *video_info)
{
#ifndef _XBOX
   const ui_window_t *window      = ui_companion_driver_get_window_ptr();

   if (window)
   {
      char title[128];

      title[0] = '\0';

      video_driver_get_window_title(title, sizeof(title));

      if (title[0])
         window->set_title(&main_window, title);
   }
#endif
}

static bool d3d8_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   D3DVIEWPORT8 screen_vp;
   unsigned i                          = 0;
   d3d8_video_t *d3d                    = (d3d8_video_t*)data;
   unsigned width                      = video_info->width;
   unsigned height                     = video_info->height;
   (void)i;

   if (!frame)
      return true;

   /* We cannot recover in fullscreen. */
   if (d3d->needs_restore)
   {
#ifndef _XBOX
      HWND window = win32_get_window();
      if (IsIconic(window))
         return true;
#endif

      if (!d3d8_restore(d3d))
      {
         RARCH_ERR("[D3D8]: Failed to restore.\n");
         return false;
      }
   }

   if (d3d->should_resize)
   {
      d3d8_set_viewport(d3d, width, height, false, true);
      d3d->should_resize = false;
   }

   /* render_chain() only clears out viewport,
    * clear out everything. */
   screen_vp.X      = 0;
   screen_vp.Y      = 0;
   screen_vp.MinZ   = 0;
   screen_vp.MaxZ   = 1;
   screen_vp.Width  = width;
   screen_vp.Height = height;
   d3d8_set_viewports(d3d->dev, &screen_vp);
   d3d8_clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);

   /* Insert black frame first, so we
    * can screenshot, etc. */
   if (video_info->black_frame_insertion)
   {
      if (!d3d8_swap(d3d, d3d->dev) || d3d->needs_restore)
         return true;
      d3d8_clear(d3d->dev, 0, 0, D3DCLEAR_TARGET, 0, 1, 0);
   }

   if (!d3d8_renderchain_render(
            d3d,
            frame, frame_width, frame_height,
            pitch, d3d->dev_rotation))
   {
      RARCH_ERR("[D3D8]: Failed to render scene.\n");
      return false;
   }

#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      d3d8_set_mvp(d3d->dev, &d3d->mvp);
      d3d8_overlay_render(d3d, video_info, d3d->menu, false);

      d3d->menu_display.offset = 0;
      d3d8_set_stream_source(d3d->dev, 0, d3d->menu_display.buffer, 0, sizeof(Vertex));

      d3d8_set_viewports(d3d->dev, &screen_vp);
      menu_driver_frame(video_info);
   }
   else if (video_info->statistics_show)
   {
      struct font_params *osd_params = (struct font_params*)
         &video_info->osd_stat_params;

      if (osd_params)
         font_driver_render_msg(d3d, video_info, video_info->stat_text,
               (const struct font_params*)&video_info->osd_stat_params, NULL);
   }
#endif

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled)
   {
      d3d8_set_mvp(d3d->dev, &d3d->mvp);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d8_overlay_render(d3d, video_info, &d3d->overlays[i], true);
   }
#endif

   if (!string_is_empty(msg))
   {
      d3d8_set_viewports(d3d->dev, &screen_vp);
      d3d8_begin_scene(d3d->dev);
      font_driver_render_msg(d3d, video_info, msg, NULL, NULL);
      d3d8_end_scene(d3d->dev);
   }

   d3d8_update_title(video_info);
   d3d8_swap(d3d, d3d->dev);

   return true;
}

static bool d3d8_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   return false;
}

static void d3d8_set_menu_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   D3DLOCKED_RECT d3dlr;
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   (void)d3dlr;
   (void)frame;
   (void)rgb32;
   (void)width;
   (void)height;
   (void)alpha;

   if (    !d3d->menu->tex            ||
            d3d->menu->tex_w != width ||
            d3d->menu->tex_h != height)
   {
      if (d3d->menu)
	     d3d8_texture_free(d3d->menu->tex);

      d3d->menu->tex = d3d8_texture_new(d3d->dev, NULL,
            width, height, 1,
            0, d3d8_get_argb8888_format(),
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!d3d->menu->tex)
      {
         RARCH_ERR("[D3D8]: Failed to create menu texture.\n");
         return;
      }

      d3d->menu->tex_w          = width;
      d3d->menu->tex_h          = height;
#ifdef _XBOX
      d3d->menu->tex_coords [2] = width;
      d3d->menu->tex_coords[3]  = height;
#endif
   }

   d3d->menu->alpha_mod = alpha;

   if (d3d8_lock_rectangle(d3d->menu->tex, 0, &d3dlr,
            NULL, 0, D3DLOCK_NOSYSLOCK))
   {
      unsigned h, w;
      if (rgb32)
      {
         uint8_t        *dst = (uint8_t*)d3dlr.pBits;
         const uint32_t *src = (const uint32_t*)frame;

         for (h = 0; h < height; h++, dst += d3dlr.Pitch, src += width)
         {
            memcpy(dst, src, width * sizeof(uint32_t));
            memset(dst + width * sizeof(uint32_t), 0,
                  d3dlr.Pitch - width * sizeof(uint32_t));
         }
      }
      else
      {
         uint32_t       *dst = (uint32_t*)d3dlr.pBits;
         const uint16_t *src = (const uint16_t*)frame;

         for (h = 0; h < height; h++, dst += d3dlr.Pitch >> 2, src += width)
         {
            for (w = 0; w < width; w++)
            {
               uint16_t c = src[w];
               uint32_t r = (c >> 12) & 0xf;
               uint32_t g = (c >>  8) & 0xf;
               uint32_t b = (c >>  4) & 0xf;
               uint32_t a = (c >>  0) & 0xf;
               r          = ((r << 4) | r) << 16;
               g          = ((g << 4) | g) <<  8;
               b          = ((b << 4) | b) <<  0;
               a          = ((a << 4) | a) << 24;
               dst[w]     = r | g | b | a;
            }
         }
      }

      if (d3d->menu)
         d3d8_unlock_rectangle(d3d->menu->tex);
   }
}

static void d3d8_set_menu_texture_enable(void *data,
      bool state, bool full_screen)
{
   d3d8_video_t *d3d = (d3d8_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   d3d->menu->enabled            = state;
   d3d->menu->fullscreen         = full_screen;
}

struct d3d8_texture_info
{
   void *userdata;
   void *data;
   enum texture_filter_type type;
};

static void d3d8_video_texture_load_d3d(
      struct d3d8_texture_info *info,
      uintptr_t *id)
{
   D3DLOCKED_RECT d3dlr;
   unsigned usage           = 0;
   d3d8_video_t *d3d         = (d3d8_video_t*)info->userdata;
   struct texture_image *ti = (struct texture_image*)info->data;
   LPDIRECT3DTEXTURE8 tex   = (LPDIRECT3DTEXTURE8)d3d8_texture_new(d3d->dev, NULL,
               ti->width, ti->height, 0,
               usage, d3d8_get_argb8888_format(),
               D3DPOOL_MANAGED, 0, 0, 0,
               NULL, NULL, false);

   if (!tex)
   {
      RARCH_ERR("[D3D8]: Failed to create texture\n");
      return;
   }

   if (d3d8_lock_rectangle(tex, 0, &d3dlr,
            NULL, 0, D3DLOCK_NOSYSLOCK))
   {
      unsigned i;
      uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
      const uint32_t *src = ti->pixels;
      unsigned      pitch = d3dlr.Pitch >> 2;

      for (i = 0; i < ti->height; i++, dst += pitch, src += ti->width)
         memcpy(dst, src, ti->width << 2);
      d3d8_unlock_rectangle(tex);
   }

   *id = (uintptr_t)tex;
}

static int d3d8_video_texture_load_wrap_d3d(void *data)
{
   uintptr_t id = 0;
   struct d3d8_texture_info *info = (struct d3d8_texture_info*)data;
   if (!info)
      return 0;
   d3d8_video_texture_load_d3d(info, &id);
   return id;
}

static uintptr_t d3d8_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   struct d3d8_texture_info info;
   uintptr_t id = 0;

   info.userdata = video_data;
   info.data     = data;
   info.type     = filter_type;

   if (threaded)
      return video_thread_texture_load(&info,
            d3d8_video_texture_load_wrap_d3d);

   d3d8_video_texture_load_d3d(&info, &id);
   return id;
}

static void d3d8_unload_texture(void *data, uintptr_t id)
{
   LPDIRECT3DTEXTURE8 texid;
   if (!id)
	   return;

   texid = (LPDIRECT3DTEXTURE8)id;
   d3d8_texture_free(texid);
}

static void d3d8_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifndef _XBOX
   win32_show_cursor(data, !fullscreen);
#endif
}

static uint32_t d3d8_get_flags(void *data)
{
   uint32_t             flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);

   return flags;
}

static const video_poke_interface_t d3d_poke_interface = {
   d3d8_get_flags,
   d3d8_load_texture,
   d3d8_unload_texture,
   d3d8_set_video_mode,
#if defined(_XBOX) || defined(__WINRT__)
   NULL,
#else
   /* UWP does not expose this information easily */
   win32_get_refresh_rate,
#endif
   NULL,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d8_set_aspect_ratio,
   d3d8_apply_state_changes,
   d3d8_set_menu_texture_frame,
   d3d8_set_menu_texture_enable,
   d3d8_set_osd_msg,

   win32_show_cursor,
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL                          /* get_hw_render_interface */
};

static void d3d8_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &d3d_poke_interface;
}

static bool d3d8_has_windowed(void *data)
{
#ifdef _XBOX
   return false;
#else
   return true;
#endif
}

video_driver_t video_d3d8 = {
   d3d8_init,
   d3d8_frame,
   d3d8_set_nonblock_state,
   d3d8_alive,
   NULL,                      /* focus */
   d3d8_suppress_screensaver,
   d3d8_has_windowed,
   d3d8_set_shader,
   d3d8_free,
   "d3d8",
   d3d8_set_viewport,
   d3d8_set_rotation,
   d3d8_viewport_info,
   NULL,                      /* read_viewport  */
   NULL,                      /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d8_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   d3d8_get_poke_interface
};
