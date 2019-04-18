/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2018 - Daniel De Matteis
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

#ifndef __D3D9_RENDERCHAIN_H__
#define __D3D9_RENDERCHAIN_H__

#include <stdint.h>

#include <retro_common_api.h>
#include <retro_inline.h>
#include <boolean.h>

#include <d3d9.h>
#include "../common/d3d9_common.h"
#include "../../verbosity.h"

RETRO_BEGIN_DECLS

#define D3D_DEFAULT_NONPOW2         ((UINT)-2)
#define D3D_FILTER_LINEAR           (3 << 0)
#define D3D_FILTER_POINT            (2 << 0)

struct lut_info
{
   LPDIRECT3DTEXTURE9 tex;
   char id[64];
   bool smooth;
};

struct shader_pass
{
   unsigned last_width, last_height;
   struct LinkInfo info;
   D3DPOOL pool;
   LPDIRECT3DTEXTURE9 tex;
   LPDIRECT3DVERTEXBUFFER9 vertex_buf;
   LPDIRECT3DVERTEXDECLARATION9 vertex_decl;
   void *attrib_map;
   void *vprg;
   void *fprg;
   void *vtable;
   void *ftable;
};

#define D3D_PI 3.14159265358979323846264338327

#define VECTOR_LIST_TYPE unsigned
#define VECTOR_LIST_NAME unsigned
#include "../../libretro-common/lists/vector_list.c"
#undef VECTOR_LIST_TYPE
#undef VECTOR_LIST_NAME

#define VECTOR_LIST_TYPE struct lut_info
#define VECTOR_LIST_NAME lut_info
#include "../../libretro-common/lists/vector_list.c"
#undef VECTOR_LIST_TYPE
#undef VECTOR_LIST_NAME

#define VECTOR_LIST_TYPE struct shader_pass
#define VECTOR_LIST_NAME shader_pass
#include "../../libretro-common/lists/vector_list.c"
#undef VECTOR_LIST_TYPE
#undef VECTOR_LIST_NAME

struct D3D9Vertex
{
   float x, y, z;
   float u, v;
   float lut_u, lut_v;
   float r, g, b, a;
};

typedef struct d3d9_renderchain
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
   struct shader_pass_vector_list  *passes;
   struct unsigned_vector_list *bound_tex;
   struct unsigned_vector_list *bound_vert;
   struct lut_info_vector_list *luts;
} d3d9_renderchain_t;

static INLINE void d3d9_renderchain_set_vertices_on_change(
      d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation
      )
{
   struct D3D9Vertex vert[4];
   unsigned i;
   void *verts       = NULL;
   const struct
      LinkInfo *info = (const struct LinkInfo*)&pass->info;
   float _u          = (float)(width)  / info->tex_w;
   float _v          = (float)(height) / info->tex_h;

   pass->last_width  = width;
   pass->last_height = height;

   vert[0].x         = 0.0f;
   vert[0].y         = out_height;
   vert[0].z         = 0.5f;
   vert[0].u         = 0.0f;
   vert[0].v         = 0.0f;
   vert[0].lut_u     = 0.0f;
   vert[0].lut_v     = 0.0f;
   vert[0].r         = 1.0f;
   vert[0].g         = 1.0f;
   vert[0].b         = 1.0f;
   vert[0].a         = 1.0f;

   vert[1].x         = out_width;
   vert[1].y         = out_height;
   vert[1].z         = 0.5f;
   vert[1].u         = _u;
   vert[1].v         = 0.0f;
   vert[1].lut_u     = 1.0f;
   vert[1].lut_v     = 0.0f;
   vert[1].r         = 1.0f;
   vert[1].g         = 1.0f;
   vert[1].b         = 1.0f;
   vert[1].a         = 1.0f;

   vert[2].x         = 0.0f;
   vert[2].y         = 0.0f;
   vert[2].z         = 0.5f;
   vert[2].u         = 0.0f;
   vert[2].v         = _v;
   vert[2].lut_u     = 0.0f;
   vert[2].lut_v     = 1.0f;
   vert[2].r         = 1.0f;
   vert[2].g         = 1.0f;
   vert[2].b         = 1.0f;
   vert[2].a         = 1.0f;

   vert[3].x         = out_width;
   vert[3].y         = 0.0f;
   vert[3].z         = 0.5f;
   vert[3].u         = _u;
   vert[3].v         = _v;
   vert[3].lut_u     = 1.0f;
   vert[3].lut_v     = 1.0f;
   vert[3].r         = 1.0f;
   vert[3].g         = 1.0f;
   vert[3].b         = 1.0f;
   vert[3].a         = 1.0f;

   /* Align texels and vertices.
    *
    * Fixes infamous 'half-texel offset' issue of D3D9
    *	http://msdn.microsoft.com/en-us/library/bb219690%28VS.85%29.aspx.
    */
   for (i = 0; i < 4; i++)
   {
      vert[i].x     -= 0.5f;
      vert[i].y     += 0.5f;
   }

   verts             = d3d9_vertex_buffer_lock(pass->vertex_buf);
   memcpy(verts, vert, sizeof(vert));
   d3d9_vertex_buffer_unlock(pass->vertex_buf);
}

static INLINE bool d3d9_renderchain_add_pass(d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      const struct LinkInfo *info)
{
   LPDIRECT3DTEXTURE9      tex;
   LPDIRECT3DVERTEXBUFFER9 vertbuf = (LPDIRECT3DVERTEXBUFFER9)
      d3d9_vertex_buffer_new(chain->dev,
            4 * sizeof(struct D3D9Vertex),
            D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, NULL);

   if (!vertbuf)
      return false;

   pass->vertex_buf = vertbuf;

   tex = (LPDIRECT3DTEXTURE9)d3d9_texture_new(
         chain->dev,
         NULL,
         info->tex_w,
         info->tex_h,
         1,
         D3DUSAGE_RENDERTARGET,
         chain->passes->data[
         chain->passes->count - 1].info.pass->fbo.fp_fbo
         ? D3DFMT_A32B32G32R32F : d3d9_get_argb8888_format(),
         D3DPOOL_DEFAULT, 0, 0, 0, NULL, NULL, false);

   if (!tex)
      return false;

   pass->tex        = tex;

   d3d9_set_texture(chain->dev, 0, pass->tex);
   d3d9_set_sampler_address_u(chain->dev, 0, D3DTADDRESS_BORDER);
   d3d9_set_sampler_address_v(chain->dev, 0, D3DTADDRESS_BORDER);
   d3d9_set_texture(chain->dev, 0, NULL);

   shader_pass_vector_list_append(chain->passes, *pass);

   return true;
}

static INLINE bool d3d9_renderchain_add_lut(d3d9_renderchain_t *chain,
      const char *id, const char *path, bool smooth)
{
   struct lut_info info;
   LPDIRECT3DTEXTURE9 lut    = (LPDIRECT3DTEXTURE9)
      d3d9_texture_new(
            chain->dev,
            path,
            D3D_DEFAULT_NONPOW2,
            D3D_DEFAULT_NONPOW2,
            0,
            0,
            ((D3DFORMAT)-3), /* D3DFMT_FROM_FILE */
            D3DPOOL_MANAGED,
            smooth ? D3D_FILTER_LINEAR : D3D_FILTER_POINT,
            0,
            0,
            NULL,
            NULL,
            false
            );

   RARCH_LOG("[D3D9]: LUT texture loaded: %s.\n", path);

   info.tex    = lut;
   info.smooth = smooth;
   strlcpy(info.id, id, sizeof(info.id));
   if (!lut)
      return false;

   d3d9_set_texture(chain->dev, 0, lut);
   d3d9_set_sampler_address_u(chain->dev, 0, D3DTADDRESS_BORDER);
   d3d9_set_sampler_address_v(chain->dev, 0, D3DTADDRESS_BORDER);
   d3d9_set_texture(chain->dev, 0, NULL);

   lut_info_vector_list_append(chain->luts, info);

   return true;
}

static INLINE void d3d9_renderchain_destroy_passes_and_luts(
      d3d9_renderchain_t *chain)
{
   if (chain->passes)
   {
      unsigned i;

      for (i = 0; i < chain->passes->count; i++)
      {
         if (chain->passes->data[i].attrib_map)
            free(chain->passes->data[i].attrib_map);
      }

      shader_pass_vector_list_free(chain->passes);

      chain->passes = NULL;
   }

   lut_info_vector_list_free(chain->luts);
   unsigned_vector_list_free(chain->bound_tex);
   unsigned_vector_list_free(chain->bound_vert);

   chain->luts       = NULL;
   chain->bound_tex  = NULL;
   chain->bound_vert = NULL;
}

static INLINE void d3d9_renderchain_add_lut_internal(
      d3d9_renderchain_t *chain,
      unsigned index, unsigned i)
{
   d3d9_set_texture(chain->dev, index, chain->luts->data[i].tex);
   d3d9_set_sampler_magfilter(chain->dev, index,
         d3d_translate_filter(chain->luts->data[i].smooth ? RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST));
   d3d9_set_sampler_minfilter(chain->dev, index,
         d3d_translate_filter(chain->luts->data[i].smooth ? RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST));
   d3d9_set_sampler_address_u(chain->dev, index, D3DTADDRESS_BORDER);
   d3d9_set_sampler_address_v(chain->dev, index, D3DTADDRESS_BORDER);
   unsigned_vector_list_append(chain->bound_tex, index);
}

static INLINE void d3d9_renderchain_start_render(d3d9_renderchain_t *chain)
{
   chain->passes->data[0].tex         = chain->prev.tex[
      chain->prev.ptr];
   chain->passes->data[0].vertex_buf  = chain->prev.vertex_buf[
      chain->prev.ptr];
   chain->passes->data[0].last_width  = chain->prev.last_width[
      chain->prev.ptr];
   chain->passes->data[0].last_height = chain->prev.last_height[
      chain->prev.ptr];
}

static INLINE void d3d9_renderchain_end_render(d3d9_renderchain_t *chain)
{
   chain->prev.last_width[chain->prev.ptr]  = chain->passes->data[0].last_width;
   chain->prev.last_height[chain->prev.ptr] = chain->passes->data[0].last_height;
   chain->prev.ptr                          = (chain->prev.ptr + 1) & TEXTURESMASK;
}

static INLINE void d3d9_renderchain_unbind_all(d3d9_renderchain_t *chain)
{
   unsigned i;

   /* Have to be a bit anal about it.
    * Render targets hate it when they have filters apparently.
    */
   for (i = 0; i < chain->bound_tex->count; i++)
   {
      d3d9_set_sampler_minfilter(chain->dev,
            chain->bound_tex->data[i], D3DTEXF_POINT);
      d3d9_set_sampler_magfilter(chain->dev,
            chain->bound_tex->data[i], D3DTEXF_POINT);
      d3d9_set_texture(chain->dev,
            chain->bound_tex->data[i], NULL);
   }

   for (i = 0; i < chain->bound_vert->count; i++)
      d3d9_set_stream_source(chain->dev,
            chain->bound_vert->data[i], 0, 0, 0);

   if (chain->bound_tex)
   {
      unsigned_vector_list_free(chain->bound_tex);
      chain->bound_tex = unsigned_vector_list_new();
   }

   if (chain->bound_vert)
   {
      unsigned_vector_list_free(chain->bound_vert);
      chain->bound_vert = unsigned_vector_list_new();
   }
}

static INLINE bool d3d9_renderchain_set_pass_size(
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      struct shader_pass *pass2,
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

static INLINE void d3d9_recompute_pass_sizes(
      LPDIRECT3DDEVICE9 dev,
      d3d9_renderchain_t *chain,
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

   if (!d3d9_renderchain_set_pass_size(dev,
            (struct shader_pass*)&chain->passes->data[0],
            (struct shader_pass*)&chain->passes->data[
            chain->passes->count - 1],
            current_width, current_height))
   {
      RARCH_ERR("[D3D9]: Failed to set pass size.\n");
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

      if (!d3d9_renderchain_set_pass_size(dev,
               (struct shader_pass*)&chain->passes->data[i],
               (struct shader_pass*)&chain->passes->data[
               chain->passes->count - 1],
               link_info.tex_w, link_info.tex_h))
      {
         RARCH_ERR("[D3D9]: Failed to set pass size.\n");
         return;
      }

      current_width  = out_width;
      current_height = out_height;

      link_info.pass = &d3d->shader.pass[i];
   }
}

static INLINE void d3d9_init_renderchain(d3d9_renderchain_t *chain)
{
   chain->passes     = shader_pass_vector_list_new();
   chain->luts       = lut_info_vector_list_new();
   chain->bound_tex  = unsigned_vector_list_new();
   chain->bound_vert = unsigned_vector_list_new();
}

static INLINE void d3d9_renderchain_blit_to_texture(
      LPDIRECT3DTEXTURE9 tex,
      const void *frame,
      unsigned tex_width,  unsigned tex_height,
      unsigned width,      unsigned height,
      unsigned last_width, unsigned last_height,
      unsigned pitch, unsigned pixel_size)
{
   D3DLOCKED_RECT d3dlr    = {0, NULL};

   if (
         (last_width != width || last_height != height)
      )
   {
      d3d9_lock_rectangle(tex, 0, &d3dlr,
            NULL, tex_height, D3DLOCK_NOSYSLOCK);
      d3d9_lock_rectangle_clear(tex, 0, &d3dlr,
            NULL, tex_height, D3DLOCK_NOSYSLOCK);
   }

   if (d3d9_lock_rectangle(tex, 0, &d3dlr, NULL, 0, 0))
   {
      d3d9_texture_blit(pixel_size, tex,
            &d3dlr, frame, width, height, pitch);
      d3d9_unlock_rectangle(tex);
   }
}

RETRO_END_DECLS

#endif
