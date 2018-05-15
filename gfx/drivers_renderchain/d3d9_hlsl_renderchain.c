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

#include "../../defines/d3d_defines.h"
#include "../common/d3d_common.h"
#include "../common/d3d9_common.h"

#include "../video_driver.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

struct hlsl_pass
{
   unsigned last_width, last_height;
   struct LinkInfo info;
   D3DPOOL pool;
   LPDIRECT3DTEXTURE9 tex;
   LPDIRECT3DVERTEXBUFFER9 vertex_buf;
   LPDIRECT3DVERTEXDECLARATION9 vertex_decl;
   void *attrib_map;
};

struct HLSLVertex
{
   float x, y, z;
   float u, v;
   float lut_u, lut_v;
   float r, g, b, a;
};

#define VECTOR_LIST_TYPE struct hlsl_pass
#define VECTOR_LIST_NAME hlsl_pass
#include "../../libretro-common/lists/vector_list.c"
#undef VECTOR_LIST_TYPE
#undef VECTOR_LIST_NAME

#include "d3d9_renderchain.h"

typedef struct hlsl_d3d9_renderchain
{
   unsigned pixel_size;
   uint64_t frame_count;
   struct
   {
      LPDIRECT3DTEXTURE9 tex[TEXTURES];
      LPDIRECT3DVERTEXBUFFER9 vertex_buf[TEXTURES];
      unsigned ptr;
      unsigned last_width[TEXTURES];
      unsigned last_height[TEXTURES];
   } prev;
   LPDIRECT3DDEVICE9 dev;
   D3DVIEWPORT9 *final_viewport;
   struct hlsl_pass_vector_list  *passes;
   struct unsigned_vector_list *bound_tex;
   struct unsigned_vector_list *bound_vert;
   struct lut_info_vector_list *luts;
} hlsl_d3d9_renderchain_t;

static bool hlsl_d3d9_renderchain_init_shader_fvf(
      hlsl_d3d9_renderchain_t *chain,
      struct hlsl_pass *pass)
{
   static const D3DVERTEXELEMENT9 decl[] =
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
      D3D9_DECL_FVF_TEXCOORD(0, 2, 0),
      D3DDECL_END()
   };

   return d3d9_vertex_declaration_new(chain->dev,
         decl, (void**)&pass->vertex_decl);
}

static bool hlsl_d3d9_renderchain_create_first_pass(
      LPDIRECT3DDEVICE9 dev,
      hlsl_d3d9_renderchain_t *chain,
      const struct LinkInfo *info,
      unsigned _fmt)
{
   unsigned i;
   struct hlsl_pass pass;
   unsigned fmt = 
      (_fmt == RETRO_PIXEL_FORMAT_RGB565) ? 
      d3d9_get_rgb565_format() : d3d9_get_xrgb8888_format();

   pass.info        = *info;
   pass.last_width  = 0;
   pass.last_height = 0;
   pass.attrib_map  = (struct unsigned_vector_list*)
      unsigned_vector_list_new();

   chain->prev.ptr  = 0;

   for (i = 0; i < TEXTURES; i++)
   {
      chain->prev.last_width[i]  = 0;
      chain->prev.last_height[i] = 0;
      chain->prev.vertex_buf[i]  = (LPDIRECT3DVERTEXBUFFER9)
         d3d9_vertex_buffer_new(
            chain->dev, 4 * sizeof(struct HLSLVertex),
            D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, NULL);

      if (!chain->prev.vertex_buf[i])
         return false;

      chain->prev.tex[i] = (LPDIRECT3DTEXTURE9)
         d3d9_texture_new(chain->dev, NULL,
            info->tex_w, info->tex_h, 1, 0, fmt,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!chain->prev.tex[i])
         return false;

      d3d9_set_texture(chain->dev, 0, chain->prev.tex[i]);
      d3d9_set_sampler_minfilter(dev, 0,
            d3d_translate_filter(info->pass->filter));
      d3d9_set_sampler_magfilter(dev, 0,
            d3d_translate_filter(info->pass->filter));
      d3d9_set_sampler_address_u(dev, 0, D3DTADDRESS_BORDER);
      d3d9_set_sampler_address_v(dev, 0, D3DTADDRESS_BORDER);
      d3d9_set_texture(chain->dev, 0, NULL);
   }

   pass.vertex_buf        = d3d9_vertex_buffer_new(
         dev, 4 * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
         0,
         D3DPOOL_MANAGED,
         NULL);

   if (!pass.vertex_buf)
      return false;

   pass.tex = d3d9_texture_new(dev, NULL,
         pass.info.tex_w, pass.info.tex_h, 1, 0, fmt,
         0, 0, 0, 0, NULL, NULL, false);

   if (!pass.tex)
      return false;

   d3d9_set_sampler_address_u(dev, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d9_set_sampler_address_v(dev, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   d3d9_set_render_state(dev, D3DRS_CULLMODE, D3DCULL_NONE);
   d3d9_set_render_state(dev, D3DRS_ZENABLE, FALSE);

   if (!hlsl_d3d9_renderchain_init_shader_fvf(chain, &pass))
      return false;
   hlsl_pass_vector_list_append(chain->passes, pass);

   return true;
}

static void hlsl_d3d9_renderchain_set_vertices(
      d3d9_video_t *d3d,
      hlsl_d3d9_renderchain_t *chain,
      struct hlsl_pass *pass,
      unsigned pass_count,
      unsigned vert_width, unsigned vert_height,
      uint64_t frame_count)
{
   video_shader_ctx_params_t params;
   video_shader_ctx_info_t shader_info;
   unsigned width, height;

   video_driver_get_size(&width, &height);

   if (pass->last_width != vert_width || pass->last_height != vert_height)
   {
      unsigned i;
      Vertex vert[4];
      float tex_w       = 0.0f;
      float tex_h       = 0.0f;
      void *verts       = NULL;

      pass->last_width  = vert_width;
      pass->last_height = vert_height;

      tex_w              = vert_width  / ((float)pass->info.tex_w);
      tex_h              = vert_height / ((float)pass->info.tex_h);

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
         vert[i].x      -= 0.5f / ((float)pass->info.tex_w);
         vert[i].y      += 0.5f / ((float)pass->info.tex_h);
      }

      verts = d3d9_vertex_buffer_lock(pass->vertex_buf);
      memcpy(verts, vert, sizeof(vert));
      d3d9_vertex_buffer_unlock(pass->vertex_buf);
   }

   shader_info.data       = d3d;
   shader_info.idx        = pass_count;
   shader_info.set_active = true;

   video_shader_driver_use(&shader_info);

   params.data          = d3d;
   params.width         = vert_width;
   params.height        = vert_height;
   params.tex_width     = pass->info.tex_w;
   params.tex_height    = pass->info.tex_h;
   params.out_width     = width;
   params.out_height    = height;
   params.frame_counter = (unsigned int)frame_count;
   params.info          = NULL;
   params.prev_info     = NULL;
   params.feedback_info = NULL;
   params.fbo_info      = NULL;
   params.fbo_info_cnt  = 0;

#if 0
   d3d9_cg_renderchain_calc_and_set_shader_mvp(
         /*pass->vPrg, */vp_width, vp_height, rotation);
#endif
   video_shader_driver_set_parameters(&params);
}

static void d3d9_hlsl_deinit_progs(hlsl_d3d9_renderchain_t *chain)
{
   RARCH_LOG("[D3D9 HLSL]: Destroying programs.\n");

   if (chain->passes->count >= 1)
   {
      unsigned i;

      d3d9_vertex_buffer_free(NULL, chain->passes->data[0].vertex_decl);

      for (i = 1; i < chain->passes->count; i++)
      {
         if (chain->passes->data[i].tex)
            d3d9_texture_free(chain->passes->data[i].tex);
         chain->passes->data[i].tex = NULL;
         d3d9_vertex_buffer_free(
               chain->passes->data[i].vertex_buf,
               chain->passes->data[i].vertex_decl);
      }
   }
}

static void d3d9_hlsl_destroy_resources(hlsl_d3d9_renderchain_t *chain)
{
   unsigned i;

   for (i = 0; i < TEXTURES; i++)
   {
      if (chain->prev.tex[i])
         d3d9_texture_free(chain->prev.tex[i]);
      if (chain->prev.vertex_buf[i])
         d3d9_vertex_buffer_free(chain->prev.vertex_buf[i], NULL);
   }

   d3d9_hlsl_deinit_progs(chain);

   for (i = 0; i < chain->luts->count; i++)
   {
      if (chain->luts->data[i].tex)
         d3d9_texture_free(chain->luts->data[i].tex);
   }
}

static void hlsl_d3d9_renderchain_free(void *data)
{
   unsigned i;
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)data;

   if (!chain)
      return;

   d3d9_hlsl_destroy_resources(chain);

   if (chain->passes)
   {
      unsigned i;

      for (i = 0; i < chain->passes->count; i++)
      {
         if (chain->passes->data[i].attrib_map)
            free(chain->passes->data[i].attrib_map);
      }

      hlsl_pass_vector_list_free(chain->passes);

      chain->passes = NULL;
   }

   lut_info_vector_list_free(chain->luts);
   unsigned_vector_list_free(chain->bound_tex);
   unsigned_vector_list_free(chain->bound_vert);

   chain->luts       = NULL;
   chain->bound_tex  = NULL;
   chain->bound_vert = NULL;

   free(chain);
}

void *hlsl_d3d9_renderchain_new(void)
{
   hlsl_d3d9_renderchain_t *renderchain =
      (hlsl_d3d9_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   renderchain->passes     = hlsl_pass_vector_list_new();
   renderchain->luts       = lut_info_vector_list_new();
   renderchain->bound_tex  = unsigned_vector_list_new();
   renderchain->bound_vert = unsigned_vector_list_new();

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

   RARCH_LOG("[D3D9]: Using HLSL shader backend.\n");

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
   hlsl_d3d9_renderchain_t *chain     = (hlsl_d3d9_renderchain_t*)
      d3d->renderchain_data;
   unsigned fmt                       = (rgb32)
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;

   if (!chain)
      return false;
   if (!hlsl_d3d9_renderchain_init_shader(d3d, chain))
   {
      RARCH_ERR("[D3D9 HLSL]: Failed to initialize shader subsystem.\n");
      return false;
   }

   chain->dev                         = dev;
   chain->final_viewport              = (D3DVIEWPORT9*)final_viewport;
   chain->frame_count                 = 0;
   chain->pixel_size                  = (fmt == RETRO_PIXEL_FORMAT_RGB565) ? 2 : 4;

   if (!hlsl_d3d9_renderchain_create_first_pass(dev, chain, info, fmt))
      return false;

   return true;
}

static bool d3d9_hlsl_set_pass_size(
      LPDIRECT3DDEVICE9 dev,
      struct hlsl_pass *pass,
      struct hlsl_pass *pass2,
      unsigned width, unsigned height)
{
   if (width != pass->info.tex_w || height != pass->info.tex_h)
   {
      d3d9_texture_free(pass->tex);

      pass->info.tex_w = width;
      pass->info.tex_h = height;
      pass->pool       = D3DPOOL_DEFAULT;
      pass->tex        = (LPDIRECT3DTEXTURE9)
         d3d9_texture_new(dev, NULL,
            width, height, 1,
            D3DUSAGE_RENDERTARGET,
            pass2->info.pass->fbo.fp_fbo ?
            D3DFMT_A32B32G32R32F : d3d9_get_argb8888_format(),
            D3DPOOL_DEFAULT, 0, 0, 0,
            NULL, NULL, false);

      if (!pass->tex)
         return false;

      d3d9_set_texture(dev, 0, pass->tex);
      d3d9_set_sampler_address_u(dev, 0, D3DTADDRESS_BORDER);
      d3d9_set_sampler_address_v(dev, 0, D3DTADDRESS_BORDER);
      d3d9_set_texture(dev, 0, NULL);
   }

   return true;
}

static void d3d9_hlsl_recompute_pass_sizes(
      LPDIRECT3DDEVICE9 dev,
      hlsl_d3d9_renderchain_t *chain,
      d3d9_video_t *d3d)
{
   unsigned i;
   struct LinkInfo link_info;
   unsigned input_scale              = d3d->video_info.input_scale 
      * RARCH_SCALE_BASE;
   unsigned current_width            = input_scale;
   unsigned current_height           = input_scale;
   unsigned out_width                = 0;
   unsigned out_height               = 0;

   link_info.pass                    = &d3d->shader.pass[0];
   link_info.tex_w                   = current_width;
   link_info.tex_h                   = current_height;


   if (!d3d9_hlsl_set_pass_size(dev,
            (struct hlsl_pass*)&chain->passes->data[0],
            (struct hlsl_pass*)&chain->passes->data[chain->passes->count - 1],
            current_width, current_height))
   {
      RARCH_ERR("[D3D9 Cg]: Failed to set pass size.\n");
      return;
   }

   for (i = 1; i < d3d->shader.passes; i++)
   {
      d3d9_convert_geometry(
            &link_info,
            &out_width, &out_height,
            current_width, current_height, &d3d->final_viewport);

      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      if (!d3d9_hlsl_set_pass_size(dev,
               (struct hlsl_pass*)&chain->passes->data[i],
               (struct hlsl_pass*)&chain->passes->data[chain->passes->count - 1],
               link_info.tex_w, link_info.tex_h))
      {
         RARCH_ERR("[D3D9 Cg]: Failed to set pass size.\n");
         return;
      }

      current_width  = out_width;
      current_height = out_height;

      link_info.pass = &d3d->shader.pass[i];
   }
}

static void hlsl_d3d9_renderchain_set_final_viewport(
      d3d9_video_t *d3d,
      void *renderchain_data,
      const D3DVIEWPORT9 *final_viewport)
{
   hlsl_d3d9_renderchain_t *chain     = (hlsl_d3d9_renderchain_t*)renderchain_data;

   if (chain && final_viewport)
      chain->final_viewport = (D3DVIEWPORT9*)final_viewport;

   d3d9_hlsl_recompute_pass_sizes(chain->dev, chain, d3d);
}

static void hlsl_d3d9_renderchain_render_pass(
      hlsl_d3d9_renderchain_t *chain,
      struct hlsl_pass *pass,
      state_tracker_t *tracker,
      unsigned pass_index)
{
   unsigned i;

   d3d9_set_texture(chain->dev, 0, pass->tex);
   d3d9_set_sampler_minfilter(chain->dev, 0,
         d3d_translate_filter(pass->info.pass->filter));
   d3d9_set_sampler_magfilter(chain->dev, 0,
         d3d_translate_filter(pass->info.pass->filter));

   d3d9_set_vertex_declaration(chain->dev, pass->vertex_decl);
   for (i = 0; i < 4; i++)
      d3d9_set_stream_source(chain->dev, i,
            pass->vertex_buf, 0,
            sizeof(Vertex));

   d3d9_draw_primitive(chain->dev, D3DPT_TRIANGLESTRIP, 0, 2);

   /* So we don't render with linear filter into render targets,
    * which apparently looked odd (too blurry). */
   d3d9_set_sampler_minfilter(chain->dev, 0, D3DTEXF_POINT);
   d3d9_set_sampler_magfilter(chain->dev, 0, D3DTEXF_POINT);
}

static void d3d9_hlsl_renderchain_calc_and_set_shader_mvp(
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   struct d3d_matrix proj, ortho, rot, matrix;

   d3d_matrix_ortho_off_center_lh(&ortho, 0, vp_width, 0, vp_height, 0, 1);
   d3d_matrix_identity(&rot);
   d3d_matrix_rotation_z(&rot, rotation * (D3D_PI / 2.0));

   d3d_matrix_multiply(&proj, &ortho, &rot);
   d3d_matrix_transpose(&matrix, &proj);

#if 0
   cgD3D9SetUniformMatrix(cgpModelViewProj, (D3DMATRIX*)&matrix);
#endif
}

static bool hlsl_d3d9_renderchain_render(
      d3d9_video_t *d3d,
      state_tracker_t *tracker,
      const void *frame,
      unsigned width, unsigned height,
      unsigned pitch, unsigned rotation)
{
   unsigned i, current_width, current_height, out_width = 0, out_height = 0;
   struct hlsl_pass *last_pass    = NULL;
   struct hlsl_pass *first_pass   = NULL;
   settings_t *settings           = config_get_ptr();
   hlsl_d3d9_renderchain_t *chain = (hlsl_d3d9_renderchain_t*)
      d3d->renderchain_data;

   current_width                  = width;
   current_height                 = height;

   first_pass                     = (struct hlsl_pass*)&chain->passes->data[0];
   last_pass                      = (struct hlsl_pass*)&chain->passes->data[0];

   d3d9_convert_geometry(
         &first_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->final_viewport);

   d3d9_renderchain_blit_to_texture(first_pass->tex,
         frame,
         first_pass->info.tex_w,
         first_pass->info.tex_h,
         width,
         height,
         first_pass->last_width,
         first_pass->last_height,
         pitch,
         chain->pixel_size);

   d3d9_convert_geometry(&last_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->final_viewport);

   d3d9_set_viewports(chain->dev, &d3d->final_viewport);

   hlsl_d3d9_renderchain_set_vertices(d3d, chain, last_pass,
         1, width, height, chain->frame_count);

   hlsl_d3d9_renderchain_render_pass(chain, last_pass,
         tracker,
         chain->passes->count);

   chain->frame_count++;

   d3d9_hlsl_renderchain_calc_and_set_shader_mvp(
         /* chain->vStock, */ chain->final_viewport->Width,
         chain->final_viewport->Height, 0);

   return true;
}

static bool hlsl_d3d9_renderchain_add_pass(
      void *data, const struct LinkInfo *info)
{
   (void)data;

   /* stub */
   return true;
}

d3d9_renderchain_driver_t hlsl_d3d9_renderchain = {
   hlsl_d3d9_renderchain_free,
   hlsl_d3d9_renderchain_new,
   hlsl_d3d9_renderchain_init,
   hlsl_d3d9_renderchain_set_final_viewport,
   hlsl_d3d9_renderchain_add_pass,
   NULL, /* add_lut */
   hlsl_d3d9_renderchain_render,
   NULL, /* read_viewport */
   "hlsl_d3d9",
};
