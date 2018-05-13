/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <string.h>
#include <retro_inline.h>
#include <retro_math.h>

#include <d3d9.h>

#include "../drivers/d3d.h"
#include "../../defines/d3d_defines.h"
#include "../common/d3d_common.h"
#include "../common/d3d9_common.h"

#include "../video_driver.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct hlsl_d3d9_renderchain
{
   unsigned pixel_size;
   uint64_t frame_count;
   unsigned last_width, last_height;
   unsigned tex_w;
   unsigned tex_h;
   LPDIRECT3DDEVICE9 dev;
   LPDIRECT3DTEXTURE9 tex;
   LPDIRECT3DVERTEXBUFFER9 vertex_buf;
   LPDIRECT3DVERTEXDECLARATION9 vertex_decl;
} hlsl_d3d9_renderchain_t;

static void hlsl_d3d9_renderchain_clear(hlsl_d3d9_renderchain_t *chain)
{
   d3d9_texture_free(chain->tex);
   d3d9_vertex_buffer_free(chain->vertex_buf, chain->vertex_decl);
}

static bool hlsl_d3d9_renderchain_init_shader_fvf(d3d9_video_t *d3d,
      hlsl_d3d9_renderchain_t *chain)
{
   static const D3DVERTEXELEMENT9 VertexElements[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      D3DDECL_END()
   };

   return d3d9_vertex_declaration_new(d3d->dev,
         VertexElements, (void**)&chain->vertex_decl);
}

static bool hlsl_d3d9_renderchain_create_first_pass(d3d9_video_t *d3d,
      const video_info_t *info)
{
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)
      d3d->renderchain_data;

   chain->vertex_buf        = d3d9_vertex_buffer_new(
         d3d->dev, 4 * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
#ifdef _XBOX
		 0,
#else
         D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1,
#endif
         D3DPOOL_MANAGED,
         NULL);

   if (!chain->vertex_buf)
      return false;

   chain->tex = d3d9_texture_new(d3d->dev, NULL,
         chain->tex_w, chain->tex_h, 1, 0,
         info->rgb32 ? 
         d3d9_get_xrgb8888_format() : d3d9_get_rgb565_format(),
         0, 0, 0, 0, NULL, NULL, false);

   if (!chain->tex)
      return false;

   d3d9_set_sampler_address_u(d3d->dev, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d9_set_sampler_address_v(d3d->dev, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   d3d9_set_render_state(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);
   d3d9_set_render_state(d3d->dev, D3DRS_ZENABLE, FALSE);

   if (!hlsl_d3d9_renderchain_init_shader_fvf(d3d, chain))
      return false;

   return true;
}

static void hlsl_d3d9_renderchain_set_vertices(
      d3d9_video_t *d3d,
      hlsl_d3d9_renderchain_t *chain,
      unsigned pass,
      unsigned vert_width, unsigned vert_height,
      uint64_t frame_count)
{
   video_shader_ctx_params_t params;
   video_shader_ctx_info_t shader_info;
   unsigned width, height;

   video_driver_get_size(&width, &height);

   if (chain->last_width != vert_width || chain->last_height != vert_height)
   {
      unsigned i;
      Vertex vert[4];
      float tex_w      = 0.0f;
      float tex_h      = 0.0f;
      void *verts      = NULL;

      chain->last_width  = vert_width;
      chain->last_height = vert_height;

      tex_w              = vert_width  / ((float)chain->tex_w);
      tex_h              = vert_height / ((float)chain->tex_h);

      vert[0].x          = -1.0f;
      vert[0].y          = -1.0f;
      vert[0].u          = 0.0f;
      vert[0].v          = tex_h;

      vert[1].x          =  1.0f;
      vert[1].y          = -1.0f;
      vert[1].u          = tex_w;
      vert[1].v          = tex_h;

      vert[2].x          = -1.0f;
      vert[2].y          =  1.0f;
      vert[2].u          = 0.0f;
      vert[2].v          = 0.0f;

      vert[3].x          =  1.0f;
      vert[3].y          =  1.0f;
      vert[3].u          = tex_w;
      vert[3].v          = 0.0f;

      /* Align texels and vertices. */
      for (i = 0; i < 4; i++)
      {
         vert[i].x      -= 0.5f / ((float)chain->tex_w);
         vert[i].y      += 0.5f / ((float)chain->tex_h);
      }

      verts = d3d9_vertex_buffer_lock(chain->vertex_buf);
      memcpy(verts, vert, sizeof(vert));
      d3d9_vertex_buffer_unlock(chain->vertex_buf);
   }

   shader_info.data = d3d;
   shader_info.idx  = pass;
   shader_info.set_active = true;

   video_shader_driver_use(&shader_info);

   params.data          = d3d;
   params.width         = vert_width;
   params.height        = vert_height;
   params.tex_width     = chain->tex_w;
   params.tex_height    = chain->tex_h;
   params.out_width     = width;
   params.out_height    = height;
   params.frame_counter = (unsigned int)frame_count;
   params.info          = NULL;
   params.prev_info     = NULL;
   params.feedback_info = NULL;
   params.fbo_info      = NULL;
   params.fbo_info_cnt  = 0;

   video_shader_driver_set_parameters(&params);
}

static void hlsl_d3d9_renderchain_blit_to_texture(
      hlsl_d3d9_renderchain_t *chain, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   D3DLOCKED_RECT d3dlr           = { 0, NULL };

   if (chain->last_width != width || chain->last_height != height)
   {
      d3d9_lock_rectangle(chain->tex,
            0, &d3dlr, NULL, chain->tex_h, D3DLOCK_NOSYSLOCK);
      d3d9_lock_rectangle_clear(chain->tex,
            0, &d3dlr, NULL, chain->tex_h, D3DLOCK_NOSYSLOCK);
   }

   /* Set the texture to NULL so D3D doesn't complain about it being in use... */
   d3d9_set_texture(chain->dev, 0, NULL);

   if (d3d9_lock_rectangle(chain->tex, 0, &d3dlr, NULL, 0, 0))
   {
      d3d9_texture_blit(chain->pixel_size, chain->tex,
            &d3dlr, frame, width, height, pitch);
      d3d9_unlock_rectangle(chain->tex);
   }
}

static void hlsl_d3d9_renderchain_free(void *data)
{
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)data;

   if (!chain)
      return;

   hlsl_d3d9_renderchain_clear(chain);
   free(chain);
}

void *hlsl_d3d9_renderchain_new(void)
{
   hlsl_d3d9_renderchain_t *renderchain =
      (hlsl_d3d9_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   return renderchain;
}

static bool hlsl_d3d9_renderchain_init_shader(d3d9_video_t *d3d,
      hlsl_d3d9_renderchain_t *chain)
{
   video_shader_ctx_init_t init;

   init.shader_type               = RARCH_SHADER_HLSL;
   init.data                      = d3d;
   init.path                      = retroarch_get_shader_preset();
   init.shader                    = NULL;

   RARCH_LOG("D3D]: Using HLSL shader backend.\n");

   return video_shader_driver_init(&init);
}

static bool hlsl_d3d9_renderchain_init(
      d3d9_video_t *d3d,
      const video_info_t *video_info,
      LPDIRECT3DDEVICE9 dev,
      const D3DVIEWPORT9 *final_viewport,
      const struct LinkInfo *info,
      bool rgb32
      )
{
   unsigned width, height;
   hlsl_d3d9_renderchain_t *chain     = (hlsl_d3d9_renderchain_t*)
      d3d->renderchain_data;
   unsigned fmt                       = (rgb32)
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   struct video_viewport *custom_vp   = video_viewport_get_custom();

   if (!hlsl_d3d9_renderchain_init_shader(d3d, chain))
      return false;

   video_driver_get_size(&width, &height);

   chain->dev                         = dev;
   chain->pixel_size                  = (fmt == RETRO_PIXEL_FORMAT_RGB565) ? 2 : 4;
   chain->tex_w                       = info->tex_w;
   chain->tex_h                       = info->tex_h;

   if (!hlsl_d3d9_renderchain_create_first_pass(d3d, video_info))
      return false;

   /* FIXME */
   if (custom_vp->width == 0)
      custom_vp->width = width;

   if (custom_vp->height == 0)
      custom_vp->height = height;

   return true;
}

static void hlsl_d3d9_renderchain_set_final_viewport(
      d3d9_video_t *d3d,
      void *renderchain_data, const D3DVIEWPORT9 *final_viewport)
{
}

static bool hlsl_d3d9_renderchain_render(
      d3d9_video_t *d3d,
      const void *frame,
      unsigned frame_width, unsigned frame_height,
      unsigned pitch, unsigned rotation)
{
   unsigned i;
   unsigned width, height;
   settings_t *settings           = config_get_ptr();
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)
      d3d->renderchain_data;
   bool video_smooth              = settings->bools.video_smooth;

   chain->frame_count++;

   video_driver_get_size(&width, &height);

   hlsl_d3d9_renderchain_blit_to_texture(chain,
         frame, frame_width, frame_height, pitch);
   hlsl_d3d9_renderchain_set_vertices(d3d, chain,
         1, frame_width, frame_height, chain->frame_count);

   d3d9_set_texture(chain->dev, 0, chain->tex);
   d3d9_set_viewports(chain->dev, &d3d->final_viewport);
   d3d9_set_sampler_minfilter(chain->dev, 0,
         video_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d9_set_sampler_magfilter(chain->dev, 0,
         video_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);

   d3d9_set_vertex_declaration(chain->dev, chain->vertex_decl);
   for (i = 0; i < 4; i++)
      d3d9_set_stream_source(chain->dev, i,
            chain->vertex_buf, 0, sizeof(Vertex));
   d3d9_draw_primitive(chain->dev, D3DPT_TRIANGLESTRIP, 0, 2);

   return true;
}

static bool hlsl_d3d9_renderchain_add_pass(
      void *data, const struct LinkInfo *info)
{
   (void)data;

   /* stub */
   return true;
}

static void hlsl_d3d9_renderchain_add_state_tracker(
      void *data, void *tracker_data)
{
   (void)data;
   (void)tracker_data;

   /* stub */
}

static void hlsl_d3d9_renderchain_convert_geometry(
	  void *data, const void *info_data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height,
      void *final_viewport_data)
{
   (void)data;
   (void)info_data;
   (void)out_width;
   (void)out_height;
   (void)width;
   (void)height;
   (void)final_viewport_data;

   /* stub */
}

d3d9_renderchain_driver_t hlsl_d3d9_renderchain = {
   hlsl_d3d9_renderchain_free,
   hlsl_d3d9_renderchain_new,
   hlsl_d3d9_renderchain_init,
   hlsl_d3d9_renderchain_set_final_viewport,
   hlsl_d3d9_renderchain_add_pass,
   NULL,
   hlsl_d3d9_renderchain_add_state_tracker,
   hlsl_d3d9_renderchain_render,
   hlsl_d3d9_renderchain_convert_geometry,
   NULL,
   NULL,
   "hlsl_d3d9",
};
