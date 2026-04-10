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

/* Direct3D 9 driver with HLSL runtime backend.
 *
 * Minimum version : Direct3D 9.0 (2002)
 * Minimum OS      : Windows 98, Windows 2000, Windows ME
 * Recommended OS  : Windows XP
 * Requirements    : HLSL or fixed function backend
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
#include <encodings/utf.h>

#include <string.h>
#include <retro_inline.h>
#include <retro_math.h>

#ifndef _XBOX
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d9.h>
#include "../common/d3dcompiler_common.h"
#include "../common/d3d9_common.h"

#include "d3d_shaders/opaque.hlsl.d3d9.h"
#include "../video_driver.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include <defines/d3d_defines.h>
#include <math.h>
#include "../common/d3d_common.h"

static INLINE void d3d_matrix_identity(void *_pout)
{
   struct d3d_matrix *pout = (struct d3d_matrix*)_pout;
   pout->m[0][0] = 1.0f;
   pout->m[0][1] = 0.0f;
   pout->m[0][2] = 0.0f;
   pout->m[0][3] = 0.0f;
   pout->m[1][0] = 0.0f;
   pout->m[1][1] = 1.0f;
   pout->m[1][2] = 0.0f;
   pout->m[1][3] = 0.0f;
   pout->m[2][0] = 0.0f;
   pout->m[2][1] = 0.0f;
   pout->m[2][2] = 1.0f;
   pout->m[2][3] = 0.0f;
   pout->m[3][0] = 0.0f;
   pout->m[3][1] = 0.0f;
   pout->m[3][2] = 0.0f;
   pout->m[3][3] = 1.0f;
}

static INLINE void d3d_matrix_transpose(void *_pout, const void *_pm)
{
   unsigned i, j;
   struct d3d_matrix       *pout = (struct d3d_matrix*)_pout;
   const struct d3d_matrix *pm   = (const struct d3d_matrix*)_pm;
   for (i = 0; i < 4; i++)
      for (j = 0; j < 4; j++)
         pout->m[i][j] = pm->m[j][i];
}

static INLINE void d3d_matrix_ortho_off_center_lh(void *_pout,
      float l, float r, float b, float t, float zn, float zf)
{
   struct d3d_matrix *pout = (struct d3d_matrix*)_pout;
   pout->m[0][0] = 2.0f / (r - l);
   pout->m[1][1] = 2.0f / (t - b);
   pout->m[2][2] = 1.0f / (zf - zn);
   pout->m[3][0] = -1.0f - 2.0f * l / (r - l);
   pout->m[3][1] =  1.0f + 2.0f * t / (b - t);
   pout->m[3][2] = zn / (zn - zf);
}

static INLINE void d3d_matrix_multiply(void *_pout,
      const void *_pm1, const void *_pm2)
{
   unsigned i, j;
   struct d3d_matrix      *pout = (struct d3d_matrix*)_pout;
   const struct d3d_matrix *pm1 = (const struct d3d_matrix*)_pm1;
   const struct d3d_matrix *pm2 = (const struct d3d_matrix*)_pm2;
   for (i = 0; i < 4; i++)
      for (j = 0; j < 4; j++)
         pout->m[i][j] = pm1->m[i][0] * pm2->m[0][j]
                        + pm1->m[i][1] * pm2->m[1][j]
                        + pm1->m[i][2] * pm2->m[2][j]
                        + pm1->m[i][3] * pm2->m[3][j];
}

static INLINE void d3d_matrix_rotation_z(void *_pout, float angle)
{
   struct d3d_matrix *pout = (struct d3d_matrix*)_pout;
   pout->m[0][0] =  cosf(angle);
   pout->m[1][1] =  cosf(angle);
   pout->m[0][1] =  sinf(angle);
   pout->m[1][0] = -sinf(angle);
}

#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../frontend/frontend_driver.h"

#include "../common/win32_common.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"

#include "../../core.h"
#include "../../verbosity.h"
#include "../../retroarch.h"

#ifdef __WINRT__
#error "UWP does not support D3D9"
#endif

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
   D3DVIEWPORT9 *out_vp;
   struct shader_pass_vector_list  *passes;
   struct unsigned_vector_list *bound_tex;
   struct unsigned_vector_list *bound_vert;
   struct lut_info_vector_list *luts;
} d3d9_renderchain_t;

static void *d3d9_texture_new(void *_dev,
      unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   void *buf             = NULL;
#ifndef _XBOX
   if (want_mipmap)
      usage |= D3DUSAGE_AUTOGENMIPMAP;
#endif
   return (SUCCEEDED(IDirect3DDevice9_CreateTexture(dev,
               width, height, miplevels, usage,
               (D3DFORMAT)format,
               (D3DPOOL)pool,
               (struct IDirect3DTexture9**)&buf, NULL))) ? buf : NULL;
}


static INLINE bool d3d9_renderchain_add_pass(d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      const struct LinkInfo *info)
{
   LPDIRECT3DTEXTURE9      tex;
   LPDIRECT3DVERTEXBUFFER9 vertbuf = NULL;

   if (!SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
               chain->dev,
               4 * sizeof(struct Vertex),
               D3DUSAGE_WRITEONLY, 0,
               D3DPOOL_DEFAULT,
               (LPDIRECT3DVERTEXBUFFER9*)&vertbuf, NULL)))
      vertbuf = NULL;

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
   struct texture_image image;
   LPDIRECT3DTEXTURE9 lut    = NULL;

   image.pixels              = NULL;
   image.width               = 0;
   image.height              = 0;
   image.supports_rgba       = true;

   if (!image_texture_load(&image, path))
   {
      RARCH_ERR("[D3D9] Failed to load LUT image: %s.\n", path);
      return false;
   }

   lut = (LPDIRECT3DTEXTURE9)d3d9_texture_new(
         chain->dev,
         image.width, image.height, 1,
         0, D3D9_ARGB8888_FORMAT,
         D3DPOOL_MANAGED, 0, 0, 0,
         NULL, NULL, false);

   if (!lut)
   {
      image_texture_free(&image);
      return false;
   }

   {
      D3DLOCKED_RECT lr;
      if (SUCCEEDED(IDirect3DTexture9_LockRect(lut, 0, &lr, NULL, 0)))
      {
         unsigned y;
         uint32_t       *dst   = (uint32_t*)lr.pBits;
         const uint32_t *src   = image.pixels;
         unsigned        pitch = lr.Pitch >> 2;
         for (y = 0; y < image.height; y++, dst += pitch, src += image.width)
            memcpy(dst, src, image.width << 2);
         IDirect3DTexture9_UnlockRect(lut, 0);
      }
   }

   image_texture_free(&image);

   RARCH_LOG("[D3D9] LUT texture loaded: %s.\n", path);

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
      D3DVIEWPORT9 *out_vp)
{
   switch (info->pass->fbo.type_x)
   {
      case RARCH_SCALE_VIEWPORT:
         *out_width = info->pass->fbo.scale_x * out_vp->Width;
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
         *out_height = info->pass->fbo.scale_y * out_vp->Height;
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
      RARCH_ERR("[D3D9] Failed to set pass size.\n");
      return;
   }

   for (i = 1; i < (int) d3d->shader.passes; i++)
   {
      d3d9_convert_geometry(
            &link_info,
            &out_width, &out_height,
            current_width, current_height, &d3d->out_vp);

      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      if (!d3d9_renderchain_set_pass_size(dev,
               (struct shader_pass*)&chain->passes->data[i],
               (struct shader_pass*)&chain->passes->data[
               chain->passes->count - 1],
               link_info.tex_w, link_info.tex_h))
      {
         RARCH_ERR("[D3D9] Failed to set pass size.\n");
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

/* TODO/FIXME - Temporary workaround for D3D9 not being able to poll flags during init */
static gfx_ctx_driver_t d3d9_hlsl_fake_context;

/* BEGIN HLSL RENDERCHAIN */

#define RARCH_HLSL_MAX_SHADERS 16

typedef struct hlsl_renderchain
{
   struct d3d9_renderchain chain;
   struct shader_pass stock_shader;
} hlsl_renderchain_t;

/* Pipeline vertex buffer for menu shader effects (VIDEO_SHADER_MENU, etc.).
 * Stored as a file-static since the d3d9 menu_display struct
 * does not have a pipeline_vbo member like d3d10_video_t does. */
static LPDIRECT3DVERTEXBUFFER9 d3d9_hlsl_menu_pipeline_vbo = NULL;

static void d3d9_vertex_buffer_free(void *vertex_data, void *vertex_declaration)
{
   if (vertex_data)
   {
      LPDIRECT3DVERTEXBUFFER9 buf =
         (LPDIRECT3DVERTEXBUFFER9)vertex_data;
      IDirect3DVertexBuffer9_Release(buf);
      buf = NULL;
   }

   if (vertex_declaration)
   {
      LPDIRECT3DVERTEXDECLARATION9 vertex_decl =
         (LPDIRECT3DVERTEXDECLARATION9)vertex_declaration;
      IDirect3DVertexDeclaration9_Release(vertex_decl);
      vertex_decl = NULL;
   }
}

/* Forward declarations for functions used by display driver
 * but defined in the video driver section below. */
static INLINE void d3d9_hlsl_set_param_1f(void* prog,
      LPDIRECT3DDEVICE9 userdata, const char *name, const void *value);

/*
 * DISPLAY DRIVER
 */

static const float d3d9_hlsl_vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float d3d9_hlsl_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static LPDIRECT3DTEXTURE9 d3d9_hlsl_white_texture = NULL;

/* D3D constant table stubs.
 *
 * The HLSL renderchain previously used LPD3DXCONSTANTTABLE to set shader
 * uniforms. Without D3DX, these are no-ops. The stock shader's MVP and
 * other parameters are now set via SetVertexShaderConstantF / 
 * SetPixelShaderConstantF directly in d3d9hlsl.c. */

static void *d3d9_hlsl_constant_table_get_constant_by_name(void *_tbl,
      void *_handle, void *_name) { return NULL; }
static void d3d9_hlsl_constant_table_set_float_array(LPDIRECT3DDEVICE9 dev,
      void *p, void *_handle, const void *_pf, unsigned count) { }
static void d3d9_hlsl_constant_table_set_defaults(LPDIRECT3DDEVICE9 dev,
      void *p) { }
static void d3d9_hlsl_constant_table_set_matrix(LPDIRECT3DDEVICE9 dev,
      void *p,
      void *data, const void *_matrix) { }
static const bool d3d9_hlsl_constant_table_set_float(void *p,
      void *a, void *b, float val) { return false; }

static INT32 gfx_display_prim_to_d3d9_hlsl_enum(
      enum gfx_display_prim_type prim_type)
{
   switch (prim_type)
   {
      case GFX_DISPLAY_PRIM_TRIANGLES:
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         return D3DPT_COMM_TRIANGLESTRIP;
      case GFX_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   /* TODO/FIXME - hack */
   return 0;
}

static void gfx_display_d3d9_hlsl_blend_begin(void *data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, true);
}

static void gfx_display_d3d9_hlsl_blend_end(void *data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (d3d)
      IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, false);
}

static void gfx_display_d3d9_bind_texture(gfx_display_ctx_draw_t *draw,
      d3d9_video_t *d3d)
{
   LPDIRECT3DDEVICE9 dev = d3d->dev;

   if (draw->texture)
      IDirect3DDevice9_SetTexture(dev, 0,
            (IDirect3DBaseTexture9*)draw->texture);
   else if (d3d9_hlsl_white_texture)
      IDirect3DDevice9_SetTexture(dev, 0,
            (IDirect3DBaseTexture9*)d3d9_hlsl_white_texture);
   else
      IDirect3DDevice9_SetTexture(dev, 0, NULL);

   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_ADDRESSU, D3DTADDRESS_COMM_CLAMP);
   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_ADDRESSV, D3DTADDRESS_COMM_CLAMP);
   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_MINFILTER, D3DTEXF_COMM_LINEAR);
   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_MAGFILTER, D3DTEXF_COMM_LINEAR);
   IDirect3DDevice9_SetSamplerState(dev, 0,
         D3DSAMP_MIPFILTER, D3DTEXF_COMM_LINEAR);
}

static void gfx_display_d3d9_hlsl_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   unsigned i;
   LPDIRECT3DDEVICE9 dev;
   D3DPRIMITIVETYPE type;
   bool has_vertex_data;
   unsigned start                = 0;
   unsigned count                = 0;
   unsigned vertex_count           = 4;
   d3d9_video_t *d3d             = (d3d9_video_t*)data;
   Vertex * pv                   = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;

   if (!d3d || !draw)
      return;

   dev                           = d3d->dev;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      {
         /* Draw the pipeline vertices using the stock shader,
          * then restore blend state and menu display vertex state.
          * Adapted from d3d10 gfx_display_d3d10_draw pipeline path. */
         hlsl_renderchain_t *_chain = (hlsl_renderchain_t*)d3d->renderchain_data;

         if (_chain)
         {
            IDirect3DDevice9_SetVertexShader(dev, (LPDIRECT3DVERTEXSHADER9)(&_chain->stock_shader)->vprg);

            IDirect3DDevice9_SetPixelShader(dev, (LPDIRECT3DPIXELSHADER9)(&_chain->stock_shader)->fprg);

            IDirect3DDevice9_DrawPrimitive(dev, D3DPT_COMM_TRIANGLESTRIP,
                  0, draw->coords->vertices - 2);
         }

         /* Re-enable alpha blending after pipeline draw */
         IDirect3DDevice9_SetRenderState(dev,
               D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
         IDirect3DDevice9_SetRenderState(dev,
               D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
         IDirect3DDevice9_SetRenderState(dev,
               D3DRS_ALPHABLENDENABLE, true);

         /* Restore menu display vertex buffer state */
         IDirect3DDevice9_SetStreamSource(dev, 0,
               (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
               0, sizeof(Vertex));
         IDirect3DDevice9_SetVertexDeclaration(dev,
               (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);
         return;
      }
      default:
         break;
   }

   /* Determine whether caller provides explicit vertex arrays or
    * expects us to build the quad from draw->x/y/width/height
    * (the single-sprite path used by Ozone and other modern menus).
    * Mirrors the d3d10 vertex_count==1 vs multi-vertex split. */
   has_vertex_data = draw->coords->vertex
      && draw->coords->tex_coord && draw->coords->color;

   if (has_vertex_data)
      vertex_count = draw->coords->vertices;

   if (!has_vertex_data)
   {
      /* Single-sprite path: no explicit vertex arrays provided.
       * Build a quad directly from draw->x/y/width/height in
       * normalized [0,1] space, using DrawPrimitiveUP to avoid
       * any vertex buffer offset/locking issues. */
      D3DCOLOR col[4];
      Vertex quad[4];
      float x1, y1, x2, y2;
      const float *c = draw->coords->color;

      /* Per-vertex colors: map the 16-float color array directly
       * to the 4 quad vertices in order. */
      /* The color array is laid out for bottom-up coordinates
       * (vertex 0,1 = bottom, vertex 2,3 = top) as used by d3d10/d3d11.
       * Since this path un-flips Y (top-down ortho), swap the top and
       * bottom vertex color pairs so gradients render correctly. */
      if (c)
      {
         col[0] = D3DCOLOR_ARGB((int)(c[11]*0xFF), (int)(c[ 8]*0xFF), (int)(c[ 9]*0xFF), (int)(c[10]*0xFF));
         col[1] = D3DCOLOR_ARGB((int)(c[15]*0xFF), (int)(c[12]*0xFF), (int)(c[13]*0xFF), (int)(c[14]*0xFF));
         col[2] = D3DCOLOR_ARGB((int)(c[ 3]*0xFF), (int)(c[ 0]*0xFF), (int)(c[ 1]*0xFF), (int)(c[ 2]*0xFF));
         col[3] = D3DCOLOR_ARGB((int)(c[ 7]*0xFF), (int)(c[ 4]*0xFF), (int)(c[ 5]*0xFF), (int)(c[ 6]*0xFF));
      }
      else
      {
         col[0] = col[1] = col[2] = col[3] = D3DCOLOR_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
      }

      /* Normalize to [0,1] range.
       * Both ozone_draw_icon and gfx_display_draw_quad pre-flip Y
       * (draw.y = height - y - h). The Y-flip here undoes that,
       * then topdown_ortho applies the correct top-down mapping. */
      x1 = draw->x / (float)video_width;
      y1 = ((float)video_height - draw->y - draw->height) / (float)video_height;
      x2 = (draw->x + draw->width)  / (float)video_width;
      y2 = ((float)video_height - draw->y) / (float)video_height;

      /* Apply scale_factor: scale the quad around its center,
       * matching D3D10's geometry shader params.scaling behavior. */
      if (draw->scale_factor && draw->scale_factor != 1.0f)
      {
         float cx = (x1 + x2) * 0.5f;
         float cy = (y1 + y2) * 0.5f;
         float hw = (x2 - x1) * 0.5f * draw->scale_factor;
         float hh = (y2 - y1) * 0.5f * draw->scale_factor;
         x1 = cx - hw;
         y1 = cy - hh;
         x2 = cx + hw;
         y2 = cy + hh;
      }

      quad[0].x = x1; quad[0].y = y1; quad[0].z = 0.5f;
      quad[0].u = 0.0f; quad[0].v = 0.0f; quad[0].color = col[0];

      quad[1].x = x2; quad[1].y = y1; quad[1].z = 0.5f;
      quad[1].u = 1.0f; quad[1].v = 0.0f; quad[1].color = col[1];

      quad[2].x = x1; quad[2].y = y2; quad[2].z = 0.5f;
      quad[2].u = 0.0f; quad[2].v = 1.0f; quad[2].color = col[2];

      quad[3].x = x2; quad[3].y = y2; quad[3].z = 0.5f;
      quad[3].u = 1.0f; quad[3].v = 1.0f; quad[3].color = col[3];

      /* Top-down ortho: maps X [0,1]→[-1,1], Y [0,1]→[1,-1] (Y=0 at top).
       * Row-major layout for mul(vector, matrix) in the stock HLSL vertex shader.
       * Translation goes in the 4th row (D3D convention). */
      {
         static const float topdown_ortho[16] = {
             2.0f,  0.0f, 0.0f, 0.0f,
             0.0f, -2.0f, 0.0f, 0.0f,
             0.0f,  0.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f, 1.0f
         };
         IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev,
               0, topdown_ortho, 4);
      }

      gfx_display_d3d9_bind_texture(draw, d3d);

      IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_COMM_TRIANGLESTRIP,
            2, quad, sizeof(Vertex));

      /* DrawPrimitiveUP unbinds the stream source, re-bind it */
      IDirect3DDevice9_SetStreamSource(dev, 0,
            (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
            0, sizeof(Vertex));
      return;
   }

   /* Multi-vertex path: explicit vertex/tex_coord/color arrays */
   if ((d3d->menu_display.offset + vertex_count)
         > (unsigned)d3d->menu_display.size)
      d3d->menu_display.offset = 0;

   IDirect3DVertexBuffer9_Lock((LPDIRECT3DVERTEXBUFFER9)
            d3d->menu_display.buffer, 0, 0, (void**)&pv,
            D3DLOCK_NOOVERWRITE);
   if (!pv)
      return;

   pv          += d3d->menu_display.offset;
   vertex       = draw->coords->vertex;
   tex_coord    = draw->coords->tex_coord;
   color        = draw->coords->color;

   if (!vertex)
      vertex    = &d3d9_hlsl_vertexes[0];
   if (!tex_coord)
      tex_coord = &d3d9_hlsl_tex_coords[0];

   for (i = 0; i < draw->coords->vertices; i++)
   {
      int colors[4];

      colors[0]   = *color++ * 0xFF;
      colors[1]   = *color++ * 0xFF;
      colors[2]   = *color++ * 0xFF;
      colors[3]   = *color++ * 0xFF;

      pv[i].x     = *vertex++;
      pv[i].y     = *vertex++;
      pv[i].z     = 0.5f;
      pv[i].u     = *tex_coord++;
      pv[i].v     = *tex_coord++;

      pv[i].color =
         D3DCOLOR_ARGB(
               colors[3], /* A */
               colors[0], /* R */
               colors[1], /* G */
               colors[2]  /* B */
               );
   }
   IDirect3DVertexBuffer9_Unlock((LPDIRECT3DVERTEXBUFFER9)
         d3d->menu_display.buffer);

   /* Multi-vertex coords from gfx_display_draw_texture_slice and
    * similar are in bottom-up [0,1] space (Y=0 at bottom), matching
    * D3D10's VIDEO_SHADER_STOCK_BLEND ortho.
    * Use bottom-up ortho: X [0,1]→[-1,1], Y [0,1]→[-1,1]. */
   {
      /* Row-major layout for mul(vector, matrix) in the stock HLSL vertex shader. */
      static const float bottomup_ortho[16] = {
          2.0f,  0.0f, 0.0f, 0.0f,
          0.0f,  2.0f, 0.0f, 0.0f,
          0.0f,  0.0f, 1.0f, 0.0f,
         -1.0f, -1.0f, 0.0f, 1.0f
      };
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev,
            0, bottomup_ortho, 4);
   }

   if (draw && draw->texture)
      gfx_display_d3d9_bind_texture(draw, d3d);

   type  = (D3DPRIMITIVETYPE)gfx_display_prim_to_d3d9_hlsl_enum(draw->prim_type);
   start = d3d->menu_display.offset;
   count = draw->coords->vertices -
         ((draw->prim_type == GFX_DISPLAY_PRIM_TRIANGLESTRIP)
          ? 2 : 0);

   IDirect3DDevice9_DrawPrimitive(dev, type, start, count);

   d3d->menu_display.offset += draw->coords->vertices;
}

static void gfx_display_d3d9_hlsl_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   static float t                        = 0.0f;
   video_coord_array_t *ca               = NULL;
   d3d9_video_t *d3d                     = (d3d9_video_t*)data;

   if (!d3d || !draw)
      return;

   ca                                    = &p_disp->dispca;

   draw->x                               = 0;
   draw->y                               = 0;
   draw->coords                          = NULL;
   draw->matrix_data                     = NULL;

   if (ca)
      draw->coords                       = (struct video_coords*)&ca->coords;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         /* Create a pipeline vertex buffer from the coordinate
          * array data if it doesn't already exist.
          * Adapted from d3d10 gfx_display_d3d10_draw_pipeline. */
         if (!d3d9_hlsl_menu_pipeline_vbo && ca->coords.vertices)
         {
            unsigned i;
            Vertex *verts    = NULL;
            unsigned vcount  = ca->coords.vertices;

            d3d9_hlsl_menu_pipeline_vbo = NULL;
                  if (!SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
                              d3d->dev,
                              vcount * sizeof(Vertex),
                              D3DUSAGE_WRITEONLY, 0,
                              D3DPOOL_DEFAULT,
                              (LPDIRECT3DVERTEXBUFFER9*)&d3d9_hlsl_menu_pipeline_vbo, NULL)))
                     d3d9_hlsl_menu_pipeline_vbo = NULL;

            if (d3d9_hlsl_menu_pipeline_vbo)
            {
               IDirect3DVertexBuffer9_Lock(
                     (LPDIRECT3DVERTEXBUFFER9)d3d9_hlsl_menu_pipeline_vbo,
                     0, 0, (void**)&verts, 0);

               if (verts)
               {
                  for (i = 0; i < vcount; i++)
                  {
                     verts[i].x     = ca->coords.vertex[i * 2 + 0];
                     verts[i].y     = ca->coords.vertex[i * 2 + 1];
                     verts[i].z     = 0.5f;
                     verts[i].u     = 0.0f;
                     verts[i].v     = 0.0f;
                     verts[i].color = D3DCOLOR_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
                  }

                  IDirect3DVertexBuffer9_Unlock(
                        (LPDIRECT3DVERTEXBUFFER9)d3d9_hlsl_menu_pipeline_vbo);
               }
            }
         }

         if (d3d9_hlsl_menu_pipeline_vbo)
         {
            IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
                  (LPDIRECT3DVERTEXBUFFER9)d3d9_hlsl_menu_pipeline_vbo,
                  0, sizeof(Vertex));
         }

         draw->coords->vertices = ca->coords.vertices;

         /* Set pipeline blend state */
         IDirect3DDevice9_SetRenderState(d3d->dev,
               D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
         IDirect3DDevice9_SetRenderState(d3d->dev,
               D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
         IDirect3DDevice9_SetRenderState(d3d->dev,
               D3DRS_ALPHABLENDENABLE, true);
         break;
      }

      case VIDEO_SHADER_MENU_3:
      {
         draw->coords->vertices = 4;
         break;
      }

      default:
         return;
   }

   /* Update time uniform - mirrors d3d10 ubo_values.time increment */
   t += 0.01f;

   {
      hlsl_renderchain_t *_chain = (hlsl_renderchain_t*)d3d->renderchain_data;
      if (_chain)
      {
         d3d9_hlsl_set_param_1f(
               (void*)_chain->stock_shader.vtable,
               d3d->dev, "IN.frame_count", &t);
         d3d9_hlsl_set_param_1f(
               (void*)_chain->stock_shader.ftable,
               d3d->dev, "IN.frame_count", &t);
      }
   }
}

static void gfx_display_d3d9_hlsl_scissor_begin(
      void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   RECT rect;
   d3d9_video_t *d3d9 = (d3d9_video_t*)data;

   if (!d3d9)
      return;

   rect.left          = x;
   rect.top           = y;
   rect.right         = width + x;
   rect.bottom        = height + y;

   IDirect3DDevice9_SetScissorRect(d3d9->dev, &rect);
}

static void gfx_display_d3d9_hlsl_scissor_end(void *data,
      unsigned video_width, unsigned video_height)
{
   RECT rect;
   d3d9_video_t   *d3d9 = (d3d9_video_t*)data;

   if (!d3d9)
      return;

   rect.left            = 0;
   rect.top             = 0;
   rect.right           = video_width;
   rect.bottom          = video_height;

   IDirect3DDevice9_SetScissorRect(d3d9->dev, &rect);
}

gfx_display_ctx_driver_t gfx_display_ctx_d3d9_hlsl = {
   gfx_display_d3d9_hlsl_draw,
   gfx_display_d3d9_hlsl_draw_pipeline,
   gfx_display_d3d9_hlsl_blend_begin,
   gfx_display_d3d9_hlsl_blend_end,
   NULL,                                     /* get_default_mvp        */
   NULL,                                     /* get_default_vertices   */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_D3D9_API,
   GFX_VIDEO_DRIVER_DIRECT3D9_HLSL,
   "d3d9_hlsl",
   true,
   gfx_display_d3d9_hlsl_scissor_begin,
   gfx_display_d3d9_hlsl_scissor_end
};

/*
 * FONT DRIVER
 */

typedef struct
{
   LPDIRECT3DTEXTURE9            texture;
   const font_renderer_driver_t *font_driver;
   void                         *font_data;
   struct font_atlas             *atlas;
   unsigned                      tex_width;
   unsigned                      tex_height;
} d3d9_font_t;

static void *d3d9_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   unsigned i, j;
   d3d9_video_t    *d3d  = (d3d9_video_t*)data;
   d3d9_font_t *font = (d3d9_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   if (!font_renderer_create_default(
            &font->font_driver, &font->font_data,
            font_path, font_size))
   {
      free(font);
      return NULL;
   }

   font->atlas      = font->font_driver->get_atlas(font->font_data);
   font->tex_width  = font->atlas->width;
   font->tex_height = font->atlas->height;

   /* Create an A8R8G8B8 texture from the A8 atlas buffer.
    * D3D9 doesn't universally support D3DFMT_A8
    * as a texture format, so we expand it. */
   font->texture    = (LPDIRECT3DTEXTURE9)
      d3d9_texture_new(d3d->dev,
            font->tex_width, font->tex_height, 1,
            0, (INT32)D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

   if (font->texture)
   {
      D3DLOCKED_RECT lr;

      if (SUCCEEDED(IDirect3DTexture9_LockRect(
                  font->texture, 0, &lr, NULL, 0)))
      {
         for (j = 0; j < font->atlas->height; j++)
         {
            uint32_t       *dst = (uint32_t*)((uint8_t*)lr.pBits + j * lr.Pitch);
            const uint8_t  *src = font->atlas->buffer + j * font->atlas->width;
            for (i = 0; i < font->atlas->width; i++)
               dst[i] = D3DCOLOR_ARGB(src[i], 0xFF, 0xFF, 0xFF);
         }
         IDirect3DTexture9_UnlockRect(font->texture, 0);
      }
   }

   font->atlas->dirty = false;
   return font;
}

static void d3d9_font_free(void *data, bool is_threaded)
{
   d3d9_font_t *font = (d3d9_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (font->texture)
      IDirect3DTexture9_Release(font->texture);

   free(font);
}

static int d3d9_font_get_message_width(void *data,
      const char *msg, size_t msg_len, float scale)
{
   size_t i;
   int delta_x = 0;
   const struct font_glyph *glyph_q = NULL;
   d3d9_font_t *font           = (d3d9_font_t*)data;

   if (!font)
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph;
      const char *msg_tmp = &msg[i];
      unsigned    code    = utf8_walk(&msg_tmp);
      unsigned    skip    = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

/* Emit a single glyph quad (6 vertices for two triangles)
 * into the output vertex array. Returns the number of
 * vertices written (always 6). */
static INLINE unsigned d3d9_font_emit_quad(
      Vertex *pv,
      float x, float y, float w, float h,
      float tex_u, float tex_v, float tex_w, float tex_h,
      D3DCOLOR color)
{
   /* Triangle 1: top-left, top-right, bottom-left */
   pv[0].x     = x;
   pv[0].y     = y;
   pv[0].z     = 0.5f;
   pv[0].u     = tex_u;
   pv[0].v     = tex_v;
   pv[0].color = color;

   pv[1].x     = x + w;
   pv[1].y     = y;
   pv[1].z     = 0.5f;
   pv[1].u     = tex_u + tex_w;
   pv[1].v     = tex_v;
   pv[1].color = color;

   pv[2].x     = x;
   pv[2].y     = y + h;
   pv[2].z     = 0.5f;
   pv[2].u     = tex_u;
   pv[2].v     = tex_v + tex_h;
   pv[2].color = color;

   /* Triangle 2: top-right, bottom-right, bottom-left */
   pv[3].x     = x + w;
   pv[3].y     = y;
   pv[3].z     = 0.5f;
   pv[3].u     = tex_u + tex_w;
   pv[3].v     = tex_v;
   pv[3].color = color;

   pv[4].x     = x + w;
   pv[4].y     = y + h;
   pv[4].z     = 0.5f;
   pv[4].u     = tex_u + tex_w;
   pv[4].v     = tex_v + tex_h;
   pv[4].color = color;

   pv[5].x     = x;
   pv[5].y     = y + h;
   pv[5].z     = 0.5f;
   pv[5].u     = tex_u;
   pv[5].v     = tex_v + tex_h;
   pv[5].color = color;

   return 6;
}

static void d3d9_font_render_msg(
      void *userdata, void *data,
      const char *msg,
      const struct font_params *params)
{
   float x, y, scale, drop_mod, drop_alpha;
   enum text_alignment text_align;
   int drop_x, drop_y;
   unsigned r, g, b, alpha;
   D3DCOLOR color, color_dark;
   struct font_line_metrics *line_metrics = NULL;
   float line_height;
   d3d9_font_t *font  = (d3d9_font_t*)data;
   d3d9_video_t     *d3d   = (d3d9_video_t*)userdata;
   unsigned          width  = 0;
   unsigned          height = 0;

   if (!font || !msg || !*msg)
      return;
   if (!d3d)
      return;

   video_driver_get_size(&width, &height);
   if (!width || !height)
      return;

   if (params)
   {
      x          = params->x;
      y          = params->y;
      scale      = params->scale;
      text_align = params->text_align;
      drop_x     = params->drop_x;
      drop_y     = params->drop_y;
      drop_mod   = params->drop_mod;
      drop_alpha = params->drop_alpha;

      r          = FONT_COLOR_GET_RED(params->color);
      g          = FONT_COLOR_GET_GREEN(params->color);
      b          = FONT_COLOR_GET_BLUE(params->color);
      alpha      = FONT_COLOR_GET_ALPHA(params->color);

      color      = D3DCOLOR_ARGB(alpha, r, g, b);
   }
   else
   {
      settings_t *settings    = config_get_ptr();
      float video_msg_pos_x   = settings->floats.video_msg_pos_x;
      float video_msg_pos_y   = settings->floats.video_msg_pos_y;
      float video_msg_color_r = settings->floats.video_msg_color_r;
      float video_msg_color_g = settings->floats.video_msg_color_g;
      float video_msg_color_b = settings->floats.video_msg_color_b;

      x          = video_msg_pos_x;
      y          = video_msg_pos_y;
      scale      = 1.0f;
      text_align = TEXT_ALIGN_LEFT;

      r          = (unsigned)(video_msg_color_r * 255);
      g          = (unsigned)(video_msg_color_g * 255);
      b          = (unsigned)(video_msg_color_b * 255);
      alpha      = 255;
      color      = D3DCOLOR_ARGB(alpha, r, g, b);

      drop_x     = -2;
      drop_y     = -2;
      drop_mod   = 0.3f;
      drop_alpha = 1.0f;
   }

   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / height;

   /* Enable alpha blending for font rendering */
   IDirect3DDevice9_SetRenderState(d3d->dev,
         D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev,
         D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev,
         D3DRS_ALPHABLENDENABLE, true);

   /* Use top-down ortho: Y=0→top, Y=1→bottom, matching Ozone coords.
    * Row-major layout for mul(vector, matrix) in the stock HLSL vertex shader. */
   {
      static const float topdown_ortho[16] = {
          2.0f,  0.0f, 0.0f, 0.0f,
          0.0f, -2.0f, 0.0f, 0.0f,
          0.0f,  0.0f, 1.0f, 0.0f,
         -1.0f,  1.0f, 0.0f, 1.0f
      };
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev,
            0, topdown_ortho, 4);
   }

   /* Update atlas texture if dirty */
   if (font->atlas->dirty)
   {
      /* If the atlas dimensions changed (grew), we must recreate
       * the texture to match, otherwise glyphs added after init
       * will have wrong UVs or missing data. */
      if (   font->atlas->width  != font->tex_width
          || font->atlas->height != font->tex_height)
      {
         if (font->texture)
            IDirect3DTexture9_Release(font->texture);

         font->tex_width  = font->atlas->width;
         font->tex_height = font->atlas->height;
         font->texture    = (LPDIRECT3DTEXTURE9)
            d3d9_texture_new(d3d->dev,
                  font->tex_width, font->tex_height, 1,
                  0, (INT32)D3DFMT_A8R8G8B8,
                  D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);
      }

      if (font->texture)
      {
         unsigned i, j;
         D3DLOCKED_RECT lr;

         if (SUCCEEDED(IDirect3DTexture9_LockRect(
                     font->texture, 0, &lr, NULL, 0)))
         {
            for (j = 0; j < font->atlas->height; j++)
            {
               uint32_t       *dst = (uint32_t*)((uint8_t*)lr.pBits + j * lr.Pitch);
               const uint8_t  *src = font->atlas->buffer + j * font->atlas->width;
               for (i = 0; i < font->atlas->width; i++)
                  dst[i] = D3DCOLOR_ARGB(src[i], 0xFF, 0xFF, 0xFF);
            }
            IDirect3DTexture9_UnlockRect(font->texture, 0);
         }
      }
      font->atlas->dirty = false;
   }

   /* Render each line, handling newlines like d3d10 does.
    * Callers in d3d9_hlsl_frame wrap in BeginScene/EndScene. */

   /* Set vertex declaration and stock shader for DrawPrimitiveUP */
   IDirect3DDevice9_SetVertexDeclaration(d3d->dev,
         (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);
   {
      hlsl_renderchain_t *_chain = (hlsl_renderchain_t*)d3d->renderchain_data;
      if (_chain)
      {
         IDirect3DDevice9_SetVertexShader(d3d->dev, (LPDIRECT3DVERTEXSHADER9)(&_chain->stock_shader)->vprg);
         IDirect3DDevice9_SetPixelShader(d3d->dev, (LPDIRECT3DPIXELSHADER9)(&_chain->stock_shader)->fprg);
      }
   }

   {
      int lines       = 0;
      bool has_drop   = drop_x || drop_y;
      const char *m   = msg;

      if (has_drop)
      {
         unsigned r_dark     = r * drop_mod;
         unsigned g_dark     = g * drop_mod;
         unsigned b_dark     = b * drop_mod;
         unsigned alpha_dark = alpha * drop_alpha;
         color_dark          = D3DCOLOR_ARGB(alpha_dark, r_dark, g_dark, b_dark);
      }

      for (;;)
      {
         const char *end = m;
         size_t msg_len;

         while (*end && *end != '\n')
            end++;
         msg_len = (size_t)(end - m);

         if (msg_len > 0)
         {
            unsigned i;
            float inv_viewport_w = 1.0f / (float)width;
            float inv_viewport_h = 1.0f / (float)height;
            float inv_tex_w      = 1.0f / (float)font->tex_width;
            float inv_tex_h      = 1.0f / (float)font->tex_height;
            const struct font_glyph *glyph_q = font->font_driver->get_glyph(font->font_data, '?');
            float line_y = y - (float)lines * line_height;

            /* Drop shadow pass */
            if (has_drop)
            {
               float drop_pos_x    = x + scale * drop_x / (float)width;
               float drop_pos_y    = line_y + scale * drop_y / (float)height;
               int lx              = roundf(drop_pos_x * width);
               int ly              = roundf((1.0f - drop_pos_y) * height);
               unsigned vert_count = 0;
               Vertex *verts       = (Vertex*)calloc(msg_len * 6, sizeof(Vertex));

               if (verts)
               {
                  if (text_align == TEXT_ALIGN_RIGHT || text_align == TEXT_ALIGN_CENTER)
                  {
                     int width_accum     = 0;
                     const char *scan    = m;
                     const char *scan_end = m + msg_len;
                     while (scan < scan_end)
                     {
                        const struct font_glyph *glyph;
                        uint32_t code = utf8_walk(&scan);
                        if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
                           if (!(glyph = glyph_q))
                              continue;
                        width_accum += glyph->advance_x;
                     }
                     if (text_align == TEXT_ALIGN_RIGHT)
                        drop_pos_x -= (float)(width_accum * scale) / (float)width;
                     else
                        drop_pos_x -= (float)(width_accum * scale) / (float)width / 2.0f;
                     lx = roundf(drop_pos_x * width);
                  }

                  for (i = 0; i < msg_len; i++)
                  {
                     const struct font_glyph *glyph;
                     const char *msg_tmp = &m[i];
                     unsigned    code    = utf8_walk(&msg_tmp);
                     unsigned    skip    = msg_tmp - &m[i];

                     if (skip > 1)
                        i += skip - 1;

                     if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
                        if (!(glyph = glyph_q))
                           continue;

                     vert_count += d3d9_font_emit_quad(
                           &verts[vert_count],
                           (lx + glyph->draw_offset_x * scale) * inv_viewport_w,
                           (ly + glyph->draw_offset_y * scale) * inv_viewport_h,
                           glyph->width  * scale * inv_viewport_w,
                           glyph->height * scale * inv_viewport_h,
                           glyph->atlas_offset_x * inv_tex_w,
                           glyph->atlas_offset_y * inv_tex_h,
                           glyph->width  * inv_tex_w,
                           glyph->height * inv_tex_h,
                           color_dark);

                     lx += glyph->advance_x * scale;
                     ly += glyph->advance_y * scale;
                  }

                  if (vert_count > 0)
                  {
                     IDirect3DDevice9_SetTexture(d3d->dev, 0,
                           (IDirect3DBaseTexture9*)font->texture);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_ADDRESSU, D3DTADDRESS_COMM_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_ADDRESSV, D3DTADDRESS_COMM_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MINFILTER, D3DTEXF_COMM_LINEAR);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MAGFILTER, D3DTEXF_COMM_LINEAR);

                     IDirect3DDevice9_DrawPrimitiveUP(d3d->dev,
                           D3DPT_TRIANGLELIST,
                           vert_count / 3,
                           verts,
                           sizeof(Vertex));

                     IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
                           (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
                           0, sizeof(Vertex));
                  }

                  free(verts);
               }
            }

            /* Main text pass */
            {
               float pos_x          = x;
               float pos_y          = line_y;
               int lx               = roundf(pos_x * width);
               int ly               = roundf((1.0f - pos_y) * height);
               unsigned vert_count  = 0;
               Vertex *verts        = (Vertex*)calloc(msg_len * 6, sizeof(Vertex));

               if (verts)
               {
                  if (text_align == TEXT_ALIGN_RIGHT || text_align == TEXT_ALIGN_CENTER)
                  {
                     int width_accum     = 0;
                     const char *scan    = m;
                     const char *scan_end = m + msg_len;
                     while (scan < scan_end)
                     {
                        const struct font_glyph *glyph;
                        uint32_t code = utf8_walk(&scan);
                        if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
                           if (!(glyph = glyph_q))
                              continue;
                        width_accum += glyph->advance_x;
                     }
                     if (text_align == TEXT_ALIGN_RIGHT)
                        pos_x -= (float)(width_accum * scale) / (float)width;
                     else
                        pos_x -= (float)(width_accum * scale) / (float)width / 2.0f;
                     lx = roundf(pos_x * width);
                  }

                  for (i = 0; i < msg_len; i++)
                  {
                     const struct font_glyph *glyph;
                     const char *msg_tmp = &m[i];
                     unsigned    code    = utf8_walk(&msg_tmp);
                     unsigned    skip    = msg_tmp - &m[i];

                     if (skip > 1)
                        i += skip - 1;

                     if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
                        if (!(glyph = glyph_q))
                           continue;

                     vert_count += d3d9_font_emit_quad(
                           &verts[vert_count],
                           (lx + glyph->draw_offset_x * scale) * inv_viewport_w,
                           (ly + glyph->draw_offset_y * scale) * inv_viewport_h,
                           glyph->width  * scale * inv_viewport_w,
                           glyph->height * scale * inv_viewport_h,
                           glyph->atlas_offset_x * inv_tex_w,
                           glyph->atlas_offset_y * inv_tex_h,
                           glyph->width  * inv_tex_w,
                           glyph->height * inv_tex_h,
                           color);

                     lx += glyph->advance_x * scale;
                     ly += glyph->advance_y * scale;
                  }

                  if (vert_count > 0)
                  {
                     IDirect3DDevice9_SetTexture(d3d->dev, 0,
                           (IDirect3DBaseTexture9*)font->texture);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_ADDRESSU, D3DTADDRESS_COMM_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_ADDRESSV, D3DTADDRESS_COMM_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MINFILTER, D3DTEXF_COMM_LINEAR);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MAGFILTER, D3DTEXF_COMM_LINEAR);

                     IDirect3DDevice9_DrawPrimitiveUP(d3d->dev,
                           D3DPT_TRIANGLELIST,
                           vert_count / 3,
                           verts,
                           sizeof(Vertex));

                     IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
                           (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
                           0, sizeof(Vertex));
                  }

                  free(verts);
               }
            }
         }

         if (*end != '\n')
            break;
         m = end + 1;
         lines++;
      }
   }

}

static const struct font_glyph *d3d9_font_get_glyph(
      void *data, uint32_t code)
{
   d3d9_font_t *font = (d3d9_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph(
            (void*)font->font_data, code);
   return NULL;
}

static bool d3d9_font_get_line_metrics(
      void *data, struct font_line_metrics **metrics)
{
   d3d9_font_t *font = (d3d9_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t d3d9_font = {
   d3d9_font_init,
   d3d9_font_free,
   d3d9_font_render_msg,
   "d3d9_hlsl",
   d3d9_font_get_glyph,
   NULL, /* bind_block */
   NULL, /* flush */
   d3d9_font_get_message_width,
   d3d9_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

static void *d3d9_hlsl_get_constant_by_name(void* prog, const char *name)
{
   char lbl[64];
   lbl[0] = '\0';
   snprintf(lbl, sizeof(lbl), "$%s", name);
   return d3d9_hlsl_constant_table_get_constant_by_name(prog, NULL, lbl);
}

static INLINE void d3d9_hlsl_set_param_2f(void* prog, LPDIRECT3DDEVICE9 userdata, const char *name, const void *values)
{
   void* param         = (void*)d3d9_hlsl_get_constant_by_name(prog, name);
   if (param)
      d3d9_hlsl_constant_table_set_float_array(userdata, prog, (void*)param, values, 2);
}

static INLINE void d3d9_hlsl_set_param_1f(void* prog, LPDIRECT3DDEVICE9 userdata, const char *name, const void *value)
{
   void* param         = (void*)d3d9_hlsl_get_constant_by_name(prog, name);
   float *val               = (float*)value;
   if (param)
      d3d9_hlsl_constant_table_set_float(prog, userdata, (void*)param, *val);
}

static INLINE void d3d9_hlsl_set_param_matrix(void* prog, LPDIRECT3DDEVICE9 userdata,
      const char *name, const void *values)
{
   void* param         = (void*)d3d9_hlsl_get_constant_by_name(prog, name);
   if (param)
      d3d9_hlsl_constant_table_set_matrix(userdata, prog, (void*)param, ( void*)values);
}

static bool d3d9_hlsl_load_program_from_file(
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      const char *prog)
{
   D3DBlob code_f = NULL;
   D3DBlob code_v = NULL;
   wchar_t wpath[PATH_MAX_LENGTH];

   if (!prog || !*prog)
      return false;

   /* Convert path to wide string for d3d_compile_from_file */
   mbstowcs(wpath, prog, PATH_MAX_LENGTH);

   if (!d3d_compile_from_file(wpath, "main_fragment", "ps_3_0", &code_f))
   {
      RARCH_ERR("[D3D9 HLSL] Could not compile fragment shader program (%s).\n", prog);
      return false;
   }
   if (!d3d_compile_from_file(wpath, "main_vertex", "vs_3_0", &code_v))
   {
      RARCH_ERR("[D3D9 HLSL] Could not compile vertex shader program (%s).\n", prog);
      code_f->lpVtbl->Release(code_f);
      return false;
   }

   pass->ftable = NULL;
   pass->vtable = NULL;

   IDirect3DDevice9_CreatePixelShader(dev,
         (const DWORD*)code_f->lpVtbl->GetBufferPointer(code_f),
         (LPDIRECT3DPIXELSHADER9*)&pass->fprg);
   IDirect3DDevice9_CreateVertexShader(dev,
         (const DWORD*)code_v->lpVtbl->GetBufferPointer(code_v),
         (LPDIRECT3DVERTEXSHADER9*)&pass->vprg);
   code_f->lpVtbl->Release(code_f);
   code_v->lpVtbl->Release(code_v);

   return true;
}

static bool d3d9_hlsl_load_program(
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      const char *prog)
{
   D3DBlob code_f    = NULL;
   D3DBlob code_v    = NULL;
   size_t prog_len   = strlen(prog);

   if (!d3d_compile(prog, prog_len, "stock", "main_fragment", "ps_3_0", &code_f))
   {
      RARCH_ERR("[D3D9 HLSL] Could not compile stock fragment shader.\n");
      return false;
   }
   if (!d3d_compile(prog, prog_len, "stock", "main_vertex", "vs_3_0", &code_v))
   {
      RARCH_ERR("[D3D9 HLSL] Could not compile stock vertex shader.\n");
      code_f->lpVtbl->Release(code_f);
      return false;
   }

   pass->ftable = NULL;
   pass->vtable = NULL;

   IDirect3DDevice9_CreatePixelShader(dev,
         (const DWORD*)code_f->lpVtbl->GetBufferPointer(code_f),
         (LPDIRECT3DPIXELSHADER9*)&pass->fprg);
   IDirect3DDevice9_CreateVertexShader(dev,
         (const DWORD*)code_v->lpVtbl->GetBufferPointer(code_v),
         (LPDIRECT3DVERTEXSHADER9*)&pass->vprg);
   code_f->lpVtbl->Release(code_f);
   code_v->lpVtbl->Release(code_v);

   return true;
}

static void hlsl_d3d9_renderchain_set_shader_params(
      d3d9_renderchain_t *chain,
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      unsigned video_w, unsigned video_h,
      unsigned tex_w, unsigned tex_h,
      unsigned vp_width, unsigned vp_height)
{
   float frame_cnt;
   float video_size[2];
   float texture_size[2];
   float output_size[2];
   void* fprg                 = (void*)pass->ftable;
   void* vprg                 = (void*)pass->vtable;

   video_size[0]                            = video_w;
   video_size[1]                            = video_h;
   texture_size[0]                          = tex_w;
   texture_size[1]                          = tex_h;
   output_size[0]                           = vp_width;
   output_size[1]                           = vp_height;

   d3d9_hlsl_constant_table_set_defaults(dev, fprg);
   d3d9_hlsl_constant_table_set_defaults(dev, vprg);

   d3d9_hlsl_set_param_2f(vprg, dev, "IN.video_size",      &video_size);
   d3d9_hlsl_set_param_2f(fprg, dev, "IN.video_size",      &video_size);
   d3d9_hlsl_set_param_2f(vprg, dev, "IN.texture_size",    &texture_size);
   d3d9_hlsl_set_param_2f(fprg, dev, "IN.texture_size",    &texture_size);
   d3d9_hlsl_set_param_2f(vprg, dev, "IN.output_size",     &output_size);
   d3d9_hlsl_set_param_2f(fprg, dev, "IN.output_size",     &output_size);

   frame_cnt = chain->frame_count;

   if (pass->info.pass->frame_count_mod)
      frame_cnt         = chain->frame_count
         % pass->info.pass->frame_count_mod;

   d3d9_hlsl_set_param_1f(fprg, dev, "IN.frame_count",     &frame_cnt);
   d3d9_hlsl_set_param_1f(vprg, dev, "IN.frame_count",     &frame_cnt);
}

static bool hlsl_d3d9_renderchain_init_shader_fvf(
      d3d9_renderchain_t *chain,
      struct shader_pass *pass)
{
   static const D3DVERTEXELEMENT9 decl[4] =
   {
      {0, offsetof(Vertex, x),  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_POSITION, 0},
      {0, offsetof(Vertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_COLOR, 0},
      {0, offsetof(Vertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_TEXCOORD, 0},
      D3DDECL_END()
   };

   return (SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(chain->dev,
               (const D3DVERTEXELEMENT9*)decl, (IDirect3DVertexDeclaration9**)&pass->vertex_decl)));
}

static bool hlsl_d3d9_renderchain_create_first_pass(
      LPDIRECT3DDEVICE9 dev,
      d3d9_renderchain_t *chain,
      const struct LinkInfo *info,
      unsigned _fmt)
{
   int i;
   struct shader_pass pass       = { 0 };
   unsigned fmt                  =
        (_fmt == RETRO_PIXEL_FORMAT_RGB565)
      ? D3D9_RGB565_FORMAT
      : D3D9_XRGB8888_FORMAT;

   pass.info                     = *info;
   pass.last_width               = 0;
   pass.last_height              = 0;
   pass.attrib_map               = (struct unsigned_vector_list*)
      unsigned_vector_list_new();

   chain->prev.ptr               = 0;

   for (i = 0; i < TEXTURES; i++)
   {
      int32_t filter             = d3d_translate_filter(info->pass->filter);
      chain->prev.last_width[i]  = 0;
      chain->prev.last_height[i] = 0;
      chain->prev.vertex_buf[i]  = NULL;
         if (!SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
                     chain->dev,
                     4 * sizeof(struct Vertex),
                     D3DUSAGE_WRITEONLY, 0,
                     D3DPOOL_DEFAULT,
                     (LPDIRECT3DVERTEXBUFFER9*)&chain->prev.vertex_buf[i], NULL)))
            chain->prev.vertex_buf[i] = NULL;

      if (!chain->prev.vertex_buf[i])
         return false;

      chain->prev.tex[i] = (LPDIRECT3DTEXTURE9)
         d3d9_texture_new(chain->dev,
            info->tex_w, info->tex_h, 1, 0, fmt,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!chain->prev.tex[i])
         return false;

      IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)chain->prev.tex[i]);
      IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_MINFILTER, filter);
      IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_MAGFILTER, filter);
      IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_ADDRESSU,  D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_ADDRESSV,  D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)NULL);
   }

   d3d9_hlsl_load_program_from_file(chain->dev,
         &pass, info->pass->source.path);

   if (!hlsl_d3d9_renderchain_init_shader_fvf(chain, &pass))
      return false;
   shader_pass_vector_list_append(chain->passes, pass);
   return true;
}

static void hlsl_d3d9_renderchain_calc_and_set_shader_mvp(
      hlsl_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   struct d3d_matrix proj, ortho, rot, matrix;

   d3d_matrix_identity(&ortho);
   d3d_matrix_ortho_off_center_lh(&ortho, 0,
         vp_width, 0, vp_height, 0, 1);
   d3d_matrix_identity(&rot);
   d3d_matrix_rotation_z(&rot, rotation * (D3D_PI / 2.0));
   d3d_matrix_multiply(&proj, &ortho, &rot);
   d3d_matrix_transpose(&matrix, &proj);

   d3d9_hlsl_set_param_matrix((void*)pass->vtable,
         chain->chain.dev, "modelViewProj", (const void*)&matrix);
}

static INLINE void d3d9_hlsl_renderchain_set_vertices_on_change(
      d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation
      )
{
   struct Vertex vert[4];
   void *verts       = NULL;
   const struct
      LinkInfo *info = (const struct LinkInfo*)&pass->info;
   float _u          = (float)(width)  / info->tex_w;
   float _v          = (float)(height) / info->tex_h;

   pass->last_width  = width;
   pass->last_height = height;

   /* Copied from D3D8 driver */
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
   vert[1].u        = _u;
   vert[2].v        = _v;
   vert[3].u        = _u;
   vert[3].v        = _v;

   vert[0].color    = 0xFFFFFFFF;
   vert[1].color    = 0xFFFFFFFF;
   vert[2].color    = 0xFFFFFFFF;
   vert[3].color    = 0xFFFFFFFF;

   /* Align texels and vertices.
    *
    * Fixes infamous 'half-texel offset' issue of D3D9
    *	http://msdn.microsoft.com/en-us/library/bb219690%28VS.85%29.aspx.
    */
   /* Maybe we do need something like this left out for now */
#if 0
   for (i = 0; i < 4; i++)
   {
      vert[i].x    -= 0.5f;
      vert[i].y    += 0.5f;
   }
#endif

   IDirect3DVertexBuffer9_Lock(pass->vertex_buf, 0, 0, &verts, 0);
   memcpy(verts, vert, sizeof(vert));
   IDirect3DVertexBuffer9_Unlock(pass->vertex_buf);
}


static void hlsl_d3d9_renderchain_set_vertices(
      d3d9_video_t *d3d,
      hlsl_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      uint64_t frame_count,
      unsigned rotation)
{
   if (pass->last_width != width || pass->last_height != height)
      d3d9_hlsl_renderchain_set_vertices_on_change(&chain->chain,
            pass, width, height, out_width, out_height,
            vp_width, vp_height, rotation);

   hlsl_d3d9_renderchain_calc_and_set_shader_mvp(chain, pass,
         vp_width, vp_height, rotation);
   hlsl_d3d9_renderchain_set_shader_params(&chain->chain,
         chain->chain.dev,
         pass,
         width, height,
         pass->info.tex_w, pass->info.tex_h,
         vp_width, vp_height);
}

static void d3d9_hlsl_deinit_progs(hlsl_renderchain_t *chain)
{
   if (chain->chain.passes->count >= 1)
   {
      size_t i;

      d3d9_vertex_buffer_free(NULL,
            chain->chain.passes->data[0].vertex_decl);

      for (i = 1; i < chain->chain.passes->count; i++)
      {
         if (chain->chain.passes->data[i].tex)
            IDirect3DTexture9_Release(chain->chain.passes->data[i].tex);
         chain->chain.passes->data[i].tex = NULL;
         d3d9_vertex_buffer_free(
               chain->chain.passes->data[i].vertex_buf,
               chain->chain.passes->data[i].vertex_decl);
      }
   }
}

static void d3d9_hlsl_destroy_resources(hlsl_renderchain_t *chain)
{
   size_t i;

   for (i = 0; i < TEXTURES; i++)
   {
      if (chain->chain.prev.tex[i])
         IDirect3DTexture9_Release(chain->chain.prev.tex[i]);
      if (chain->chain.prev.vertex_buf[i])
         d3d9_vertex_buffer_free(chain->chain.prev.vertex_buf[i], NULL);
   }

   d3d9_hlsl_deinit_progs(chain);

   for (i = 0; i < chain->chain.luts->count; i++)
   {
      if (chain->chain.luts->data[i].tex)
         IDirect3DTexture9_Release(chain->chain.luts->data[i].tex);
   }
}

static void hlsl_d3d9_renderchain_free(void *data)
{
   hlsl_renderchain_t *chain = (hlsl_renderchain_t*)data;

   if (!chain)
      return;

   d3d9_hlsl_destroy_resources(chain);
   d3d9_renderchain_destroy_passes_and_luts(&chain->chain);
   free(chain);
}

static bool hlsl_d3d9_renderchain_init(
      d3d9_video_t *d3d,
      hlsl_renderchain_t *chain,
      LPDIRECT3DDEVICE9 dev,
      const D3DVIEWPORT9 *out_vp,
      const struct LinkInfo *info,
      unsigned fmt
      )
{
   chain->chain.dev                         = dev;
   chain->chain.out_vp                      = (D3DVIEWPORT9*)out_vp;
   chain->chain.frame_count                 = 0;
   chain->chain.pixel_size                  = (fmt == RETRO_PIXEL_FORMAT_RGB565) ? 2 : 4;

   if (!hlsl_d3d9_renderchain_create_first_pass(dev, &chain->chain, info, fmt))
      return false;
   if (!d3d9_hlsl_load_program(chain->chain.dev, &chain->stock_shader, stock_hlsl_program))
      return false;

   IDirect3DDevice9_SetVertexShader(dev, (LPDIRECT3DVERTEXSHADER9)(&chain->stock_shader)->vprg);
   IDirect3DDevice9_SetPixelShader(dev, (LPDIRECT3DPIXELSHADER9)(&chain->stock_shader)->fprg);

   return true;
}

static void hlsl_d3d9_renderchain_render_pass(
      hlsl_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned pass_index)
{
   /* Currently we override the passes shader program
      with the stock shader as at least the last pass
      is not setup correctly */
#if 0
   IDirect3DDevice9_SetVertexShader(chain->chain.dev, (LPDIRECT3DVERTEXSHADER9)(pass)->vprg);

   IDirect3DDevice9_SetPixelShader(chain->chain.dev, (LPDIRECT3DPIXELSHADER9)(pass)->fprg);
#else
   IDirect3DDevice9_SetVertexShader(chain->chain.dev, (LPDIRECT3DVERTEXSHADER9)(&chain->stock_shader)->vprg);

   IDirect3DDevice9_SetPixelShader(chain->chain.dev, (LPDIRECT3DPIXELSHADER9)(&chain->stock_shader)->fprg);
#endif

   IDirect3DDevice9_SetTexture(chain->chain.dev, 0,
         (IDirect3DBaseTexture9*)pass->tex);

   /* D3D8 sets the sampler address modes -
      I've left them out for the time being
      but maybe this is a bug in d3d9 */
#if 0
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
#endif
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_MINFILTER,
         d3d_translate_filter(pass->info.pass->filter));
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_MAGFILTER,
         d3d_translate_filter(pass->info.pass->filter));

   IDirect3DDevice9_SetVertexDeclaration(
         chain->chain.dev, pass->vertex_decl);
   IDirect3DDevice9_SetStreamSource(
         chain->chain.dev, 0, pass->vertex_buf,
         0,sizeof(struct Vertex));

#if 0
   /* Set orig texture. */
   d3d9_hlsl_renderchain_bind_orig(chain, chain->dev, pass);

   /* Set prev textures. */
   d3d9_hlsl_renderchain_bind_prev(chain, chain->dev, pass);

   /* Set lookup textures */
   for (i = 0; i < chain->luts->count; i++)
   {
      CGparameter vparam;
      CGparameter fparam = d3d9_hlsl_get_constant_by_name(
            pass->fprg, chain->luts->data[i].id);
      int bound_index    = -1;

      if (fparam)
      {
         unsigned index  = cgGetParameterResourceIndex(fparam);
         bound_index     = index;

         d3d9_renderchain_add_lut_internal(chain, index, i);
      }

      vparam = d3d9_hlsl_get_constant_by_name(pass->vprg,
            chain->luts->data[i].id);

      if (vparam)
      {
         unsigned index = cgGetParameterResourceIndex(vparam);
         if (index != (unsigned)bound_index)
            d3d9_renderchain_add_lut_internal(chain, index, i);
      }
   }

   /* We only bother binding passes which are two indices behind. */
   if (pass_index >= 3)
      d3d9_hlsl_renderchain_bind_pass(chain, chain->chain.dev,
            pass, pass_index);
#endif

   IDirect3DDevice9_BeginScene(chain->chain.dev);
   IDirect3DDevice9_DrawPrimitive(chain->chain.dev,
         D3DPT_TRIANGLESTRIP, 0, 2);
   IDirect3DDevice9_EndScene(chain->chain.dev);

   /* So we don't render with linear filter into render targets,
    * which apparently looked odd (too blurry). */
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

   d3d9_renderchain_unbind_all(&chain->chain);
}

static void d3d9_hlsl_blit_to_texture(
      LPDIRECT3DTEXTURE9 tex, const void *frame,
      unsigned tex_width,  unsigned tex_height,
      unsigned width,      unsigned height,
      unsigned last_width, unsigned last_height,
      unsigned pitch, unsigned pixel_size)
{
   unsigned y;
   D3DLOCKED_RECT d3dlr;
   d3dlr.Pitch  = 0;
   d3dlr.pBits  = NULL;

   if ((last_width != width || last_height != height))
   {
      IDirect3DTexture9_LockRect(tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
      memset(d3dlr.pBits, 0, tex_height * d3dlr.Pitch);
      IDirect3DTexture9_UnlockRect((LPDIRECT3DTEXTURE9)tex, 0);
   }

   IDirect3DTexture9_LockRect(tex, 0, &d3dlr, NULL, 0);
   for (y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t      *out = (uint8_t*)d3dlr.pBits   + y * d3dlr.Pitch;
      memcpy(out, in, width * pixel_size);
   }
   IDirect3DTexture9_UnlockRect(tex, 0);
}

static void hlsl_d3d9_renderchain_render(
      d3d9_video_t *d3d,
      const void *frame,
      unsigned width, unsigned height,
      unsigned pitch, unsigned rotation)
{
   LPDIRECT3DSURFACE9 back_buffer, target;
   unsigned i, current_width, current_height,
          out_width = 0, out_height = 0;
   struct shader_pass *last_pass    = NULL;
   struct shader_pass *first_pass   = NULL;
   hlsl_renderchain_t *chain        = (hlsl_renderchain_t*)
      d3d->renderchain_data;

   d3d9_renderchain_start_render(&chain->chain);

   current_width                  = width;
   current_height                 = height;

   first_pass                     = (struct shader_pass*)
      &chain->chain.passes->data[0];

   d3d9_convert_geometry(
         &first_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->chain.out_vp);

   d3d9_hlsl_blit_to_texture(first_pass->tex,
         frame,
         first_pass->info.tex_w,
         first_pass->info.tex_h,
         width,
         height,
         first_pass->last_width,
         first_pass->last_height,
         pitch,
         chain->chain.pixel_size);

   /* Grab back buffer. */
   d3d9_device_get_render_target(
         chain->chain.dev, 0, (void**)&back_buffer);

   /* In-between render target passes. */
   for (i = 0; i < chain->chain.passes->count - 1; i++)
   {
      D3DVIEWPORT9   viewport        = {0};
      struct shader_pass *from_pass  = (struct shader_pass*)
         &chain->chain.passes->data[i];
      struct shader_pass *to_pass    = (struct shader_pass*)
         &chain->chain.passes->data[i + 1];

      IDirect3DTexture9_GetSurfaceLevel(
		      (LPDIRECT3DTEXTURE9)to_pass->tex, 0, (IDirect3DSurface9**)&target);
      IDirect3DDevice9_SetRenderTarget(chain->chain.dev, 0, target);

      d3d9_convert_geometry(&from_pass->info,
            &out_width, &out_height,
            current_width, current_height, chain->chain.out_vp);

      /* Clear out whole FBO. */
      viewport.Width  = to_pass->info.tex_w;
      viewport.Height = to_pass->info.tex_h;
      viewport.MinZ   = 0.0f;
      viewport.MaxZ   = 1.0f;

      IDirect3DDevice9_SetViewport(
            chain->chain.dev, (D3DVIEWPORT9*)&viewport);
      IDirect3DDevice9_Clear(
            chain->chain.dev, 0, 0, D3DCLEAR_TARGET,
            0, 1, 0);

      viewport.Width  = out_width;
      viewport.Height = out_height;

      IDirect3DDevice9_SetViewport(
            chain->chain.dev, (D3DVIEWPORT9*)&viewport);

      hlsl_d3d9_renderchain_set_vertices(
            d3d,
            chain, from_pass,
            current_width, current_height,
            out_width, out_height,
            out_width, out_height,
            chain->chain.frame_count, 0);

      hlsl_d3d9_renderchain_render_pass(chain,
            from_pass,
            i + 1);

      current_width  = out_width;
      current_height = out_height;
      IDirect3DSurface9_Release(target);
   }

   /* Final pass */
   IDirect3DDevice9_SetRenderTarget(chain->chain.dev, 0, back_buffer);

   last_pass = (struct shader_pass*)&chain->chain.passes->
      data[chain->chain.passes->count - 1];

   d3d9_convert_geometry(&last_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->chain.out_vp);

   IDirect3DDevice9_SetViewport(
         chain->chain.dev, (D3DVIEWPORT9*)chain->chain.out_vp);

   hlsl_d3d9_renderchain_set_vertices(
         d3d,
         chain, last_pass,
         current_width, current_height,
         out_width, out_height,
         chain->chain.out_vp->Width,
         chain->chain.out_vp->Height,
         chain->chain.frame_count, rotation);

   hlsl_d3d9_renderchain_render_pass(chain, last_pass,
         chain->chain.passes->count);

   chain->chain.frame_count++;

   if (back_buffer)
      IDirect3DSurface9_Release(back_buffer);

   d3d9_renderchain_end_render(&chain->chain);
   IDirect3DDevice9_SetVertexShader(chain->chain.dev, (LPDIRECT3DVERTEXSHADER9)(&chain->stock_shader)->vprg);

   IDirect3DDevice9_SetPixelShader(chain->chain.dev, (LPDIRECT3DPIXELSHADER9)(&chain->stock_shader)->fprg);
   hlsl_d3d9_renderchain_calc_and_set_shader_mvp(
         chain, &chain->stock_shader,
         chain->chain.out_vp->Width,
         chain->chain.out_vp->Height, 0);
}

static bool hlsl_d3d9_renderchain_add_pass(
      hlsl_renderchain_t *chain,
      const struct LinkInfo *info)
{
   struct shader_pass pass;

   pass.info                   = *info;
   pass.last_width             = 0;
   pass.last_height            = 0;
   pass.attrib_map             = (struct unsigned_vector_list*)
      unsigned_vector_list_new();
   pass.pool                   = D3DPOOL_DEFAULT;

   d3d9_hlsl_load_program_from_file(
         chain->chain.dev, &pass, info->pass->source.path);

   if (hlsl_d3d9_renderchain_init_shader_fvf(&chain->chain, &pass))
      return d3d9_renderchain_add_pass(&chain->chain, &pass,
            info);
   return false;
}

/* END HLSL RENDERCHAIN */

static uint32_t d3d9_hlsl_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_HLSL);

   return flags;
}

static void d3d9_hlsl_deinitialize(d3d9_video_t *d3d)
{
   font_driver_free_osd();

   hlsl_d3d9_renderchain_free(d3d->renderchain_data);

   d3d9_vertex_buffer_free(d3d->menu_display.buffer,
         d3d->menu_display.decl);

   if (d3d9_hlsl_menu_pipeline_vbo)
   {
      IDirect3DVertexBuffer9_Release(d3d9_hlsl_menu_pipeline_vbo);
      d3d9_hlsl_menu_pipeline_vbo = NULL;
   }

   d3d->renderchain_data    = NULL;
   d3d->menu_display.buffer = NULL;
   d3d->menu_display.decl   = NULL;
}

static bool d3d9_hlsl_init_base(
      d3d9_video_t *d3d, const video_info_t *info)
{
   D3DPRESENT_PARAMETERS d3dpp;
#ifndef _XBOX
   HWND focus_window  = win32_get_window();
#endif

   memset(&d3dpp, 0, sizeof(d3dpp));

   g_pD3D9 = (LPDIRECT3D9)d3d9_create();

   /* this needs g_pD3D9 created first */
   d3d9_make_d3dpp(d3d, info, &d3dpp);

   if (!g_pD3D9)
      return false;
   if (!d3d9_create_device(&d3d->dev, &d3dpp,
            g_pD3D9,
            focus_window,
            d3d->cur_mon_id)
      )
      return false;
   return true;
}

static bool renderchain_d3d_hlsl_init_first(
      enum gfx_ctx_api api,
      void **renderchain_handle)
{
   hlsl_renderchain_t *renderchain =
      (hlsl_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return false;

   d3d9_init_renderchain(&renderchain->chain);

   *renderchain_handle = renderchain;

   return true;
}

static void d3d9_hlsl_log_info(const struct LinkInfo *info)
{
   RARCH_LOG("[D3D9] Render pass info:\n");
   RARCH_LOG("\tTexture width: %u\n", info->tex_w);
   RARCH_LOG("\tTexture height: %u\n", info->tex_h);

   RARCH_LOG("\tScale type (X): ");

   switch (info->pass->fbo.type_x)
   {
      case RARCH_SCALE_INPUT:
         RARCH_LOG("Relative @ %fx\n", info->pass->fbo.scale_x);
         break;

      case RARCH_SCALE_VIEWPORT:
         RARCH_LOG("Viewport @ %fx\n", info->pass->fbo.scale_x);
         break;

      case RARCH_SCALE_ABSOLUTE:
         RARCH_LOG("Absolute @ %u px\n", info->pass->fbo.abs_x);
         break;
   }

   RARCH_LOG("\tScale type (Y): ");

   switch (info->pass->fbo.type_y)
   {
      case RARCH_SCALE_INPUT:
         RARCH_LOG("Relative @ %fx\n", info->pass->fbo.scale_y);
         break;

      case RARCH_SCALE_VIEWPORT:
         RARCH_LOG("Viewport @ %fx\n", info->pass->fbo.scale_y);
         break;

      case RARCH_SCALE_ABSOLUTE:
         RARCH_LOG("Absolute @ %u px\n", info->pass->fbo.abs_y);
         break;
   }

   RARCH_LOG("\tBilinear filter: %s\n",
         info->pass->filter == RARCH_FILTER_LINEAR ? "true" : "false");
}

static bool d3d9_init_multipass(d3d9_video_t *d3d, const char *shader_path)
{
   unsigned i;
   struct video_shader_pass *pass = NULL;

   memset(&d3d->shader, 0, sizeof(d3d->shader));

   if (!video_shader_load_preset_into_shader(shader_path, &d3d->shader))
      return false;

   RARCH_LOG("[D3D9] Found %u shaders.\n", d3d->shader.passes);

   for (i = 0; i < d3d->shader.passes; i++)
   {
      if (d3d->shader.pass[i].fbo.flags & FBO_SCALE_FLAG_VALID)
         continue;

      d3d->shader.pass[i].fbo.scale_y = 1.0f;
      d3d->shader.pass[i].fbo.scale_x = 1.0f;
      d3d->shader.pass[i].fbo.type_x  = RARCH_SCALE_INPUT;
      d3d->shader.pass[i].fbo.type_y  = RARCH_SCALE_INPUT;
   }

   if (d3d->shader.passes < GFX_MAX_SHADERS &&
      (d3d->shader.pass[d3d->shader.passes - 1].fbo.flags & FBO_SCALE_FLAG_VALID))
   {
      d3d->shader.passes++;
      pass              = (struct video_shader_pass*)
         &d3d->shader.pass[d3d->shader.passes - 1];

      pass->fbo.scale_x = 1.0f;
      pass->fbo.scale_y = 1.0f;
      pass->fbo.type_x  = RARCH_SCALE_VIEWPORT;
      pass->fbo.type_y  = RARCH_SCALE_VIEWPORT;
      pass->filter      = RARCH_FILTER_UNSPEC;
   }
   else
   {
      pass              = (struct video_shader_pass*)
         &d3d->shader.pass[d3d->shader.passes - 1];

      pass->fbo.scale_x = 1.0f;
      pass->fbo.scale_y = 1.0f;
      pass->fbo.type_x  = RARCH_SCALE_VIEWPORT;
      pass->fbo.type_y  = RARCH_SCALE_VIEWPORT;
   }

   return true;
}

static bool d3d9_hlsl_process_shader(d3d9_video_t *d3d)
{
   struct video_shader_pass *pass   = NULL;
   const char *shader_path = d3d->shader_path;
   if (shader_path && *shader_path)
   {
      RARCH_ERR("[D3D9] Failed to parse shader preset.\n");
      return d3d9_init_multipass(d3d, shader_path);
   }

   memset(&d3d->shader, 0, sizeof(d3d->shader));
   d3d->shader.passes               = 1;

   pass = (struct video_shader_pass*)&d3d->shader.pass[0];

   pass->fbo.flags                 |= FBO_SCALE_FLAG_VALID;
   pass->fbo.scale_y                = 1.0;
   pass->fbo.type_y                 = RARCH_SCALE_VIEWPORT;
   pass->fbo.scale_x                = pass->fbo.scale_y;
   pass->fbo.type_x                 = pass->fbo.type_y;

   if (d3d->shader_path && *d3d->shader_path)
      strlcpy(pass->source.path, d3d->shader_path,
            sizeof(pass->source.path));
   return true;
}


static bool d3d9_hlsl_init_chain(d3d9_video_t *d3d,
      unsigned input_scale, bool rgb32)
{
   struct LinkInfo link_info;
#ifndef _XBOX
   unsigned current_width, current_height, out_width, out_height;
#endif
   unsigned i           = 0;
   settings_t *settings = config_get_ptr();
   bool video_smooth    = settings->bools.video_smooth;

   /* Setup information for first pass. */
   link_info.pass       = NULL;
   link_info.tex_w      = input_scale * RARCH_SCALE_BASE;
   link_info.tex_h      = input_scale * RARCH_SCALE_BASE;
   link_info.pass       = &d3d->shader.pass[0];

   if (!renderchain_d3d_hlsl_init_first(GFX_CTX_DIRECT3D9_API,
            &d3d->renderchain_data))
      return false;
   if (!d3d->renderchain_data)
      return false;

   RARCH_LOG("[D3D9 HLSL] Using HLSL shader backend.\n");

   if (
         !hlsl_d3d9_renderchain_init(
            d3d, (hlsl_renderchain_t*)d3d->renderchain_data,
            d3d->dev, &d3d->out_vp, &link_info,
            rgb32
            ? RETRO_PIXEL_FORMAT_XRGB8888
            : RETRO_PIXEL_FORMAT_RGB565
            )
      )
      return false;

   d3d9_hlsl_log_info(&link_info);

#ifndef _XBOX
   current_width  = link_info.tex_w;
   current_height = link_info.tex_h;
   out_width      = 0;
   out_height     = 0;

   for (i = 1; i < d3d->shader.passes; i++)
   {
      d3d9_convert_geometry(
            &link_info,
            &out_width, &out_height,
            current_width, current_height, &d3d->out_vp);

      link_info.pass  = &d3d->shader.pass[i];
      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      current_width   = out_width;
      current_height  = out_height;

      if (!hlsl_d3d9_renderchain_add_pass(
               (hlsl_renderchain_t*)d3d->renderchain_data, &link_info))
      {
         RARCH_ERR("[D3D9 HLSL] Failed to add pass.\n");
         return false;
      }
      d3d9_hlsl_log_info(&link_info);
   }
#endif

   {
      hlsl_renderchain_t *_chain = (hlsl_renderchain_t*)d3d->renderchain_data;
      d3d9_renderchain_t *chain   = (d3d9_renderchain_t*)&_chain->chain;

      for (i = 0; i < d3d->shader.luts; i++)
      {
         if (!d3d9_renderchain_add_lut(
                  chain,
                  d3d->shader.lut[i].id,
                  d3d->shader.lut[i].path,
                  d3d->shader.lut[i].filter == RARCH_FILTER_UNSPEC
                  ? video_smooth
                  : (d3d->shader.lut[i].filter == RARCH_FILTER_LINEAR)))
         {
            RARCH_ERR("[D3D9 HLSL] Failed to init LUTs.\n");
            return false;
         }
      }
   }

   return true;
}

static void d3d9_set_font_rect(
      d3d9_video_t *d3d,
      const struct font_params *params)
{
   settings_t *settings           = config_get_ptr();
   float pos_x                    = settings->floats.video_msg_pos_x;
   float pos_y                    = settings->floats.video_msg_pos_y;
   float font_size                = settings->floats.video_font_size;

   if (params)
   {
      pos_x                       = params->x;
      pos_y                       = params->y;
      font_size                  *= params->scale;
   }

   d3d->font_rect.left            = d3d->video_info.width * pos_x;
   d3d->font_rect.right           = d3d->video_info.width;
   d3d->font_rect.top             = (1.0f - pos_y) * d3d->video_info.height - font_size;
   d3d->font_rect.bottom          = d3d->video_info.height;

   d3d->font_rect_shifted         = d3d->font_rect;
   d3d->font_rect_shifted.left   -= 2;
   d3d->font_rect_shifted.right  -= 2;
   d3d->font_rect_shifted.top    += 2;
   d3d->font_rect_shifted.bottom += 2;
}

static void d3d9_hlsl_set_viewport(void *data,
      unsigned width, unsigned height,
      bool force_full,
      bool allow_rotate)
{
   d3d9_video_t *d3d   = (d3d9_video_t*)data;
   float translate_x   = d3d->translate_x;
   float translate_y   = d3d->translate_y;
   int x               = 0;
   int y               = 0;
   struct video_viewport vp;

   video_driver_get_size(&width, &height);

   vp.full_width  = width;
   vp.full_height = height;
   video_driver_update_viewport(&vp, force_full, d3d->keep_aspect, true);

   x      = vp.x;
   y      = vp.y;
   width  = vp.width;
   height = vp.height;

   /* D3D doesn't support negative X/Y viewports ... */
   if (x < 0)
   {
      if (!force_full)
         d3d->translate_x = x * 2;
      x = 0;
   }
   else if (!force_full)
      d3d->translate_x = 0;

   if (y < 0)
   {
      if (!force_full)
         d3d->translate_y = y * 2;
      y = 0;
   }
   else if (!force_full)
      d3d->translate_y = 0;

   if (!force_full)
   {
      if (translate_x != d3d->translate_x || translate_y != d3d->translate_y)
         d3d->needs_restore = true;
   }

   d3d->out_vp.X      = x;
   d3d->out_vp.Y      = y;
   d3d->out_vp.Width  = width;
   d3d->out_vp.Height = height;
   d3d->out_vp.MinZ   = 0.0f;
   d3d->out_vp.MaxZ   = 1.0f;

   d3d9_set_font_rect(d3d, NULL);
}

static void d3d9_hlsl_set_osd_msg(void *data,
      const char *msg,
      const struct font_params *params, void *font)
{
   d3d9_video_t          *d3d = (d3d9_video_t*)data;
   LPDIRECT3DDEVICE9     dev  = d3d->dev;

   d3d9_set_font_rect(d3d, params);
   IDirect3DDevice9_BeginScene(dev);
   font_driver_render_msg(d3d, msg, params, font);
   IDirect3DDevice9_EndScene(dev);
}

static bool d3d9_hlsl_initialize(
      d3d9_video_t *d3d, const video_info_t *info)
{
   unsigned width, height;
   bool ret             = true;

   if (!g_pD3D9)
      ret               = d3d9_hlsl_init_base(d3d, info);
   else if (d3d->needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;

      d3d9_make_d3dpp(d3d, info, &d3dpp);
      if (!d3d9_reset(d3d->dev, &d3dpp))
      {
         d3d9_hlsl_deinitialize(d3d);
         IDirect3D9_Release(g_pD3D9);
         g_pD3D9 = NULL;

         ret     = d3d9_hlsl_init_base(d3d, info);
         if (ret)
            RARCH_LOG("[D3D9 HLSL] Recovered from dead state.\n");
      }

#ifdef HAVE_MENU
      menu_driver_init(info->is_threaded);
#endif
   }

   if (!ret)
      return ret;

   if (!d3d9_hlsl_init_chain(d3d, info->input_scale, info->rgb32))
   {
      RARCH_ERR("[D3D9 HLSL] Failed to initialize render chain.\n");
      return false;
   }

   video_driver_get_size(&width, &height);
   d3d9_hlsl_set_viewport(d3d,
      width, height, false, true);

   font_driver_init_osd(d3d, info,
         false,
         info->is_threaded,
         FONT_DRIVER_RENDER_D3D9_API);

   {
      static const D3DVERTEXELEMENT9 VertexElements[4] = {
         {0, offsetof(Vertex, x),
            D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_POSITION,0},
         {0, offsetof(Vertex, u),
            D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_TEXCOORD,0},
         {0, offsetof(Vertex, color),
            D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_COLOR,   0},
         D3DDECL_END()
      };
      if (!SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(d3d->dev,
               (const D3DVERTEXELEMENT9*)VertexElements, (IDirect3DVertexDeclaration9**)&d3d->menu_display.decl)))
         return false;
   }

   d3d->menu_display.offset = 0;
   d3d->menu_display.size   = 8192;
   d3d->menu_display.buffer = NULL;
   if (!SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
               d3d->dev,
               d3d->menu_display.size * sizeof(Vertex),
               D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0,
               D3DPOOL_DEFAULT,
               (LPDIRECT3DVERTEXBUFFER9*)&d3d->menu_display.buffer, NULL)))
      d3d->menu_display.buffer = NULL;

   if (!d3d->menu_display.buffer)
      return false;

   /* Create a 1x1 white fallback texture for draws with no texture
    * (e.g. widget background quads when gfx_white_texture is not loaded) */
   if (!d3d9_hlsl_white_texture)
   {
      IDirect3DDevice9_CreateTexture(d3d->dev, 1, 1, 1,
            0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
            (LPDIRECT3DTEXTURE9*)&d3d9_hlsl_white_texture, NULL);
      if (d3d9_hlsl_white_texture)
      {
         D3DLOCKED_RECT lr;
         if (SUCCEEDED(IDirect3DTexture9_LockRect(
                     d3d9_hlsl_white_texture, 0, &lr, NULL, 0)))
         {
            *((DWORD*)lr.pBits) = 0xFFFFFFFF; /* ARGB white */
            IDirect3DTexture9_UnlockRect(d3d9_hlsl_white_texture, 0);
         }
      }
   }

   d3d_matrix_identity(&d3d->mvp_transposed);
   d3d_matrix_ortho_off_center_lh(&d3d->mvp_transposed, 0, 1, 0, 1, 0, 1);
   d3d->mvp = d3d->mvp_transposed;

   if (d3d->translate_x)
   {
      struct d3d_matrix *pout = (struct d3d_matrix*)&d3d->mvp;
      float vp_x = -(d3d->translate_x/(float)d3d->out_vp.Width);
      pout->m[3][0] = -1.0f + vp_x - 2.0f * 1 / (0 - 1);
   }

   if (d3d->translate_y)
   {
      struct d3d_matrix *pout = (struct d3d_matrix*)&d3d->mvp;
      float vp_y = -(d3d->translate_y/(float)d3d->out_vp.Height);
      pout->m[3][1] = 1.0f + vp_y + 2.0f * 1 / (0 - 1);
   }

   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SCISSORTESTENABLE, TRUE);

   return true;
}

static bool d3d9_hlsl_restore(d3d9_video_t *d3d)
{
   d3d9_hlsl_deinitialize(d3d);

   if (!d3d9_hlsl_initialize(d3d, &d3d->video_info))
   {
      RARCH_ERR("[D3D9 HLSL] Restore error.\n");
      return false;
   }

   d3d->needs_restore = false;

   return true;
}

static bool d3d9_hlsl_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return false;

   if ((d3d->shader_path && *d3d->shader_path))
      free(d3d->shader_path);
   d3d->shader_path = NULL;

   switch (type)
   {
      case RARCH_SHADER_CG:
      case RARCH_SHADER_HLSL:
         if (path && *path)
            d3d->shader_path = strdup(path);

         break;
      case RARCH_SHADER_NONE:
         break;
      default:
         RARCH_WARN("[D3D9 HLSL] Only CG shaders are supported. Falling back to stock.\n");
   }

   if (!d3d9_hlsl_process_shader(d3d) || !d3d9_hlsl_restore(d3d))
   {
      RARCH_ERR("[D3D9 HLSL] Failed to set shader.\n");
      return false;
   }

   return true;
}

static bool d3d9_hlsl_init_internal(d3d9_video_t *d3d,
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
   d3d->windowClass.lpfnWndProc = wnd_proc_d3d_common;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
      d3d->windowClass.lpfnWndProc = wnd_proc_d3d_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
      d3d->windowClass.lpfnWndProc = wnd_proc_d3d_winraw;
#endif
   win32_window_init(&d3d->windowClass, true, NULL);
#endif

#ifdef HAVE_MONITOR
   win32_monitor_info(&current_mon, &hm_to_use, &d3d->cur_mon_id);

   mon_rect              = current_mon.rcMonitor;
   g_win32_resize_width  = info->width;
   g_win32_resize_height = info->height;

   windowed_full         = settings->bools.video_windowed_fullscreen;

   full_x                = (windowed_full || info->width  == 0)
      ? (unsigned)(mon_rect.right  - mon_rect.left)
      : info->width;
   full_y                = (windowed_full || info->height == 0)
      ? (unsigned)(mon_rect.bottom - mon_rect.top)
      : info->height;
#else
   d3d9_get_video_size(d3d, &full_x, &full_y);
#endif
   {
      unsigned new_width  = info->fullscreen ? full_x : info->width;
      unsigned new_height = info->fullscreen ? full_y : info->height;
      video_driver_set_size(new_width, new_height);
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

   d3d->video_info = *info;

   if (!d3d9_hlsl_initialize(d3d, &d3d->video_info))
      return false;

   d3d9_hlsl_fake_context.get_flags   = d3d9_hlsl_get_flags;
#ifndef _XBOX_
   d3d9_hlsl_fake_context.get_metrics = win32_get_metrics;
#endif
   video_context_driver_set(&d3d9_hlsl_fake_context);
   {
      const char *shader_preset   = video_shader_get_current_shader_preset();
      enum rarch_shader_type type = video_shader_parse_type(shader_preset);

      d3d9_hlsl_set_shader(d3d, type, shader_preset);
   }

   d3d_input_driver(settings->arrays.input_joypad_driver,
      settings->arrays.input_joypad_driver, input, input_data);

   {
      char version_str[128];
      D3DADAPTER_IDENTIFIER9 ident = {0};

      IDirect3D9_GetAdapterIdentifier(g_pD3D9, 0, 0, &ident);

      version_str[0] = '\0';

      snprintf(version_str, sizeof(version_str), "%u.%u.%u.%u",
            HIWORD(ident.DriverVersion.HighPart),
            LOWORD(ident.DriverVersion.HighPart),
            HIWORD(ident.DriverVersion.LowPart),
            LOWORD(ident.DriverVersion.LowPart));

      RARCH_LOG("[D3D9 HLSL] Using GPU: \"%s\".\n", ident.Description);
      RARCH_LOG("[D3D9 HLSL] GPU API Version: %s.\n", version_str);
      video_driver_set_gpu_api_version_string(version_str);
   }

   return true;
}

static void *d3d9_hlsl_init(const video_info_t *info,
      input_driver_t **input, void **input_data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)calloc(1, sizeof(*d3d));

   if (!d3d)
      return NULL;
   if (!d3d9_initialize_symbols(GFX_CTX_DIRECT3D9_API))
      goto error;

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

   if (!d3d9_hlsl_init_internal(d3d, info, input, input_data))
      goto error;

   d3d->keep_aspect       = info->force_aspect;

   return d3d;

error:
   RARCH_ERR("[D3D9 HLSL] Failed to init D3D.\n");
   free(d3d);
   return NULL;
}

static void d3d9_hlsl_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned width, height;
   d3d9_video_t *d3d   = (d3d9_video_t*)data;

   video_driver_get_size(&width, &height);

   vp->x               = d3d->out_vp.X;
   vp->y               = d3d->out_vp.Y;
   vp->width           = d3d->out_vp.Width;
   vp->height          = d3d->out_vp.Height;

   vp->full_width      = width;
   vp->full_height     = height;
}

#ifdef HAVE_OVERLAY
static void d3d9_free_overlay(d3d9_video_t *d3d, overlay_t *overlay)
{
   if ((LPDIRECT3DTEXTURE9)overlay->tex)
      IDirect3DTexture9_Release((LPDIRECT3DTEXTURE9)overlay->tex);
   d3d9_vertex_buffer_free(overlay->vert_buf, NULL);
}

static void d3d9_hlsl_free_overlays(d3d9_video_t *d3d)
{
   unsigned i;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d9_free_overlay(d3d, &d3d->overlays[i]);
   free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
}

static void d3d9_overlay_tex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   d3d->overlays[index].tex_coords[0] = x;
   d3d->overlays[index].tex_coords[1] = y;
   d3d->overlays[index].tex_coords[2] = w;
   d3d->overlays[index].tex_coords[3] = h;
}

static void d3d9_overlay_vertex_geom(
      void *data,
      unsigned index,
      float x, float y,
      float w, float h)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   y                                   = 1.0f - y;
   h                                   = -h;
   d3d->overlays[index].vert_coords[0] = x;
   d3d->overlays[index].vert_coords[1] = y;
   d3d->overlays[index].vert_coords[2] = w;
   d3d->overlays[index].vert_coords[3] = h;
}

static bool d3d9_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, y;
   d3d9_video_t *d3d                  = (d3d9_video_t*)data;
   const struct texture_image *images = (const struct texture_image*)
      image_data;

   if (!d3d)
      return false;

   d3d9_hlsl_free_overlays(d3d);
   d3d->overlays      = (overlay_t*)calloc(num_images, sizeof(*d3d->overlays));
   d3d->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      D3DLOCKED_RECT d3dlr;
      unsigned width     = images[i].width;
      unsigned height    = images[i].height;
      overlay_t *overlay = (overlay_t*)&d3d->overlays[i];

      overlay->tex       = d3d9_texture_new(d3d->dev,
                  width, height, 1,
                  0,
                  D3D9_ARGB8888_FORMAT,
                  D3DPOOL_MANAGED, 0, 0, 0,
                  NULL, NULL, false);

      if (!overlay->tex)
      {
         RARCH_ERR("[D3D9] Failed to create overlay texture.\n");
         return false;
      }

      IDirect3DTexture9_LockRect((LPDIRECT3DTEXTURE9)overlay->tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
      {
         uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
         const uint32_t *src = images[i].pixels;
         unsigned      pitch = d3dlr.Pitch >> 2;
         for (y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
      }
      IDirect3DTexture9_UnlockRect((LPDIRECT3DTEXTURE9)overlay->tex, 0);

      overlay->tex_w         = width;
      overlay->tex_h         = height;

      /* Default. Stretch to whole screen. */
      d3d9_overlay_tex_geom(d3d, i, 0, 0, 1, 1);
      d3d9_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d9_overlay_enable(void *data, bool state)
{
   unsigned i;
   d3d9_video_t            *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays_enabled = state;

#ifndef XBOX
   win32_show_cursor(d3d, state);
#endif
}

static void d3d9_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d9_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (d3d)
      d3d->overlays[index].alpha_mod = mod;
}

static const video_overlay_interface_t d3d9_overlay_interface = {
   d3d9_overlay_enable,
   d3d9_overlay_load,
   d3d9_overlay_tex_geom,
   d3d9_overlay_vertex_geom,
   d3d9_overlay_full_screen,
   d3d9_overlay_set_alpha,
};

static void d3d9_hlsl_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   *iface = &d3d9_overlay_interface;
}
#endif

#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
static void d3d9_overlay_render(d3d9_video_t *d3d,
      unsigned width,
      unsigned height,
      overlay_t *overlay, bool force_linear)
{
   D3DTEXTUREFILTERTYPE filter_type;
   LPDIRECT3DVERTEXDECLARATION9 vertex_decl;
   LPDIRECT3DDEVICE9 dev;
   struct video_viewport vp;
   void *verts;
   unsigned i;
   Vertex vert[4];
   D3DVERTEXELEMENT9 vElems[4] = {
      {0, offsetof(Vertex, x),  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_POSITION, 0},
      {0, offsetof(Vertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_TEXCOORD, 0},
      {0, offsetof(Vertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_COLOR, 0},
      D3DDECL_END()
   };

   if (!overlay || !overlay->tex)
      return;

   dev                 = d3d->dev;

   if (!overlay->vert_buf)
   {
      overlay->vert_buf = NULL;
      if (!SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
                  dev, sizeof(vert), D3DUSAGE_WRITEONLY,
#ifdef _XBOX
                  0,
#else
                  D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1,
#endif
                  D3DPOOL_MANAGED,
                  (LPDIRECT3DVERTEXBUFFER9*)&overlay->vert_buf, NULL)))
         overlay->vert_buf = NULL;

     if (!overlay->vert_buf)
        return;
   }

   for (i = 0; i < 4; i++)
   {
      vert[i].z       = 0.5f;
      vert[i].color   = (((uint32_t)(overlay->alpha_mod * 0xFF)) << 24) | 0xFFFFFF;
   }

   d3d9_hlsl_viewport_info(d3d, &vp);

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

   IDirect3DVertexBuffer9_Lock((LPDIRECT3DVERTEXBUFFER9)overlay->vert_buf, 0, 0, &verts, 0);
   memcpy(verts, vert, sizeof(vert));
   IDirect3DVertexBuffer9_Unlock((LPDIRECT3DVERTEXBUFFER9)overlay->vert_buf);

   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, true);

   /* set vertex declaration for overlay. */
   IDirect3DDevice9_CreateVertexDeclaration(dev,
         (const D3DVERTEXELEMENT9*)&vElems, (IDirect3DVertexDeclaration9**)&vertex_decl);
   IDirect3DDevice9_SetVertexDeclaration(dev, vertex_decl);
   IDirect3DVertexDeclaration9_Release(vertex_decl);

   IDirect3DDevice9_SetStreamSource(dev, 0,
         (LPDIRECT3DVERTEXBUFFER9)overlay->vert_buf, 0, sizeof(*vert));

   if (overlay->fullscreen)
   {
      D3DVIEWPORT9 vp_full;

      vp_full.X      = 0;
      vp_full.Y      = 0;
      vp_full.Width  = width;
      vp_full.Height = height;
      vp_full.MinZ   = 0.0f;
      vp_full.MaxZ   = 1.0f;
      IDirect3DDevice9_SetViewport(dev, (D3DVIEWPORT9*)&vp_full);
   }

   filter_type = D3DTEXF_LINEAR;

   if (!force_linear)
   {
      settings_t *settings    = config_get_ptr();
      bool menu_linear_filter = settings->bools.menu_linear_filter;
      if (!menu_linear_filter)
         filter_type       = D3DTEXF_POINT;
   }

   /* Render overlay. */
   IDirect3DDevice9_SetTexture(dev, 0,(IDirect3DBaseTexture9*)overlay->tex);
   IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_ADDRESSU,
         D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_ADDRESSV,
         D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MINFILTER, filter_type);
   IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MAGFILTER, filter_type);
   IDirect3DDevice9_BeginScene(dev);
   IDirect3DDevice9_DrawPrimitive(dev, D3DPT_TRIANGLESTRIP, 0, 2);
   IDirect3DDevice9_EndScene(dev);

   /* Restore previous state. */
   IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, false);
   IDirect3DDevice9_SetViewport(dev, (D3DVIEWPORT9*)&d3d->out_vp);
}

static void d3d9_hlsl_free_overlay(d3d9_video_t *d3d, overlay_t *overlay)
{
   if ((LPDIRECT3DTEXTURE9)overlay->tex)
      IDirect3DTexture9_Release((LPDIRECT3DTEXTURE9)overlay->tex);
   d3d9_vertex_buffer_free(overlay->vert_buf, NULL);
}
#endif

static void d3d9_hlsl_free(void *data)
{
   d3d9_video_t   *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_OVERLAY
   d3d9_hlsl_free_overlays(d3d);
   if (d3d->overlays)
      free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
#endif

   d3d9_hlsl_free_overlay(d3d, d3d->menu);
   if (d3d->menu)
      free(d3d->menu);
   d3d->menu          = NULL;

   d3d9_hlsl_deinitialize(d3d);

   if (d3d9_hlsl_white_texture)
   {
      IDirect3DTexture9_Release(d3d9_hlsl_white_texture);
      d3d9_hlsl_white_texture = NULL;
   }

   if (d3d->shader_path && *d3d->shader_path)
      free(d3d->shader_path);

   IDirect3DDevice9_Release(d3d->dev);
   IDirect3D9_Release(g_pD3D9);
   d3d->shader_path = NULL;
   d3d->dev         = NULL;
   g_pD3D9          = NULL;

   d3d9_deinitialize_symbols();

#ifndef _XBOX
   win32_monitor_from_window();
   win32_destroy_window();
#endif
   free(d3d);
}

static bool d3d9_hlsl_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count, unsigned pitch,
      const char *msg, video_frame_info_t *video_info)
{
   D3DVIEWPORT9 screen_vp;
   unsigned i                          = 0;
   d3d9_video_t *d3d                   = (d3d9_video_t*)data;
   unsigned width                      = video_info->width;
   unsigned height                     = video_info->height;
   bool statistics_show                = video_info->statistics_show;
   unsigned black_frame_insertion      = video_info->black_frame_insertion;
   struct font_params *osd_params      = (struct font_params*)
      &video_info->osd_stat_params;
   const char *stat_text               = video_info->stat_text;
#ifdef HAVE_MENU
   bool menu_is_alive                  = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#else
   bool menu_is_alive                  = false;
#endif
   bool overlay_behind_menu            = video_info->overlay_behind_menu;
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                 = video_info->widgets_active;
#endif

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

      if (!d3d9_hlsl_restore(d3d))
      {
         RARCH_ERR("[D3D9 HLSL] Failed to restore.\n");
         return false;
      }
   }

   if (d3d->should_resize)
   {
      hlsl_renderchain_t *_chain = (hlsl_renderchain_t*)d3d->renderchain_data;
      d3d9_renderchain_t *chain  = (d3d9_renderchain_t*)&_chain->chain;

      d3d9_hlsl_set_viewport(d3d, width, height, false, true);

      if (chain)
         chain->out_vp           = (D3DVIEWPORT9*)&d3d->out_vp;

      d3d9_recompute_pass_sizes(chain->dev, chain, d3d);

      d3d->should_resize         = false;
   }

   /* render_chain() only clears out viewport,
    * clear out everything. */
   screen_vp.X      = 0;
   screen_vp.Y      = 0;
   screen_vp.MinZ   = 0;
   screen_vp.MaxZ   = 1;
   screen_vp.Width  = width;
   screen_vp.Height = height;
   IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
   IDirect3DDevice9_Clear(d3d->dev, 0, 0, D3DCLEAR_TARGET,
         0, 1, 0);

   IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev, 0,
         (const float*)&d3d->mvp, 4);
   hlsl_d3d9_renderchain_render(
         d3d, frame, frame_width, frame_height,
         pitch, d3d->dev_rotation);

   if (black_frame_insertion && !d3d->menu->enabled)
   {
      unsigned n;
      for (n = 0; n < video_info->black_frame_insertion; ++n)
      {
#ifdef _XBOX
        bool ret = true;
        IDirect3DDevice9_Present(d3d->dev, NULL, NULL, NULL, NULL);
#else
        bool ret = (IDirect3DDevice9_Present(d3d->dev,
                 NULL, NULL, NULL, NULL) != D3DERR_DEVICELOST);
#endif
        if (!ret || d3d->needs_restore)
          return true;
        IDirect3DDevice9_Clear(d3d->dev, 0, 0, D3DCLEAR_TARGET,
              0, 1, 0);
      }
   }

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled && overlay_behind_menu)
   {
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev, 0,
            (const float*)&d3d->mvp_transposed, 4);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev, 0,
            (const float*)&d3d->mvp, 4);
      d3d9_overlay_render(d3d, width, height, d3d->menu, false);

      d3d->menu_display.offset = 0;
      IDirect3DDevice9_SetVertexDeclaration(d3d->dev, (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);
      IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
            (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
            0, sizeof(Vertex));
      IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
      IDirect3DDevice9_BeginScene(d3d->dev);
      menu_driver_frame(menu_is_alive, video_info);
      IDirect3DDevice9_EndScene(d3d->dev);
   }
   else if (statistics_show)
   {
      if (osd_params)
      {
         IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
         IDirect3DDevice9_BeginScene(d3d->dev);
         font_driver_render_msg(d3d, stat_text,
               (const struct font_params*)osd_params, NULL);
         IDirect3DDevice9_EndScene(d3d->dev);
      }
   }
#endif

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled && !overlay_behind_menu)
   {
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev, 0,
            (const float*)&d3d->mvp_transposed, 4);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
   {
      hlsl_renderchain_t *_chain = (hlsl_renderchain_t*)d3d->renderchain_data;
      RECT scissor_rect;

      d3d->menu_display.offset = 0;
      IDirect3DDevice9_SetVertexDeclaration(d3d->dev,
            (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);
      IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
            (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
            0, sizeof(Vertex));
      IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);

      /* Reset scissor rect to full screen */
      scissor_rect.left   = 0;
      scissor_rect.top    = 0;
      scissor_rect.right  = width;
      scissor_rect.bottom = height;
      IDirect3DDevice9_SetScissorRect(d3d->dev, &scissor_rect);

      IDirect3DDevice9_SetRenderState(d3d->dev,
            D3DRS_ALPHABLENDENABLE, true);
      IDirect3DDevice9_SetRenderState(d3d->dev,
            D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
      IDirect3DDevice9_SetRenderState(d3d->dev,
            D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
      IDirect3DDevice9_SetRenderState(d3d->dev,
            D3DRS_ZENABLE, false);
      IDirect3DDevice9_SetRenderState(d3d->dev,
            D3DRS_CULLMODE, D3DCULL_NONE);
      if (_chain)
      {
         IDirect3DDevice9_SetVertexShader(d3d->dev, (LPDIRECT3DVERTEXSHADER9)(&_chain->stock_shader)->vprg);
         IDirect3DDevice9_SetPixelShader(d3d->dev, (LPDIRECT3DPIXELSHADER9)(&_chain->stock_shader)->fprg);
      }
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev, 0,
            (const float*)&d3d->mvp, 4);
      IDirect3DDevice9_BeginScene(d3d->dev);
      gfx_widgets_frame(video_info);
      IDirect3DDevice9_EndScene(d3d->dev);
   }
#endif

   if (msg && *msg)
   {
      IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
      IDirect3DDevice9_BeginScene(d3d->dev);
      font_driver_render_msg(d3d, msg, NULL, NULL);
      IDirect3DDevice9_EndScene(d3d->dev);
   }

   video_driver_update_title(NULL);
   IDirect3DDevice9_Present(d3d->dev, NULL, NULL, NULL, NULL);

   return true;
}

static void d3d9_hlsl_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   d3d->keep_aspect   = true;
   d3d->should_resize = true;
}

static void d3d9_hlsl_apply_state_changes(void *data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (d3d)
      d3d->should_resize = true;
}

static void d3d9_hlsl_set_menu_texture_enable(void *data,
      bool state, bool full_screen)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   d3d->menu->enabled            = state;
   d3d->menu->fullscreen         = full_screen;
}

static void d3d9_hlsl_set_menu_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   D3DLOCKED_RECT d3dlr;
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   if (       (!d3d->menu->tex)
            || (d3d->menu->tex_w != width)
            || (d3d->menu->tex_h != height))
   {
      if (d3d->menu->tex)
         IDirect3DTexture9_Release((LPDIRECT3DTEXTURE9)d3d->menu->tex);

      d3d->menu->tex = d3d9_texture_new(d3d->dev,
            width, height, 1,
            0, D3D9_ARGB8888_FORMAT,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!d3d->menu->tex)
      {
         RARCH_ERR("[D3D9] Failed to create menu texture.\n");
         return;
      }

      d3d->menu->tex_w          = width;
      d3d->menu->tex_h          = height;
   }

   d3d->menu->alpha_mod = alpha;

   IDirect3DTexture9_LockRect((LPDIRECT3DTEXTURE9)d3d->menu->tex,
         0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
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

         for (h = 0; h < height; h++,
               dst += d3dlr.Pitch >> 2,
               src += width)
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
   }

   IDirect3DTexture9_UnlockRect((LPDIRECT3DTEXTURE9)d3d->menu->tex, 0);
}

static void d3d9_hlsl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifndef _XBOX
   /* TODO/FIXME - why is this called here? */
   win32_show_cursor(data, !fullscreen);
#endif
}

struct d3d9_texture_info
{
   void *userdata;
   void *data;
   enum texture_filter_type type;
};

static void d3d9_hlsl_video_texture_load_d3d(
      struct d3d9_texture_info *info,
      uintptr_t *id)
{
   D3DLOCKED_RECT d3dlr;
   LPDIRECT3DTEXTURE9 tex   = NULL;
   unsigned usage           = 0;
   bool want_mipmap         = false;
   d3d9_video_t *d3d        = (d3d9_video_t*)info->userdata;
   struct texture_image *ti = (struct texture_image*)info->data;

   if (!ti)
      return;

   if (  (info->type == TEXTURE_FILTER_MIPMAP_LINEAR)
      || (info->type == TEXTURE_FILTER_MIPMAP_NEAREST))
      want_mipmap        = true;

   if (!(tex = (LPDIRECT3DTEXTURE9)d3d9_texture_new(d3d->dev,
               ti->width, ti->height, 1,
               usage, D3D9_ARGB8888_FORMAT,
               D3DPOOL_MANAGED, 0, 0, 0,
               NULL, NULL, want_mipmap)))
      return;

   IDirect3DTexture9_LockRect(tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK);
   {
      unsigned i;
      uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
      const uint32_t *src = ti->pixels;
      unsigned      pitch = d3dlr.Pitch >> 2;
      for (i = 0; i < ti->height; i++, dst += pitch, src += ti->width)
         memcpy(dst, src, ti->width << 2);
   }
   IDirect3DTexture9_UnlockRect(tex, 0);

   *id = (uintptr_t)tex;
}

#ifdef HAVE_THREADS
static int d3d9_hlsl_video_texture_load_wrap_d3d(void *data)
{
   uintptr_t id = 0;
   struct d3d9_texture_info *info = (struct d3d9_texture_info*)data;
   if (!info)
      return 0;
   d3d9_hlsl_video_texture_load_d3d(info, &id);
   return id;
}
#endif

uintptr_t d3d9_hlsl_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;
   struct d3d9_texture_info info;

   info.userdata = video_data;
   info.data     = data;
   info.type     = filter_type;

#ifdef HAVE_THREADS
   if (threaded)
      return video_thread_texture_handle(&info,
            d3d9_hlsl_video_texture_load_wrap_d3d);
#endif

   d3d9_hlsl_video_texture_load_d3d(&info, &id);
   return id;
}

static void d3d9_hlsl_unload_texture(void *data,
      bool threaded, uintptr_t id)
{
   LPDIRECT3DTEXTURE9 texid;
   if (!id)
      return;

   texid = (LPDIRECT3DTEXTURE9)id;
   IDirect3DTexture9_Release(texid);
}

static const video_poke_interface_t d3d9_hlsl_poke_interface = {
   d3d9_hlsl_get_flags,
   d3d9_hlsl_load_texture,
   d3d9_hlsl_unload_texture,
   d3d9_hlsl_set_video_mode,
#if defined(_XBOX) || defined(__WINRT__)
   NULL, /* get_refresh_rate */
#else
   /* UWP does not expose this information easily */
   win32_get_refresh_rate,
#endif
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d9_hlsl_set_aspect_ratio,
   d3d9_hlsl_apply_state_changes,
   d3d9_hlsl_set_menu_texture_frame,
   d3d9_hlsl_set_menu_texture_enable,
   d3d9_hlsl_set_osd_msg,
   win32_show_cursor,
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void d3d9_hlsl_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &d3d9_hlsl_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool d3d9_hlsl_gfx_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

static void d3d9_hlsl_set_resize(d3d9_video_t *d3d,
      unsigned new_width, unsigned new_height)
{
   /* No changes? */
   if (     (new_width  == d3d->video_info.width)
         && (new_height == d3d->video_info.height))
      return;

   d3d->video_info.width  = new_width;
   d3d->video_info.height = new_height;
   video_driver_set_size(new_width, new_height);
}

static bool d3d9_hlsl_alive(void *data)
{
   unsigned temp_width   = 0;
   unsigned temp_height  = 0;
   bool ret              = false;
   bool        quit      = false;
   bool        resize    = false;
   d3d9_video_t *d3d     = (d3d9_video_t*)data;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   win32_check_window(NULL, &quit, &resize, &temp_width, &temp_height);

   if (quit)
      d3d->quitting      = quit;

   if (resize)
   {
      d3d->should_resize = true;
      d3d9_hlsl_set_resize(d3d, temp_width, temp_height);
      d3d9_hlsl_restore(d3d);
   }

   ret = !quit;

   if (  temp_width  != 0 &&
         temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   return ret;
}

static void d3d9_hlsl_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
#ifdef _XBOX
   int interval          = 0;
#endif
   d3d9_video_t     *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

#ifdef _XBOX
   if (!state)
      interval           = 1;
#endif

   d3d->video_info.vsync = !state;

#ifdef _XBOX
   IDirect3DDevice9_SetRenderState(d3d->dev,
         D3DRS_PRESENTINTERVAL,
         interval ?
         D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE);
#else
   d3d->needs_restore    = true;
   d3d9_hlsl_restore(d3d);
#endif
}

#ifdef _XBOX
static bool d3d9_hlsl_suspend_screensaver(void *data, bool enable) { return true; }
#endif

static void d3d9_hlsl_set_rotation(void *data, unsigned rot)
{
   d3d9_video_t        *d3d = (d3d9_video_t*)data;
   if (d3d)
      d3d->dev_rotation = rot;
}

static INLINE bool d3d9_hlsl_device_get_render_target_data(
      LPDIRECT3DDEVICE9 dev,
      LPDIRECT3DSURFACE9 src, LPDIRECT3DSURFACE9 dst)
{
#ifndef _XBOX
   return (   dev
           && SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(
               dev, src, dst)));
#else
   return false;
#endif
}

static bool d3d9_hlsl_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   unsigned width, height;
   D3DLOCKED_RECT rect;
   LPDIRECT3DSURFACE9 target = NULL;
   LPDIRECT3DSURFACE9 dest   = NULL;
   bool ret                  = true;
   d3d9_video_t *d3d         = (d3d9_video_t*)data;
   LPDIRECT3DDEVICE9 d3dr    = d3d->dev;

   video_driver_get_size(&width, &height);

   if (
            !d3d9_device_get_render_target(d3dr, 0, (void**)&target)
         || !d3d9_device_create_offscreen_plain_surface(d3dr, width, height,
            D3D9_XRGB8888_FORMAT,
            D3DPOOL_SYSTEMMEM, (void**)&dest, NULL)
         || !d3d9_hlsl_device_get_render_target_data(d3dr, target, dest)
         )
   {
      ret = false;
      goto end;
   }

   IDirect3DSurface9_LockRect(dest, &rect, NULL, D3DLOCK_READONLY);

   {
      unsigned x, y;
      unsigned vp_width       = (d3d->out_vp.Width  > width)  ? width  : d3d->out_vp.Width;
      unsigned vp_height      = (d3d->out_vp.Height > height) ? height : d3d->out_vp.Height;
      unsigned pitchpix       = rect.Pitch / 4;
      const uint32_t *pixels  = (const uint32_t*)rect.pBits;

      pixels                 += d3d->out_vp.X;
      pixels                 += (vp_height - 1) * pitchpix;
      pixels                 -= d3d->out_vp.Y * pitchpix;

      for (y = 0; y < vp_height; y++, pixels -= pitchpix)
      {
         for (x = 0; x < vp_width; x++)
         {
            *buffer++ = (pixels[x] >>  0) & 0xff;
            *buffer++ = (pixels[x] >>  8) & 0xff;
            *buffer++ = (pixels[x] >> 16) & 0xff;
         }
      }

      IDirect3DSurface9_UnlockRect(dest);
   }

end:
   if (target)
      IDirect3DSurface9_Release(target);
   if (dest)
      IDirect3DSurface9_Release(dest);
   return ret;
}

static bool d3d9_hlsl_has_windowed(void *data)
{
#ifdef _XBOX
   return false;
#else
   return true;
#endif
}

video_driver_t video_d3d9_hlsl = {
   d3d9_hlsl_init,
   d3d9_hlsl_frame,
   d3d9_hlsl_set_nonblock_state,
   d3d9_hlsl_alive,
   NULL, /* focus */
#ifdef _XBOX
   d3d9_hlsl_suspend_screensaver,
#else
   win32_suspend_screensaver,
#endif
   d3d9_hlsl_has_windowed,
   d3d9_hlsl_set_shader,
   d3d9_hlsl_free,
   "d3d9_hlsl",
   d3d9_hlsl_set_viewport,
   d3d9_hlsl_set_rotation,
   d3d9_hlsl_viewport_info,
   d3d9_hlsl_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d9_hlsl_get_overlay_interface,
#endif
   d3d9_hlsl_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   d3d9_hlsl_gfx_widgets_enabled
#endif
};
