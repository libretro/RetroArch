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

static INLINE bool d3d9_renderchain_add_pass(d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      const struct LinkInfo *info)
{
   LPDIRECT3DTEXTURE9      tex;
   LPDIRECT3DVERTEXBUFFER9 vertbuf = (LPDIRECT3DVERTEXBUFFER9)
      d3d9_vertex_buffer_new(chain->dev,
            4 * sizeof(struct Vertex),
            D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, NULL);

   if (!vertbuf)
      return false;

   pass->vertex_buf = vertbuf;

   tex = (LPDIRECT3DTEXTURE9)d3d9_texture_new(
         chain->dev,
         info->tex_w,
         info->tex_h,
         1,
         D3DUSAGE_RENDERTARGET,
         (chain->passes->data[
         chain->passes->count - 1].info.pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
         ? D3DFMT_A32B32G32R32F : D3D9_ARGB8888_FORMAT,
         D3DPOOL_DEFAULT, 0, 0, 0, NULL, NULL, false);

   if (!tex)
      return false;

   pass->tex        = tex;

   IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)pass->tex);
   IDirect3DDevice9_SetSamplerState(chain->dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(chain->dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)NULL);

   shader_pass_vector_list_append(chain->passes, *pass);

   return true;
}

static INLINE bool d3d9_renderchain_add_lut(d3d9_renderchain_t *chain,
      const char *id, const char *path, bool smooth)
{
   struct lut_info info;
   LPDIRECT3DTEXTURE9 lut    = (LPDIRECT3DTEXTURE9)
      d3d9_texture_new_from_file(
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
   if (!lut)
      return false;

   RARCH_LOG("[D3D9]: LUT texture loaded: %s.\n", path);

   info.tex    = lut;
   info.smooth = smooth;
   strlcpy(info.id, id, sizeof(info.id));

   IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)lut);
   IDirect3DDevice9_SetSamplerState(chain->dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(chain->dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)NULL);

   lut_info_vector_list_append(chain->luts, info);

   return true;
}

static INLINE void d3d9_renderchain_destroy_passes_and_luts(
      d3d9_renderchain_t *chain)
{
   if (chain->passes)
   {
      int i;

      for (i = 0; i < (int) chain->passes->count; i++)
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
   /* 2 = D3D_TEXTURE_FILTER_LINEAR, 1 = D3D_TEXTURE_FILTER_POINT */
   int32_t filter = chain->luts->data[i].smooth ? 2 : 1;
   IDirect3DDevice9_SetTexture(chain->dev, index, (IDirect3DBaseTexture9*)chain->luts->data[i].tex);
   IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_MAGFILTER, filter);
   IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_MINFILTER, filter);
   IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
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
   int i;
   /* Have to be a bit anal about it.
    * Render targets hate it when they have filters apparently.
    */
   for (i = 0; i < (int) chain->bound_tex->count; i++)
   {
      IDirect3DDevice9_SetSamplerState(chain->dev,
            chain->bound_tex->data[i], D3DSAMP_MINFILTER, D3DTEXF_POINT);
      IDirect3DDevice9_SetSamplerState(chain->dev,
            chain->bound_tex->data[i], D3DSAMP_MAGFILTER, D3DTEXF_POINT);
      IDirect3DDevice9_SetTexture(chain->dev,
            chain->bound_tex->data[i], (IDirect3DBaseTexture9*)NULL);
   }

   for (i = 0; i < (int) chain->bound_vert->count; i++)
      IDirect3DDevice9_SetStreamSource(chain->dev, chain->bound_vert->data[i], 0, 0, 0);

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
      IDirect3DTexture9_Release(pass->tex);

      pass->info.tex_w = width;
      pass->info.tex_h = height;
      pass->pool       = D3DPOOL_DEFAULT;
      pass->tex        = (LPDIRECT3DTEXTURE9)
         d3d9_texture_new(dev,
            width, height, 1,
            D3DUSAGE_RENDERTARGET,
            (pass2->info.pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
            ? D3DFMT_A32B32G32R32F 
            : D3D9_ARGB8888_FORMAT,
            D3DPOOL_DEFAULT, 0, 0, 0,
            NULL, NULL, false);

      if (!pass->tex)
         return false;

      IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9*)pass->tex);
      IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9*)NULL);
   }

   return true;
}

static INLINE void d3d9_convert_geometry(
      const struct LinkInfo *info,
      unsigned *out_width,
      unsigned *out_height,
      unsigned width,
      unsigned height,
      D3DVIEWPORT9 *final_viewport)
{
   switch (info->pass->fbo.type_x)
   {
      case RARCH_SCALE_VIEWPORT:
         *out_width = info->pass->fbo.scale_x * final_viewport->Width;
         break;

      case RARCH_SCALE_ABSOLUTE:
         *out_width = info->pass->fbo.abs_x;
         break;

      case RARCH_SCALE_INPUT:
         *out_width = info->pass->fbo.scale_x * width;
         break;
   }

   switch (info->pass->fbo.type_y)
   {
      case RARCH_SCALE_VIEWPORT:
         *out_height = info->pass->fbo.scale_y * final_viewport->Height;
         break;

      case RARCH_SCALE_ABSOLUTE:
         *out_height = info->pass->fbo.abs_y;
         break;

      case RARCH_SCALE_INPUT:
         *out_height = info->pass->fbo.scale_y * height;
         break;
   }
}

static INLINE void d3d9_recompute_pass_sizes(
      LPDIRECT3DDEVICE9 dev,
      d3d9_renderchain_t *chain,
      d3d9_video_t *d3d)
{
   int i;
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

   for (i = 1; i < (int) d3d->shader.passes; i++)
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

RETRO_END_DECLS

#endif
