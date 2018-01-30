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

#include <string.h>
#include <retro_inline.h>
#include <retro_math.h>

#include <d3d9.h>

#include "../drivers/d3d.h"
#include "../../defines/d3d_defines.h"
#include "../common/d3d_common.h"

#include "../video_driver.h"

#include "../../configuration.h"
#include "../../verbosity.h"

typedef struct hlsl_d3d9_renderchain
{
   unsigned pixel_size;
   unsigned last_width;
   unsigned last_height;
   unsigned tex_w;
   unsigned tex_h;
   uint64_t frame_count;
   void *dev;
   LPDIRECT3DTEXTURE9 tex;
   LPDIRECT3DVERTEXBUFFER9 vertex_buf;
   LPDIRECT3DVERTEXDECLARATION9 vertex_decl;
} hlsl_d3d9_renderchain_t;

/* TODO/FIXME - this forward declaration should not be necesary */
void hlsl_set_proj_matrix(void *data, void *matrix_data);

static void hlsl_d3d9_renderchain_set_mvp(
      void *data,
      void *chain_data,
      void *shader_data,
      const void *mat_data)
{
   d3d_video_t      *d3d = (d3d_video_t*)data;

   if(shader_data)
      hlsl_set_proj_matrix(shader_data, (void*)mat_data);
   else
      hlsl_set_proj_matrix((void*)&d3d->shader, (void*)mat_data);
}

static void hlsl_d3d9_renderchain_clear(void *data)
{
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)data;

   d3d_texture_free(chain->tex);
   d3d_vertex_buffer_free(chain->vertex_buf, chain->vertex_decl);
}

static bool hlsl_d3d9_renderchain_init_shader_fvf(void *data, void *pass_data)
{
   static const D3DVERTEXELEMENT9 VertexElements[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      { 0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
      D3DDECL_END()
   };
   d3d_video_t *d3d               = (d3d_video_t*)data;
   d3d_video_t *pass              = (d3d_video_t*)data;
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)
      d3d->renderchain_data;

   (void)pass_data;

   return d3d_vertex_declaration_new(d3d->dev,
         VertexElements, (void**)&chain->vertex_decl);
}

static bool hlsl_d3d9_renderchain_create_first_pass(void *data,
      const video_info_t *info)
{
   d3d_video_t       *d3d         = (d3d_video_t*)data;
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)
      d3d->renderchain_data;

   chain->vertex_buf        = d3d_vertex_buffer_new(
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

   chain->tex = d3d_texture_new(d3d->dev, NULL,
         chain->tex_w, chain->tex_h, 1, 0,
         info->rgb32 ? 
         d3d_get_xrgb8888_format() : d3d_get_rgb565_format(),
         0, 0, 0, 0, NULL, NULL, false);

   if (!chain->tex)
      return false;

   d3d_set_sampler_address_u(d3d->dev, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d_set_sampler_address_v(d3d->dev, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   d3d_set_render_state(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);
   d3d_set_render_state(d3d->dev, D3DRS_ZENABLE, FALSE);

   if (!hlsl_d3d9_renderchain_init_shader_fvf(chain, chain))
      return false;

   return true;
}

static void hlsl_d3d9_renderchain_set_vertices(
      void *data, unsigned pass,
      unsigned vert_width, unsigned vert_height, uint64_t frame_count)
{
   video_shader_ctx_params_t params;
   video_shader_ctx_info_t shader_info;
   unsigned width, height;
   d3d_video_t *d3d         = (d3d_video_t*)data;
   hlsl_d3d9_renderchain_t *chain = d3d ?
      (hlsl_d3d9_renderchain_t*)d3d->renderchain_data : NULL;

   video_driver_get_size(&width, &height);

   if (!chain)
      return;

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

      verts = d3d_vertex_buffer_lock(chain->vertex_buf);
      memcpy(verts, vert, sizeof(vert));
      d3d_vertex_buffer_unlock(chain->vertex_buf);
   }

   /* TODO/FIXME - last parameter is mat_data, should be set to
   something other than NULL */
   hlsl_d3d9_renderchain_set_mvp(d3d, chain, &d3d->shader,
         NULL);

   shader_info.data = d3d;
   shader_info.idx  = pass;
   shader_info.set_active = true;

   video_shader_driver_use(shader_info);

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

   video_shader_driver_set_parameters(params);
}

static void hlsl_d3d9_renderchain_blit_to_texture(
      void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   D3DLOCKED_RECT d3dlr;
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)data;

   d3d_frame_postprocess(chain);

   if (chain->last_width != width || chain->last_height != height)
   {
      d3d_lock_rectangle(chain->tex,
            0, &d3dlr, NULL, chain->tex_h, D3DLOCK_NOSYSLOCK);
      d3d_lock_rectangle_clear(chain->tex,
            0, &d3dlr, NULL, chain->tex_h, D3DLOCK_NOSYSLOCK);
   }

   /* Set the texture to NULL so D3D doesn't complain about it being in use... */
   d3d_set_texture(chain->dev, 0, NULL);

   if (d3d_lock_rectangle(chain->tex, 0, &d3dlr, NULL, 0, 0))
   {
      d3d_texture_blit(chain->pixel_size, chain->tex,
            &d3dlr, frame, width, height, pitch);
      d3d_unlock_rectangle(chain->tex);
   }
}

static void hlsl_d3d9_renderchain_deinit(void *data)
{
   hlsl_d3d9_renderchain_t *renderchain = (hlsl_d3d9_renderchain_t*)data;

   if (renderchain)
      free(renderchain);
}

static void hlsl_d3d9_renderchain_deinit_shader(void *data)
{
   (void)data;
   /* stub */
}

static void hlsl_d3d9_renderchain_free(void *data)
{
   d3d_video_t *chain = (d3d_video_t*)data;

   if (!chain)
      return;

   hlsl_d3d9_renderchain_deinit_shader(chain);
   hlsl_d3d9_renderchain_deinit(chain->renderchain_data);
   hlsl_d3d9_renderchain_clear(chain->renderchain_data);
}

void *hlsl_d3d9_renderchain_new(void)
{
   hlsl_d3d9_renderchain_t *renderchain =
      (hlsl_d3d9_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   return renderchain;
}

static bool hlsl_d3d9_renderchain_init_shader(void *data,
      void *renderchain_data)
{
   video_shader_ctx_init_t init;
   bool ret                       = false;
   d3d_video_t        *d3d        = (d3d_video_t*)data;
   settings_t *settings           = config_get_ptr();
   (void)renderchain_data;

   if (!d3d)
      return false;

   init.shader_type               = RARCH_SHADER_HLSL;
   init.data                      = data;
   init.path                      = retroarch_get_shader_preset();
   init.shader                    = &hlsl_backend;

   RARCH_LOG("D3D]: Using HLSL shader backend.\n");

   ret = video_shader_driver_init(&init);

   return ret;
}

static bool hlsl_d3d9_renderchain_init(void *data,
      const void *_video_info,
      void *dev_data,
      const void *final_viewport_data,
      const void *info_data,
      bool rgb32
      )
{
   unsigned width, height;
   d3d_video_t *d3d                   = (d3d_video_t*)data;
   const video_info_t *video_info     = (const video_info_t*)_video_info;
   const struct LinkInfo *link_info   = (const struct LinkInfo*)info_data;
   hlsl_d3d9_renderchain_t *chain     = (hlsl_d3d9_renderchain_t*)
      d3d->renderchain_data;
   unsigned fmt                       = (rgb32)
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   struct video_viewport *custom_vp   = video_viewport_get_custom();

   (void)final_viewport_data;

   if (!hlsl_d3d9_renderchain_init_shader(d3d, NULL))
      return false;

   video_driver_get_size(&width, &height);

   chain->dev                         = (LPDIRECT3DDEVICE9)dev_data;
   chain->pixel_size                  = (fmt == RETRO_PIXEL_FORMAT_RGB565) ? 2 : 4;
   chain->tex_w                       = link_info->tex_w;
   chain->tex_h                       = link_info->tex_h;

   if (!hlsl_d3d9_renderchain_create_first_pass(d3d, video_info))
      return false;

   /* FIXME */
   if (custom_vp->width == 0)
      custom_vp->width = width;

   if (custom_vp->height == 0)
      custom_vp->height = height;

   return true;
}

static void hlsl_d3d9_renderchain_set_final_viewport(void *data,
      void *renderchain_data, const void *viewport_data)
{
   (void)data;
   (void)renderchain_data;
   (void)viewport_data;

   /* stub */
}

static bool hlsl_d3d9_renderchain_render(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      unsigned pitch, unsigned rotation)
{
   unsigned i;
   unsigned width, height;
   d3d_video_t      *d3d          = (d3d_video_t*)data;
   settings_t *settings           = config_get_ptr();
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)d3d->renderchain_data;
   bool video_smooth              = settings->bools.video_smooth;

   chain->frame_count++;

   video_driver_get_size(&width, &height);

   hlsl_d3d9_renderchain_blit_to_texture(chain,
         frame, frame_width, frame_height, pitch);
   hlsl_d3d9_renderchain_set_vertices(d3d,
         1, frame_width, frame_height, chain->frame_count);

   d3d_set_texture(chain->dev, 0, chain->tex);
   d3d_set_viewports(chain->dev, &d3d->final_viewport);
   d3d_set_sampler_minfilter(chain->dev, 0,
         video_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d_set_sampler_magfilter(chain->dev, 0,
         video_smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);

   d3d_set_vertex_declaration(chain->dev, chain->vertex_decl);
   for (i = 0; i < 4; i++)
      d3d_set_stream_source(chain->dev, i, chain->vertex_buf, 0, sizeof(Vertex));
   d3d_draw_primitive(chain->dev, D3DPT_TRIANGLESTRIP, 0, 2);
   /* TODO/FIXME - last parameter is mat_data - should be something
   other than NULL */
   hlsl_d3d9_renderchain_set_mvp(d3d,
         chain, &d3d->shader, NULL);

   return true;
}

static bool hlsl_d3d9_renderchain_add_pass(
      void *data, const void *info_data)
{
   (void)data;
   (void)info_data;

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

static void hlsl_d3d9_renderchain_viewport_info(
      void *data, struct video_viewport *vp)
{
   unsigned width, height;
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (!d3d || !vp)
      return;

   video_driver_get_size(&width, &height);

   vp->x            = d3d->final_viewport.x;
   vp->y            = d3d->final_viewport.y;
   vp->width        = d3d->final_viewport.width;
   vp->height       = d3d->final_viewport.height;

   vp->full_width   = width;
   vp->full_height  = height;
}

d3d_renderchain_driver_t hlsl_d3d9_renderchain = {
   hlsl_d3d9_renderchain_set_mvp,
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
   hlsl_d3d9_renderchain_viewport_info,
   "hlsl_d3d9",
};
