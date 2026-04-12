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

/* Direct3D 9 driver with Cg runtime backend.
 *
 * Minimum version : Direct3D 9.0 (2002)
 * Minimum OS      : Windows 98, Windows 2000, Windows ME
 * Recommended OS  : Windows XP
 * Requirements    : Cg runtime
 */

#define CINTERFACE

#include <formats/image.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_math.h>
#include <encodings/utf.h>

#define WIN32_LEAN_AND_MEAN
#include <d3d9.h>

#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include <retro_inline.h>
#include <retro_common_api.h>
#include <boolean.h>

#include <defines/d3d_defines.h>
#include <gfx/math/matrix_4x4.h>
#include "../common/d3d_common.h"

#include "../common/d3d9_common.h"
#include "../include/Cg/cg.h"
#include "../include/Cg/cgD3D9.h"
#include "../video_driver.h"
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

#include "d3d_shaders/shaders_common.h"

static const char *stock_cg_d3d9_program = CG(
    void main_vertex
    (
    	float4 position : POSITION,
    	float2 texCoord : TEXCOORD0,
      float4 color : COLOR,

      uniform float4x4 modelViewProj,

    	out float4 oPosition : POSITION,
    	out float2 otexCoord : TEXCOORD0,
      out float4 oColor : COLOR
    )
    {
    	oPosition = mul(modelViewProj, position);
    	otexCoord = texCoord;
      oColor = color;
    }

    float4 main_fragment(in float4 color : COLOR, float2 tex : TEXCOORD0, uniform sampler2D s0 : TEXUNIT0) : COLOR
    {
       return color * tex2D(s0, tex);
    }
);

#define D3D_FILTER_LINEAR           (3 << 0)
#define D3D_FILTER_POINT            (2 << 0)

/* Cg debug helper: checks and logs Cg runtime errors.
 * Only active in debug builds to avoid overhead in release. */
#ifdef DEBUG
#define CG_DEBUG_CHECK(ctx, msg) \
   do { \
      CGerror _cg_err = cgGetError(); \
      if (_cg_err != CG_NO_ERROR) \
         RARCH_WARN("[D3D9 Cg] %s: %s\n", msg, cgGetErrorString(_cg_err)); \
   } while(0)
#else
#define CG_DEBUG_CHECK(ctx, msg) ((void)0)
#endif

#define D3D9_DECL_FVF_TEXCOORD(stream, offset, index) \
   { (WORD)(stream), (WORD)(offset * sizeof(float)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, \
      D3DDECLUSAGE_TEXCOORD, (BYTE)(index) }

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

typedef struct d3d9_cg_renderchain
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
} d3d9_cg_renderchain_t;

static INLINE bool d3d9_cg_renderchain_add_lut(d3d9_cg_renderchain_t *chain,
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

   {
      void *_tbuf = NULL;
      if (SUCCEEDED(IDirect3DDevice9_CreateTexture(chain->dev,
                  image.width, image.height, 1, 0,
                  D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                  (struct IDirect3DTexture9**)&_tbuf, NULL)))
         lut = (LPDIRECT3DTEXTURE9)_tbuf;
   }

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

static INLINE void d3d9_cg_renderchain_add_lut_internal(
      d3d9_cg_renderchain_t *chain,
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

static INLINE bool d3d9_cg_renderchain_set_pass_size(
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
      pass->tex        = NULL;
      {
         void *_tbuf = NULL;
         if (SUCCEEDED(IDirect3DDevice9_CreateTexture(dev,
                     width, height, 1,
                     D3DUSAGE_RENDERTARGET,
                     (pass2->info.pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
                     ? D3DFMT_A32B32G32R32F
                     : D3DFMT_A8R8G8B8,
                     D3DPOOL_DEFAULT,
                     (struct IDirect3DTexture9**)&_tbuf, NULL)))
            pass->tex = (LPDIRECT3DTEXTURE9)_tbuf;
      }

      if (!pass->tex)
         return false;

      IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9*)pass->tex);
      IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9*)NULL);
   }

   return true;
}

static INLINE void d3d9_recompute_pass_sizes(
      LPDIRECT3DDEVICE9 dev,
      d3d9_cg_renderchain_t *chain,
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

   if (!d3d9_cg_renderchain_set_pass_size(dev,
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
      switch (link_info.pass->fbo.type_x)
      {
         case RARCH_SCALE_VIEWPORT:
            out_width = link_info.pass->fbo.scale_x * d3d->out_vp.Width;
            break;
         case RARCH_SCALE_ABSOLUTE:
            out_width = link_info.pass->fbo.abs_x;
            break;
         case RARCH_SCALE_INPUT:
            out_width = link_info.pass->fbo.scale_x * current_width;
            break;
      }
      switch (link_info.pass->fbo.type_y)
      {
         case RARCH_SCALE_VIEWPORT:
            out_height = link_info.pass->fbo.scale_y * d3d->out_vp.Height;
            break;
         case RARCH_SCALE_ABSOLUTE:
            out_height = link_info.pass->fbo.abs_y;
            break;
         case RARCH_SCALE_INPUT:
            out_height = link_info.pass->fbo.scale_y * current_height;
            break;
      }

      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      if (!d3d9_cg_renderchain_set_pass_size(dev,
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

#define DECL_FVF_COLOR(stream, offset, index) \
   { (WORD)(stream), (WORD)(offset * sizeof(float)), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, \
      D3DDECLUSAGE_COLOR, (BYTE)(index) } \

/* TODO/FIXME - Temporary workaround for D3D9 not being able to poll flags during init */
static gfx_ctx_driver_t d3d9_cg_fake_context;

struct D3D9CGVertex
{
   float x, y, z;
   float u, v;
   float lut_u, lut_v;
   float r, g, b, a;
};

#ifdef _MSC_VER
#pragma comment(lib, "cgd3d9")
#endif

typedef struct cg_renderchain
{
   struct d3d9_cg_renderchain chain;
   struct shader_pass stock_shader;
   CGcontext cgCtx;
} cg_renderchain_t;

/* Pipeline vertex buffer for menu shader effects (VIDEO_SHADER_MENU, etc.).
 * Stored as a file-static since the d3d9 menu_display struct
 * does not have a pipeline_vbo member like d3d10_video_t does. */
static LPDIRECT3DVERTEXBUFFER9 d3d9_cg_menu_pipeline_vbo = NULL;

static LPDIRECT3DTEXTURE9 d3d9_cg_white_texture = NULL;


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



/* Set the modelViewProj matrix on the stock Cg vertex shader
 * via its named parameter. The Cg runtime assigns hardware
 * registers at compile time, so SetVertexShaderConstantF(0,...)
 * won't necessarily hit the right register — we must go through
 * cgD3D9SetUniformMatrix instead.
 *
 * NOTE: the matrix must already be in the column-major layout
 * expected by the Cg shader's mul(matrix, vector).  Callers
 * coming from D3D row-major matrices should use the _transposed
 * variant below instead. */
/*
 * DISPLAY DRIVER
 */

static const float d3d9_cg_vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float d3d9_cg_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static INT32 gfx_display_prim_to_d3d9_cg_enum(
      enum gfx_display_prim_type prim_type)
{
   switch (prim_type)
   {
      case GFX_DISPLAY_PRIM_TRIANGLES:
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         return D3DPT_TRIANGLESTRIP;
      case GFX_DISPLAY_PRIM_NONE:
      default:
         break;
   }

   /* TODO/FIXME - hack */
   return 0;
}

static void gfx_display_d3d9_cg_blend_begin(void *data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, TRUE);
}

static void gfx_display_d3d9_cg_blend_end(void *data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_ALPHABLENDENABLE, FALSE);
}

struct d3d9_texture_info
{
   void *userdata;
   void *data;
   enum texture_filter_type type;
};

static void d3d9_video_texture_load_d3d(
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

   {
      void *_tbuf = NULL;
      if (want_mipmap)
         usage |= D3DUSAGE_AUTOGENMIPMAP;
      if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d3d->dev,
                  ti->width, ti->height, 1,
                  usage, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                  (struct IDirect3DTexture9**)&_tbuf, NULL)))
         tex = (LPDIRECT3DTEXTURE9)_tbuf;
   }

   if (!tex)
      return;

   if (SUCCEEDED(IDirect3DTexture9_LockRect(tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
   {
      unsigned i;
      uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
      const uint32_t *src = ti->pixels;
      unsigned      pitch = d3dlr.Pitch >> 2;
      for (i = 0; i < ti->height; i++, dst += pitch, src += ti->width)
         memcpy(dst, src, ti->width << 2);
      IDirect3DTexture9_UnlockRect(tex, 0);
   }

   *id = (uintptr_t)tex;
}

#ifdef HAVE_THREADS
static int d3d9_cg_video_texture_load_wrap_d3d(void *data)
{
   uintptr_t id = 0;
   struct d3d9_texture_info *info = (struct d3d9_texture_info*)data;
   if (!info)
      return 0;
   d3d9_video_texture_load_d3d(info, &id);
   return id;
}
#endif

static uintptr_t d3d9_cg_load_texture(void *video_data, void *data,
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
            d3d9_cg_video_texture_load_wrap_d3d);
#endif

   d3d9_video_texture_load_d3d(&info, &id);
   return id;
}

static void d3d9_cg_unload_texture(void *data,
      bool threaded, uintptr_t id)
{
   LPDIRECT3DTEXTURE9 texid;
   if (!id)
      return;

   texid = (LPDIRECT3DTEXTURE9)id;
   IDirect3DTexture9_Release(texid);
}

static void gfx_display_d3d9_cg_bind_texture(gfx_display_ctx_draw_t *draw,
      d3d9_video_t *d3d)
{
   LPDIRECT3DDEVICE9 dev = d3d->dev;

   if (draw->texture)
      IDirect3DDevice9_SetTexture(dev, 0,
            (IDirect3DBaseTexture9*)draw->texture);
   else if (d3d9_cg_white_texture)
      IDirect3DDevice9_SetTexture(dev, 0,
            (IDirect3DBaseTexture9*)d3d9_cg_white_texture);
   else
      IDirect3DDevice9_SetTexture(dev, 0, NULL);

   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
   IDirect3DDevice9_SetSamplerState(dev, 0,
         D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
}

static void gfx_display_d3d9_cg_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   unsigned i;
   LPDIRECT3DDEVICE9 dev;
   D3DPRIMITIVETYPE type;
   bool has_vertex_data;
   unsigned start                = 0;
   unsigned count                = 0;
   unsigned vertex_count         = 4;
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
         cg_renderchain_t *_chain = (cg_renderchain_t*)d3d->renderchain_data;

         if (_chain)
         {
            cgD3D9BindProgram((CGprogram)_chain->stock_shader.fprg);
            cgD3D9BindProgram((CGprogram)_chain->stock_shader.vprg);
            CG_DEBUG_CHECK(_chain->cgCtx, "pipeline draw BindProgram");

            IDirect3DDevice9_DrawPrimitive(dev, D3DPT_TRIANGLESTRIP,
                  0, draw->coords->vertices - 2);
         }

         /* Re-enable alpha blending after pipeline draw */
         IDirect3DDevice9_SetRenderState(dev,
               D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
         IDirect3DDevice9_SetRenderState(dev,
               D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
         IDirect3DDevice9_SetRenderState(dev,
               D3DRS_ALPHABLENDENABLE, TRUE);

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

   has_vertex_data = draw->coords->vertex
      && draw->coords->tex_coord && draw->coords->color;

   if (has_vertex_data)
      vertex_count = draw->coords->vertices;

   if (!has_vertex_data)
   {
      /* Single-sprite path: build a quad from draw->x/y/width/height
       * in normalized [0,1] space, using DrawPrimitiveUP.
       *
       * Callers like ozone_draw_icon pre-flip Y (draw.y = height - y - h).
       * We undo that flip here so that the top-down ortho matrix produces
       * the correct screen position with correct texture orientation
       * (v=0 at screen top, v=1 at screen bottom). */
      D3DCOLOR col[4];
      Vertex quad[4];
      float x1, y1, x2, y2;
      const float *c = draw->coords->color;

      if (c)
      {
         /* The color array is laid out for bottom-up coordinates
          * (vertex 0,1 = bottom, vertex 2,3 = top) as used by d3d10/d3d11.
          * Since this path un-flips Y (top-down ortho), swap the top and
          * bottom vertex color pairs so gradients render correctly. */
         col[0] = D3DCOLOR_ARGB((int)(c[11]*0xFF), (int)(c[ 8]*0xFF), (int)(c[ 9]*0xFF), (int)(c[10]*0xFF));
         col[1] = D3DCOLOR_ARGB((int)(c[15]*0xFF), (int)(c[12]*0xFF), (int)(c[13]*0xFF), (int)(c[14]*0xFF));
         col[2] = D3DCOLOR_ARGB((int)(c[ 3]*0xFF), (int)(c[ 0]*0xFF), (int)(c[ 1]*0xFF), (int)(c[ 2]*0xFF));
         col[3] = D3DCOLOR_ARGB((int)(c[ 7]*0xFF), (int)(c[ 4]*0xFF), (int)(c[ 5]*0xFF), (int)(c[ 6]*0xFF));
      }
      else
      {
         col[0] = col[1] = col[2] = col[3] = D3DCOLOR_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
      }

      /* Undo the Y pre-flip, then let topdown_ortho handle the mapping.
       * This matches the HLSL driver's single-sprite coordinate path. */
      x1 = draw->x / (float)video_width;
      y1 = ((float)video_height - draw->y - draw->height) / (float)video_height;
      x2 = (draw->x + draw->width)  / (float)video_width;
      y2 = ((float)video_height - draw->y) / (float)video_height;

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

      /* Top-down ortho: maps X [0,1]->[-1,1], Y [0,1]->[1,-1] (Y=0 at top).
       * Column-major (transposed) layout for the Cg stock shader which
       * uses mul(matrix, vector).  This is the transpose of the HLSL
       * driver's row-major topdown_ortho. */
      {
         static const float topdown_ortho[16] = {
             2.0f,  0.0f, 0.0f, -1.0f,
             0.0f, -2.0f, 0.0f,  1.0f,
             0.0f,  0.0f, 1.0f,  0.0f,
             0.0f,  0.0f, 0.0f,  1.0f
         };
         cg_renderchain_t *_chain = (cg_renderchain_t*)d3d->renderchain_data;
         CGparameter cgp = cgGetNamedParameter(
               (CGprogram)_chain->stock_shader.vprg, "modelViewProj");
         if (cgp)
            cgD3D9SetUniformMatrix(cgp, (const D3DMATRIX*)topdown_ortho);
         /* Re-bind programs to commit the updated MVP to the GPU.
          * cgD3D9BindProgram pushes the parameter values that were
          * set via cgD3D9SetUniformMatrix into the hardware registers;
          * without this, the GPU keeps using whatever matrix was
          * active when the programs were last bound. */
         cgD3D9BindProgram((CGprogram)_chain->stock_shader.fprg);
         cgD3D9BindProgram((CGprogram)_chain->stock_shader.vprg);
         CG_DEBUG_CHECK(_chain->cgCtx, "single-sprite draw BindProgram");
      }

      gfx_display_d3d9_cg_bind_texture(draw, d3d);

      IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP,
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
      vertex    = &d3d9_cg_vertexes[0];
   if (!tex_coord)
      tex_coord = &d3d9_cg_tex_coords[0];

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

   /* Bottom-up ortho: X [0,1]->[-1,1], Y [0,1]->[-1,1].
    * gfx_display provides explicit vertex coordinates in bottom-up
    * normalised space (see gfx_display_draw_texture_slice comment:
    * "vertex coords are specified bottom-up"). */
   {
      static const float bottomup_ortho[16] = {
          2.0f,  0.0f, 0.0f, -1.0f,
          0.0f,  2.0f, 0.0f, -1.0f,
          0.0f,  0.0f, 1.0f,  0.0f,
          0.0f,  0.0f, 0.0f,  1.0f
      };
      cg_renderchain_t *_chain = (cg_renderchain_t*)d3d->renderchain_data;
      {
         CGparameter cgp = cgGetNamedParameter(
               (CGprogram)_chain->stock_shader.vprg, "modelViewProj");
         if (cgp)
            cgD3D9SetUniformMatrix(cgp, (const D3DMATRIX*)bottomup_ortho);
      }
      cgD3D9BindProgram((CGprogram)_chain->stock_shader.fprg);
      cgD3D9BindProgram((CGprogram)_chain->stock_shader.vprg);
      CG_DEBUG_CHECK(_chain->cgCtx, "multi-vertex draw BindProgram");
   }

   gfx_display_d3d9_cg_bind_texture(draw, d3d);

   type  = (D3DPRIMITIVETYPE)gfx_display_prim_to_d3d9_cg_enum(draw->prim_type);
   start = d3d->menu_display.offset;
   count = draw->coords->vertices -
         ((draw->prim_type == GFX_DISPLAY_PRIM_TRIANGLESTRIP)
          ? 2 : 0);

   IDirect3DDevice9_DrawPrimitive(dev, type, start, count);

   d3d->menu_display.offset += draw->coords->vertices;
}

static void gfx_display_d3d9_cg_draw_pipeline(gfx_display_ctx_draw_t *draw,
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
          * array data if it doesn't already exist. */
         if (!d3d9_cg_menu_pipeline_vbo && ca->coords.vertices)
         {
            unsigned i;
            Vertex *verts    = NULL;
            unsigned vcount  = ca->coords.vertices;

            {
               void *_vbuf = NULL;
               if (SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
                           d3d->dev,
                           vcount * sizeof(Vertex),
                           D3DUSAGE_WRITEONLY, 0,
                           D3DPOOL_DEFAULT,
                           (LPDIRECT3DVERTEXBUFFER9*)&_vbuf, NULL)))
                  d3d9_cg_menu_pipeline_vbo = (LPDIRECT3DVERTEXBUFFER9)_vbuf;
            }

            if (d3d9_cg_menu_pipeline_vbo)
            {
               IDirect3DVertexBuffer9_Lock(
                     (LPDIRECT3DVERTEXBUFFER9)d3d9_cg_menu_pipeline_vbo,
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
                        (LPDIRECT3DVERTEXBUFFER9)d3d9_cg_menu_pipeline_vbo);
               }
            }
         }

         if (d3d9_cg_menu_pipeline_vbo)
         {
            IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
                  (LPDIRECT3DVERTEXBUFFER9)d3d9_cg_menu_pipeline_vbo,
                  0, sizeof(Vertex));
         }

         draw->coords->vertices = ca->coords.vertices;

         /* Set pipeline blend state */
         IDirect3DDevice9_SetRenderState(d3d->dev,
               D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
         IDirect3DDevice9_SetRenderState(d3d->dev,
               D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
         IDirect3DDevice9_SetRenderState(d3d->dev,
               D3DRS_ALPHABLENDENABLE, TRUE);
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

   /* Update time uniform via Cg named parameter */
   t += 0.01f;

   {
      cg_renderchain_t *_chain = (cg_renderchain_t*)d3d->renderchain_data;
      if (_chain)
      {
         CGparameter param;
         param = cgGetNamedParameter(
               (CGprogram)_chain->stock_shader.vprg, "IN.frame_count");
         if (param)
            cgD3D9SetUniform(param, &t);
         param = cgGetNamedParameter(
               (CGprogram)_chain->stock_shader.fprg, "IN.frame_count");
         if (param)
            cgD3D9SetUniform(param, &t);
         CG_DEBUG_CHECK(_chain->cgCtx, "draw_pipeline time uniform");
      }
   }
}

static void gfx_display_d3d9_cg_scissor_begin(
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

static void gfx_display_d3d9_cg_scissor_end(void *data,
      unsigned video_width, unsigned video_height)
{
   RECT rect;
   d3d9_video_t            *d3d9 = (d3d9_video_t*)data;

   if (!d3d9)
      return;

   rect.left            = 0;
   rect.top             = 0;
   rect.right           = video_width;
   rect.bottom          = video_height;

   IDirect3DDevice9_SetScissorRect(d3d9->dev, &rect);
}

gfx_display_ctx_driver_t gfx_display_ctx_d3d9_cg = {
   gfx_display_d3d9_cg_draw,
   gfx_display_d3d9_cg_draw_pipeline,
   gfx_display_d3d9_cg_blend_begin,
   gfx_display_d3d9_cg_blend_end,
   NULL,                                     /* get_default_mvp        */
   NULL,                                     /* get_default_vertices   */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_D3D9_CG_API,
   GFX_VIDEO_DRIVER_DIRECT3D9_CG,
   "d3d9_cg",
   true,
   gfx_display_d3d9_cg_scissor_begin,
   gfx_display_d3d9_cg_scissor_end
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
   /* Scratch buffer to avoid per-line malloc/free in font rendering */
   Vertex                       *scratch_verts;
   unsigned                      scratch_capacity; /* in Vertex count */
} d3d9_cg_font_t;

static void *d3d9_cg_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   unsigned i, j;
   d3d9_video_t    *d3d  = (d3d9_video_t*)data;
   d3d9_cg_font_t *font = (d3d9_cg_font_t*)calloc(1, sizeof(*font));

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
   font->texture    = NULL;
   {
      void *_tbuf = NULL;
      if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d3d->dev,
                  font->tex_width, font->tex_height, 1, 0,
                  D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                  (struct IDirect3DTexture9**)&_tbuf, NULL)))
         font->texture = (LPDIRECT3DTEXTURE9)_tbuf;
   }

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

static void d3d9_cg_font_free(void *data, bool is_threaded)
{
   d3d9_cg_font_t *font = (d3d9_cg_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (font->texture)
      IDirect3DTexture9_Release(font->texture);

   free(font->scratch_verts);
   free(font);
}

static int d3d9_cg_font_get_message_width(void *data,
      const char *msg, size_t msg_len, float scale)
{
   size_t i;
   int delta_x = 0;
   const struct font_glyph *glyph_q = NULL;
   d3d9_cg_font_t *font           = (d3d9_cg_font_t*)data;

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
static INLINE unsigned d3d9_cg_font_emit_quad(
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


/* Get or grow the scratch vertex buffer for font rendering.
 * Avoids per-line calloc/free on the hot path. */
static INLINE Vertex *d3d9_cg_font_get_scratch(
      d3d9_cg_font_t *font, unsigned needed)
{
   if (needed > font->scratch_capacity)
   {
      unsigned new_cap = needed > 1536 ? needed : 1536; /* 256 glyphs * 6 verts */
      Vertex *tmp = (Vertex*)realloc(font->scratch_verts, new_cap * sizeof(Vertex));
      if (!tmp)
         return NULL;
      font->scratch_verts    = tmp;
      font->scratch_capacity = new_cap;
   }
   memset(font->scratch_verts, 0, needed * sizeof(Vertex));
   return font->scratch_verts;
}


static void d3d9_cg_font_render_msg(
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
   d3d9_cg_font_t *font  = (d3d9_cg_font_t*)data;
   d3d9_video_t     *d3d   = (d3d9_video_t*)userdata;
   unsigned          width  = 0;
   unsigned          height = 0;

   if (!d3d || !font || !msg || !*msg)
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
         D3DRS_ALPHABLENDENABLE, TRUE);

   /* Use top-down ortho: Y=0->top, Y=1->bottom, matching Ozone coords.
    * Transposed for the Cg stock shader's mul(matrix, vector) order. */
   {
      static const float topdown_ortho[16] = {
          2.0f,  0.0f, 0.0f, -1.0f,
          0.0f, -2.0f, 0.0f,  1.0f,
          0.0f,  0.0f, 1.0f,  0.0f,
          0.0f,  0.0f, 0.0f,  1.0f
      };
      cg_renderchain_t *_chain = (cg_renderchain_t*)d3d->renderchain_data;
      CGparameter cgp = cgGetNamedParameter(
            (CGprogram)_chain->stock_shader.vprg, "modelViewProj");
      if (cgp)
         cgD3D9SetUniformMatrix(cgp, (const D3DMATRIX*)topdown_ortho);
   }

   /* Update atlas texture if dirty */
   if (font->atlas->dirty)
   {
      if (   font->atlas->width  != font->tex_width
          || font->atlas->height != font->tex_height)
      {
         if (font->texture)
            IDirect3DTexture9_Release(font->texture);

         font->tex_width  = font->atlas->width;
         font->tex_height = font->atlas->height;
         font->texture    = NULL;
         {
            void *_tbuf = NULL;
            if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d3d->dev,
                        font->tex_width, font->tex_height, 1, 0,
                        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                        (struct IDirect3DTexture9**)&_tbuf, NULL)))
               font->texture = (LPDIRECT3DTEXTURE9)_tbuf;
         }
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

   /* Set vertex declaration and stock Cg shader for DrawPrimitiveUP */
   IDirect3DDevice9_SetVertexDeclaration(d3d->dev,
         (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);
   {
      cg_renderchain_t *_chain = (cg_renderchain_t*)d3d->renderchain_data;
      if (_chain)
      {
         cgD3D9BindProgram((CGprogram)_chain->stock_shader.fprg);
         cgD3D9BindProgram((CGprogram)_chain->stock_shader.vprg);
         CG_DEBUG_CHECK(_chain->cgCtx, "font_render BindProgram");
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
            float line_y = y - (float)lines * line_height;

            /* Drop shadow pass */
            if (has_drop)
            {
               float drop_pos_x = x + scale * drop_x / (float)width;
               float drop_pos_y = line_y + scale * drop_y / (float)height;
         {
            unsigned _i;
            float _inv_vp_w, _inv_vp_h;
            float _inv_tex_w, _inv_tex_h;
            const struct font_glyph *_glyph_q = NULL;
            unsigned _rl_width                 = 0;
            unsigned _rl_height                = 0;
            int _rx, _ry;
            unsigned _vert_count               = 0;
            Vertex *_verts                     = NULL;
            const char *_rl_msg                = m;
            size_t _rl_msg_len                 = msg_len;
            float _rl_pos_x                    = drop_pos_x;
            float _rl_pos_y                    = drop_pos_y;
            D3DCOLOR _rl_color                 = color_dark;

            video_driver_get_size(&_rl_width, &_rl_height);
            if (_rl_width && _rl_height)
            {
               _verts = d3d9_cg_font_get_scratch(font, _rl_msg_len * 6);

               if (_verts)
               {
                  _inv_vp_w  = 1.0f / (float)_rl_width;
                  _inv_vp_h  = 1.0f / (float)_rl_height;
                  _inv_tex_w = 1.0f / (float)font->tex_width;
                  _inv_tex_h = 1.0f / (float)font->tex_height;
                  _glyph_q   = font->font_driver->get_glyph(font->font_data, '?');

                  /* Handle text alignment */
                  if (text_align == TEXT_ALIGN_RIGHT || text_align == TEXT_ALIGN_CENTER)
                  {
                     int _width_accum = 0;
                     const char *_scan = _rl_msg;
                     const char *_scan_end = _rl_msg + _rl_msg_len;
                     while (_scan < _scan_end)
                     {
                        const struct font_glyph *_glyph;
                        uint32_t _code = utf8_walk(&_scan);
                        if (!(_glyph = font->font_driver->get_glyph(font->font_data, _code)))
                           if (!(_glyph = _glyph_q))
                              continue;
                        _width_accum += _glyph->advance_x;
                     }
                     if (text_align == TEXT_ALIGN_RIGHT)
                        _rl_pos_x -= (float)(_width_accum * scale) / (float)_rl_width;
                     else
                        _rl_pos_x -= (float)(_width_accum * scale) / (float)_rl_width / 2.0f;
                  }

                  _rx = roundf(_rl_pos_x * _rl_width);
                  _ry = roundf((1.0f - _rl_pos_y) * _rl_height);

                  for (_i = 0; _i < _rl_msg_len; _i++)
                  {
                     const struct font_glyph *_glyph;
                     const char *_msg_tmp = &_rl_msg[_i];
                     unsigned    _code    = utf8_walk(&_msg_tmp);
                     unsigned    _skip    = _msg_tmp - &_rl_msg[_i];

                     if (_skip > 1)
                        _i += _skip - 1;

                     if (!(_glyph = font->font_driver->get_glyph(font->font_data, _code)))
                        if (!(_glyph = _glyph_q))
                           continue;

                     _vert_count += d3d9_cg_font_emit_quad(
                           &_verts[_vert_count],
                           (_rx + _glyph->draw_offset_x * scale) * _inv_vp_w,
                           (_ry + _glyph->draw_offset_y * scale) * _inv_vp_h,
                           _glyph->width  * scale * _inv_vp_w,
                           _glyph->height * scale * _inv_vp_h,
                           _glyph->atlas_offset_x * _inv_tex_w,
                           _glyph->atlas_offset_y * _inv_tex_h,
                           _glyph->width  * _inv_tex_w,
                           _glyph->height * _inv_tex_h,
                           _rl_color);

                     _rx += _glyph->advance_x * scale;
                     _ry += _glyph->advance_y * scale;
                  }

                  if (_vert_count > 0)
                  {
                     IDirect3DDevice9_SetTexture(d3d->dev, 0,
                           (IDirect3DBaseTexture9*)font->texture);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                     IDirect3DDevice9_DrawPrimitiveUP(d3d->dev,
                           D3DPT_TRIANGLELIST,
                           _vert_count / 3,
                           _verts,
                           sizeof(Vertex));

                     /* DrawPrimitiveUP unbinds stream source, re-bind for
                      * subsequent display draws in the same frame */
                     IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
                           (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
                           0, sizeof(Vertex));
                  }
               }
            }
         }
            }

            /* Main text pass */
         {
            unsigned _i;
            float _inv_vp_w, _inv_vp_h;
            float _inv_tex_w, _inv_tex_h;
            const struct font_glyph *_glyph_q = NULL;
            unsigned _rl_width                 = 0;
            unsigned _rl_height                = 0;
            int _rx, _ry;
            unsigned _vert_count               = 0;
            Vertex *_verts                     = NULL;
            const char *_rl_msg                = m;
            size_t _rl_msg_len                 = msg_len;
            float _rl_pos_x                    = x;
            float _rl_pos_y                    = line_y;
            D3DCOLOR _rl_color                 = color;

            video_driver_get_size(&_rl_width, &_rl_height);
            if (_rl_width && _rl_height)
            {
               _verts = d3d9_cg_font_get_scratch(font, _rl_msg_len * 6);

               if (_verts)
               {
                  _inv_vp_w  = 1.0f / (float)_rl_width;
                  _inv_vp_h  = 1.0f / (float)_rl_height;
                  _inv_tex_w = 1.0f / (float)font->tex_width;
                  _inv_tex_h = 1.0f / (float)font->tex_height;
                  _glyph_q   = font->font_driver->get_glyph(font->font_data, '?');

                  /* Handle text alignment */
                  if (text_align == TEXT_ALIGN_RIGHT || text_align == TEXT_ALIGN_CENTER)
                  {
                     int _width_accum = 0;
                     const char *_scan = _rl_msg;
                     const char *_scan_end = _rl_msg + _rl_msg_len;
                     while (_scan < _scan_end)
                     {
                        const struct font_glyph *_glyph;
                        uint32_t _code = utf8_walk(&_scan);
                        if (!(_glyph = font->font_driver->get_glyph(font->font_data, _code)))
                           if (!(_glyph = _glyph_q))
                              continue;
                        _width_accum += _glyph->advance_x;
                     }
                     if (text_align == TEXT_ALIGN_RIGHT)
                        _rl_pos_x -= (float)(_width_accum * scale) / (float)_rl_width;
                     else
                        _rl_pos_x -= (float)(_width_accum * scale) / (float)_rl_width / 2.0f;
                  }

                  _rx = roundf(_rl_pos_x * _rl_width);
                  _ry = roundf((1.0f - _rl_pos_y) * _rl_height);

                  for (_i = 0; _i < _rl_msg_len; _i++)
                  {
                     const struct font_glyph *_glyph;
                     const char *_msg_tmp = &_rl_msg[_i];
                     unsigned    _code    = utf8_walk(&_msg_tmp);
                     unsigned    _skip    = _msg_tmp - &_rl_msg[_i];

                     if (_skip > 1)
                        _i += _skip - 1;

                     if (!(_glyph = font->font_driver->get_glyph(font->font_data, _code)))
                        if (!(_glyph = _glyph_q))
                           continue;

                     _vert_count += d3d9_cg_font_emit_quad(
                           &_verts[_vert_count],
                           (_rx + _glyph->draw_offset_x * scale) * _inv_vp_w,
                           (_ry + _glyph->draw_offset_y * scale) * _inv_vp_h,
                           _glyph->width  * scale * _inv_vp_w,
                           _glyph->height * scale * _inv_vp_h,
                           _glyph->atlas_offset_x * _inv_tex_w,
                           _glyph->atlas_offset_y * _inv_tex_h,
                           _glyph->width  * _inv_tex_w,
                           _glyph->height * _inv_tex_h,
                           _rl_color);

                     _rx += _glyph->advance_x * scale;
                     _ry += _glyph->advance_y * scale;
                  }

                  if (_vert_count > 0)
                  {
                     IDirect3DDevice9_SetTexture(d3d->dev, 0,
                           (IDirect3DBaseTexture9*)font->texture);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                     IDirect3DDevice9_DrawPrimitiveUP(d3d->dev,
                           D3DPT_TRIANGLELIST,
                           _vert_count / 3,
                           _verts,
                           sizeof(Vertex));

                     /* DrawPrimitiveUP unbinds stream source, re-bind for
                      * subsequent display draws in the same frame */
                     IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
                           (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
                           0, sizeof(Vertex));
                  }
               }
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

static const struct font_glyph *d3d9_cg_font_get_glyph(
      void *data, uint32_t code)
{
   d3d9_cg_font_t *font = (d3d9_cg_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph(
            (void*)font->font_data, code);
   return NULL;
}

static bool d3d9_cg_font_get_line_metrics(
      void *data, struct font_line_metrics **metrics)
{
   d3d9_cg_font_t *font = (d3d9_cg_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t d3d9_cg_font = {
   d3d9_cg_font_init,
   d3d9_cg_font_free,
   d3d9_cg_font_render_msg,
   "d3d9_cg",
   d3d9_cg_font_get_glyph,
   NULL, /* bind_block */
   NULL, /* flush */
   d3d9_cg_font_get_message_width,
   d3d9_cg_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

static INLINE bool d3d9_cg_validate_param_name(const char *name)
{
   int i;
   static const char *illegal[] = {
      "PREV.",
      "PREV1.",
      "PREV2.",
      "PREV3.",
      "PREV4.",
      "PREV5.",
      "PREV6.",
      "ORIG.",
      "IN.",
      "PASS",
   };

   if (!name)
      return false;

   for (i = 0; i < (int)ARRAY_SIZE(illegal); i++)
      if (strstr(name, illegal[i]) == name)
         return false;

   return true;
}

static INLINE CGparameter d3d9_cg_find_param_from_semantic(
      CGparameter param, const char *sem)
{
   for (; param; param = cgGetNextParameter(param))
   {
      const char *semantic = NULL;
      if (cgGetParameterType(param) == CG_STRUCT)
      {
         CGparameter ret = d3d9_cg_find_param_from_semantic(
               cgGetFirstStructParameter(param), sem);

         if (ret)
            return ret;
      }

      if (     (cgGetParameterDirection(param)   != CG_IN)
            || (cgGetParameterVariability(param) != CG_VARYING))
         continue;

      semantic = cgGetParameterSemantic(param);
      if (!semantic)
         continue;

      if (     string_is_equal(sem, semantic)
            && d3d9_cg_validate_param_name(cgGetParameterName(param)))
         return param;
   }

   return NULL;
}

static bool d3d9_cg_load_program(cg_renderchain_t *chain,
      struct shader_pass *pass,
      const char *prog, bool path_is_file)
{
   const char *list           = NULL;
   char *listing_f            = NULL;
   char *listing_v            = NULL;
   CGprofile vertex_profile   = cgD3D9GetLatestVertexProfile();
   CGprofile fragment_profile = cgD3D9GetLatestPixelProfile();
   const char **fragment_opts = cgD3D9GetOptimalOptions(fragment_profile);
   const char **vertex_opts   = cgD3D9GetOptimalOptions(vertex_profile);
   CGcontext cgCtx            = chain->cgCtx;

   /* Build new option arrays with -DPARAMETER_UNIFORM appended.
    * Without this define, shaders that guard their uniform parameter
    * declarations with #ifdef PARAMETER_UNIFORM (e.g. CRT-Caligari)
    * fall through to #define constants, making the parameters
    * impossible to change at runtime via cgD3D9SetUniform. */
   const char *f_opts_with_param[64];
   const char *v_opts_with_param[64];
   unsigned f_count = 0;
   unsigned v_count = 0;

   if (fragment_opts)
      for (; fragment_opts[f_count] && f_count < 62; f_count++)
         f_opts_with_param[f_count] = fragment_opts[f_count];
   f_opts_with_param[f_count++] = "-DPARAMETER_UNIFORM";
   f_opts_with_param[f_count]   = NULL;

   if (vertex_opts)
      for (; vertex_opts[v_count] && v_count < 62; v_count++)
         v_opts_with_param[v_count] = vertex_opts[v_count];
   v_opts_with_param[v_count++] = "-DPARAMETER_UNIFORM";
   v_opts_with_param[v_count]   = NULL;

   if (
            (fragment_profile == CG_PROFILE_UNKNOWN)
         || (vertex_profile   == CG_PROFILE_UNKNOWN))
   {
      RARCH_ERR("[D3D9 Cg] Invalid profile type.\n");
      goto error;
   }

   RARCH_LOG("[D3D9 Cg] Vertex profile: %s.\n",
         cgGetProfileString(vertex_profile));
   RARCH_LOG("[D3D9 Cg] Fragment profile: %s.\n",
         cgGetProfileString(fragment_profile));

   if (path_is_file && prog && *prog)
      pass->fprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
            prog, fragment_profile, "main_fragment", f_opts_with_param);
   else
      pass->fprg = cgCreateProgram(cgCtx, CG_SOURCE, stock_cg_d3d9_program,
            fragment_profile, "main_fragment", f_opts_with_param);

   list = cgGetLastListing(cgCtx);
   if (list)
      listing_f = strdup(list);

   if (path_is_file && prog && *prog)
      pass->vprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
            prog, vertex_profile, "main_vertex", v_opts_with_param);
   else
      pass->vprg = cgCreateProgram(cgCtx, CG_SOURCE, stock_cg_d3d9_program,
            vertex_profile, "main_vertex", v_opts_with_param);

   list = cgGetLastListing(cgCtx);
   if (list)
      listing_v = strdup(list);

   if (!pass->fprg || !pass->vprg)
      goto error;

   cgD3D9LoadProgram((CGprogram)pass->fprg, true, 0);
   {
      CGerror _cg_err = cgGetError();
      if (_cg_err != CG_NO_ERROR)
      {
         RARCH_ERR("[D3D9 Cg] LoadProgram (fragment) failed: %s.\n",
               cgGetErrorString(_cg_err));
         goto error;
      }
   }
   cgD3D9LoadProgram((CGprogram)pass->vprg, true, 0);
   {
      CGerror _cg_err = cgGetError();
      if (_cg_err != CG_NO_ERROR)
      {
         RARCH_ERR("[D3D9 Cg] LoadProgram (vertex) failed: %s.\n",
               cgGetErrorString(_cg_err));
         goto error;
      }
   }

   free(listing_f);
   free(listing_v);

   return true;

error:
   RARCH_ERR("[D3D9 Cg] Cg error: %s.\n", cgGetErrorString(cgGetError()));
   if (listing_f)
      RARCH_ERR("[D3D9 Cg] Fragment: %s.\n", listing_f);
   else if (listing_v)
      RARCH_ERR("[D3D9 Cg] Vertex: %s.\n", listing_v);
   free(listing_f);
   free(listing_v);

   return false;
}

static bool d3d9_cg_renderchain_init_shader_fvf(
      d3d9_cg_renderchain_t *chain,
      struct shader_pass *pass)
{
   CGparameter param;
   unsigned index, i, count;
   unsigned tex_index                          = 0;
   bool texcoord0_taken                        = false;
   bool texcoord1_taken                        = false;
   bool stream_taken[4]                        = {false};
   static const D3DVERTEXELEMENT9 decl_end     = D3DDECL_END();
   D3DVERTEXELEMENT9 decl[MAXD3DDECLLENGTH]    = {{0}};
   bool *indices                               = NULL;
   CGprogram fprg                              = (CGprogram)pass->fprg;
   CGprogram vprg                              = (CGprogram)pass->vprg;

   if (cgD3D9GetVertexDeclaration(vprg, decl) == CG_FALSE)
      return false;

   for (count = 0; count < MAXD3DDECLLENGTH; count++)
   {
      if (memcmp(&decl_end, &decl[count], sizeof(decl_end)) == 0)
         break;
   }

   /* This is completely insane.
    * We do not have a good and easy way of setting up our
    * attribute streams, so we have to do it ourselves, yay!
    *
    * Stream 0      => POSITION
    * Stream 1      => TEXCOORD0
    * Stream 2      => TEXCOORD1
    * Stream 3      => COLOR     (Not really used for anything.)
    * Stream {4..N} => Texture coord streams for varying resources
    *                  which have no semantics.
    */

   indices  = (bool*)calloc(1, count * sizeof(*indices));

   param    = d3d9_cg_find_param_from_semantic(cgGetFirstParameter(vprg, CG_PROGRAM), "POSITION");
   if (!param)
      param = d3d9_cg_find_param_from_semantic(cgGetFirstParameter(vprg, CG_PROGRAM), "POSITION0");

   if (param)
   {
      static const D3DVERTEXELEMENT9 element =
      {
         0, 0 * sizeof(float),
         D3DDECLTYPE_FLOAT3,
         D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_POSITION,
         0
      };
      stream_taken[0] = true;
      index           = cgGetParameterResourceIndex(param);
      decl[index]     = element;
      indices[index]  = true;

      RARCH_LOG("[D3D9 Cg] FVF POSITION semantic found.\n");
   }

   param = d3d9_cg_find_param_from_semantic(cgGetFirstParameter(vprg, CG_PROGRAM), "TEXCOORD");
   if (!param)
      param = d3d9_cg_find_param_from_semantic(cgGetFirstParameter(vprg, CG_PROGRAM), "TEXCOORD0");

   if (param)
   {
      static const D3DVERTEXELEMENT9 tex_coord0 = D3D9_DECL_FVF_TEXCOORD(1, 3, 0);
      stream_taken[1] = true;
      texcoord0_taken = true;
      index           = cgGetParameterResourceIndex(param);
      decl[index]     = tex_coord0;
      indices[index]  = true;

      RARCH_LOG("[D3D9 Cg] FVF TEXCOORD0 semantic found.\n");
   }

   param = d3d9_cg_find_param_from_semantic(cgGetFirstParameter(vprg, CG_PROGRAM), "TEXCOORD1");
   if (param)
   {
      static const D3DVERTEXELEMENT9 tex_coord1    = D3D9_DECL_FVF_TEXCOORD(2, 5, 1);
      stream_taken[2] = true;
      texcoord1_taken = true;
      index           = cgGetParameterResourceIndex(param);
      decl[index]     = tex_coord1;
      indices[index]  = true;

      RARCH_LOG("[D3D9 Cg] FVF TEXCOORD1 semantic found.\n");
   }

   param = d3d9_cg_find_param_from_semantic(cgGetFirstParameter(vprg, CG_PROGRAM), "COLOR");
   if (!param)
      param = d3d9_cg_find_param_from_semantic(cgGetFirstParameter(vprg, CG_PROGRAM), "COLOR0");

   if (param)
   {
      static const D3DVERTEXELEMENT9 color = DECL_FVF_COLOR(3, 7, 0);
      stream_taken[3] = true;
      index           = cgGetParameterResourceIndex(param);
      decl[index]     = color;
      indices[index]  = true;

      RARCH_LOG("[D3D9 Cg] FVF COLOR0 semantic found.\n");
   }

   /* Stream {0, 1, 2, 3} might be already taken. Find first vacant stream. */
   for (index = 0; index < 4; index++)
   {
      if (!stream_taken[index])
         break;
   }

   /* Find first vacant texcoord declaration. */
   if (texcoord0_taken && texcoord1_taken)
      tex_index = 2;
   else if (texcoord1_taken && !texcoord0_taken)
      tex_index = 0;
   else if (texcoord0_taken && !texcoord1_taken)
      tex_index = 1;

   for (i = 0; i < count; i++)
   {
      if (indices[i])
         unsigned_vector_list_append((struct unsigned_vector_list *)
               pass->attrib_map, 0);
      else
      {
         D3DVERTEXELEMENT9 elem = D3D9_DECL_FVF_TEXCOORD(index, 3, tex_index);

         unsigned_vector_list_append((struct unsigned_vector_list *)
               pass->attrib_map, index);

         decl[i]     = elem;

         /* Find next vacant stream. */
         while ((++index < 4) && stream_taken[index])
            index++;

         /* Find next vacant texcoord declaration. */
         if ((++tex_index == 1) && texcoord1_taken)
            tex_index++;
      }
   }

   free(indices);

   return (SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(chain->dev,
               (const D3DVERTEXELEMENT9*)decl,
               (IDirect3DVertexDeclaration9**)&pass->vertex_decl)));
}

static void d3d9_cg_renderchain_bind_orig(
      d3d9_cg_renderchain_t *chain,
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass)
{
   CGparameter param;
   float video_size[2];
   float texture_size[2];
   struct shader_pass *first_pass = (struct shader_pass*)&chain->passes->data[0];
   CGprogram fprg             = (CGprogram)pass->fprg;
   CGprogram vprg             = (CGprogram)pass->vprg;
   video_size[0]              = first_pass->last_width;
   video_size[1]              = first_pass->last_height;
   texture_size[0]            = first_pass->info.tex_w;
   texture_size[1]            = first_pass->info.tex_h;

   /* Vertex program */
   param                      = cgGetNamedParameter(vprg, "ORIG.video_size");
   if (param)
      cgD3D9SetUniform(param, &video_size);
   param                      = cgGetNamedParameter(vprg, "ORIG.texture_size");
   if (param)
      cgD3D9SetUniform(param, &texture_size);

   /* Fragment program */
   param                      = cgGetNamedParameter(fprg, "ORIG.video_size");
   if (param)
      cgD3D9SetUniform(param, &video_size);
   param                      = cgGetNamedParameter(fprg, "ORIG.texture_size");
   if (param)
      cgD3D9SetUniform(param, &texture_size);

   param = cgGetNamedParameter(fprg, "ORIG.texture");

   if (param)
   {
      unsigned index = cgGetParameterResourceIndex(param);
      int32_t orig_filter = d3d_translate_filter(first_pass->info.pass->filter);
      IDirect3DDevice9_SetTexture(chain->dev, index, (IDirect3DBaseTexture9*)first_pass->tex);
      IDirect3DDevice9_SetSamplerState(chain->dev,
            index, D3DSAMP_MINFILTER, orig_filter);
      IDirect3DDevice9_SetSamplerState(chain->dev,
            index, D3DSAMP_MAGFILTER, orig_filter);
      IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      unsigned_vector_list_append(chain->bound_tex, index);
   }

   param = cgGetNamedParameter(vprg, "ORIG.tex_coord");
   if (param)
   {
      LPDIRECT3DVERTEXBUFFER9 vert_buf = (LPDIRECT3DVERTEXBUFFER9)first_pass->vertex_buf;
      struct unsigned_vector_list *attrib_map = (struct unsigned_vector_list*)
         pass->attrib_map;
      unsigned index = attrib_map->data[cgGetParameterResourceIndex(param)];

      IDirect3DDevice9_SetStreamSource(chain->dev, index, vert_buf, 0,
sizeof(struct D3D9CGVertex));
      unsigned_vector_list_append(chain->bound_vert, index);
   }
}

static void d3d9_cg_renderchain_bind_prev(d3d9_cg_renderchain_t *chain,
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass)
{
   unsigned i;
   float texture_size[2];
   int32_t prev_filter = d3d_translate_filter(chain->passes->data[0].info.pass->filter);
   static const char *prev_names[] = {
      "PREV",
      "PREV1",
      "PREV2",
      "PREV3",
      "PREV4",
      "PREV5",
      "PREV6",
   };

   texture_size[0] = chain->passes->data[0].info.tex_w;
   texture_size[1] = chain->passes->data[0].info.tex_h;

   for (i = 0; i < TEXTURES - 1; i++)
   {
      char attr[64];
      CGparameter param;
      float video_size[2];
      CGprogram fprg = (CGprogram)pass->fprg;
      CGprogram vprg = (CGprogram)pass->vprg;
      size_t _len    = strlcpy(attr, prev_names[i], sizeof(attr));

      video_size[0]  = chain->prev.last_width[
         (chain->prev.ptr - (i + 1)) & TEXTURESMASK];
      video_size[1]  = chain->prev.last_height[
         (chain->prev.ptr - (i + 1)) & TEXTURESMASK];

      strlcpy(attr + _len, ".texture", sizeof(attr) - _len);
      param = cgGetNamedParameter(fprg, attr);
      if (param)
      {
         unsigned         index = cgGetParameterResourceIndex(param);
         LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)
            chain->prev.tex[
            (chain->prev.ptr - (i + 1)) & TEXTURESMASK];

         IDirect3DDevice9_SetTexture(chain->dev, index, (IDirect3DBaseTexture9*)tex);
         unsigned_vector_list_append(chain->bound_tex, index);

         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_MINFILTER,
               prev_filter);
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_MAGFILTER,
               prev_filter);
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      }

      strlcpy(attr + _len, ".tex_coord", sizeof(attr) - _len);
      param = cgGetNamedParameter(vprg, attr);
      if (param)
      {
         LPDIRECT3DVERTEXBUFFER9 vert_buf = (LPDIRECT3DVERTEXBUFFER9)
            chain->prev.vertex_buf[
            (chain->prev.ptr - (i + 1)) & TEXTURESMASK];
         struct unsigned_vector_list *attrib_map = (struct unsigned_vector_list*)pass->attrib_map;
         unsigned index = attrib_map->data[cgGetParameterResourceIndex(param)];

         IDirect3DDevice9_SetStreamSource(chain->dev, index, vert_buf, 0,
               sizeof(struct D3D9CGVertex));
         unsigned_vector_list_append(chain->bound_vert, index);
      }

      strlcpy(attr + _len, ".video_size",   sizeof(attr) - _len);

      param = cgGetNamedParameter(vprg, attr);
      if (param)
         cgD3D9SetUniform(param, &video_size);
      param = cgGetNamedParameter(fprg, attr);
      if (param)
         cgD3D9SetUniform(param, &video_size);

      strlcpy(attr + _len, ".texture_size", sizeof(attr) - _len);
      param = cgGetNamedParameter(vprg, attr);
      if (param)
         cgD3D9SetUniform(param, &texture_size);
      param = cgGetNamedParameter(fprg, attr);
      if (param)
         cgD3D9SetUniform(param, &texture_size);

   }
}

static void d3d9_cg_renderchain_bind_pass(
      d3d9_cg_renderchain_t *chain,
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass, unsigned pass_index)
{
   unsigned i;
   CGprogram fprg               = (CGprogram)pass->fprg;
   CGprogram vprg               = (CGprogram)pass->vprg;

   for (i = 1; i < pass_index - 1; i++)
   {
      CGparameter param;
      float video_size[2];
      float texture_size[2];
      char pass_base[64];
      struct shader_pass *curr_pass = (struct shader_pass*)&chain->passes->data[i];
      size_t _len = snprintf(pass_base, sizeof(pass_base), "PASS%u", i);

      video_size[0]   = curr_pass->last_width;
      video_size[1]   = curr_pass->last_height;
      texture_size[0] = curr_pass->info.tex_w;
      texture_size[1] = curr_pass->info.tex_h;

      strlcpy(pass_base + _len, ".texture",  sizeof(pass_base) - _len);
      param = cgGetNamedParameter(fprg, pass_base);
      if (param)
      {
         unsigned index = cgGetParameterResourceIndex(param);
         int32_t pass_filter = d3d_translate_filter(curr_pass->info.pass->filter);
         unsigned_vector_list_append(chain->bound_tex, index);

         IDirect3DDevice9_SetTexture(chain->dev, index, (IDirect3DBaseTexture9*)curr_pass->tex);
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_MINFILTER,
               pass_filter);
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_MAGFILTER,
               pass_filter);
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      }

      strlcpy(pass_base + _len, ".tex_coord", sizeof(pass_base) - _len);
      param = cgGetNamedParameter(vprg, pass_base);
      if (param)
      {
         struct unsigned_vector_list *attrib_map =
            (struct unsigned_vector_list*)pass->attrib_map;
         unsigned index = attrib_map->data[cgGetParameterResourceIndex(param)];

         IDirect3DDevice9_SetStreamSource(chain->dev, index,
               pass->vertex_buf, 0,
               sizeof(struct D3D9CGVertex));
         unsigned_vector_list_append(chain->bound_vert, index);
      }

      strlcpy(pass_base + _len, ".video_size",   sizeof(pass_base) - _len);
      param           = cgGetNamedParameter(vprg, pass_base);
      if (param)
         cgD3D9SetUniform(param, &video_size);
      param           = cgGetNamedParameter(fprg, pass_base);
      if (param)
         cgD3D9SetUniform(param, &video_size);

      strlcpy(pass_base + _len, ".texture_size", sizeof(pass_base) - _len);
      param           = cgGetNamedParameter(vprg, pass_base);
      if (param)
         cgD3D9SetUniform(param, &texture_size);
      param           = cgGetNamedParameter(fprg, pass_base);
      if (param)
         cgD3D9SetUniform(param, &texture_size);

   }
}

static void d3d9_cg_deinit_progs(cg_renderchain_t *chain)
{
   int i;

   if (chain->chain.passes->count >= 1)
   {
      d3d9_vertex_buffer_free(NULL, chain->chain.passes->data[0].vertex_decl);

      for (i = 1; i < (int)chain->chain.passes->count; i++)
      {
         if (chain->chain.passes->data[i].tex)
            IDirect3DTexture9_Release(chain->chain.passes->data[i].tex);
         chain->chain.passes->data[i].tex = NULL;
         d3d9_vertex_buffer_free(
               chain->chain.passes->data[i].vertex_buf,
               chain->chain.passes->data[i].vertex_decl);

         if (chain->chain.passes->data[i].fprg)
            cgDestroyProgram((CGprogram)chain->chain.passes->data[i].fprg);
         if (chain->chain.passes->data[i].vprg)
            cgDestroyProgram((CGprogram)chain->chain.passes->data[i].vprg);
      }
   }

   if (chain->stock_shader.fprg)
      cgDestroyProgram((CGprogram)chain->stock_shader.fprg);
   if (chain->stock_shader.vprg)
      cgDestroyProgram((CGprogram)chain->stock_shader.vprg);
}

void d3d9_cg_renderchain_free(void *data)
{
   int i;
   cg_renderchain_t *chain = (cg_renderchain_t*)data;

   if (!chain)
      return;

   /* Release previous frame textures and vertex buffers */
   for (i = 0; i < TEXTURES; i++)
   {
      if (chain->chain.prev.tex[i])
         IDirect3DTexture9_Release(chain->chain.prev.tex[i]);
      if (chain->chain.prev.vertex_buf[i])
         d3d9_vertex_buffer_free(chain->chain.prev.vertex_buf[i], NULL);
   }

   d3d9_cg_deinit_progs(chain);

   for (i = 0; i < (int)chain->chain.luts->count; i++)
   {
      if (chain->chain.luts->data[i].tex)
         IDirect3DTexture9_Release(chain->chain.luts->data[i].tex);
   }

   cgD3D9UnloadAllPrograms();
   cgD3D9SetDevice(NULL);

   /* Free passes and LUT lists */
   if (chain->chain.passes)
   {
      for (i = 0; i < (int) chain->chain.passes->count; i++)
      {
         if (chain->chain.passes->data[i].attrib_map)
            free(chain->chain.passes->data[i].attrib_map);
      }
      shader_pass_vector_list_free(chain->chain.passes);
   }

   lut_info_vector_list_free(chain->chain.luts);
   unsigned_vector_list_free(chain->chain.bound_tex);
   unsigned_vector_list_free(chain->chain.bound_vert);

   if (chain->cgCtx)
      cgDestroyContext(chain->cgCtx);

   free(chain);
}

static bool d3d9_cg_renderchain_init_shader(d3d9_video_t *d3d,
      cg_renderchain_t *renderchain)
{
   CGcontext cgCtx    = cgCreateContext();

   if (!cgCtx)
   {
      RARCH_ERR("[D3D9 Cg] Failed to create Cg context.\n");
      return false;
   }

   if (FAILED(cgD3D9SetDevice((IDirect3DDevice9*)d3d->dev)))
      return false;

   renderchain->cgCtx = cgCtx;
   return true;
}

static bool d3d9_cg_renderchain_create_first_pass(
      LPDIRECT3DDEVICE9 dev,
      cg_renderchain_t   *cg_chain,
      d3d9_cg_renderchain_t *chain,
      const struct LinkInfo *info, unsigned _fmt)
{
   unsigned i;
   struct shader_pass pass;
   math_matrix_4x4 ident;
   unsigned fmt = (_fmt == RETRO_PIXEL_FORMAT_RGB565) ?
      D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;

   matrix_4x4_identity(ident);

   IDirect3DDevice9_SetTransform(dev, D3DTS_WORLD, (D3DMATRIX*)&ident);
   IDirect3DDevice9_SetTransform(dev, D3DTS_VIEW,  (D3DMATRIX*)&ident);

   pass.info        = *info;
   pass.last_width  = 0;
   pass.last_height = 0;
   pass.attrib_map  = (struct unsigned_vector_list*)
      unsigned_vector_list_new();

   chain->prev.ptr  = 0;

   {
      int32_t init_filter = d3d_translate_filter(info->pass->filter);
      for (i = 0; i < TEXTURES; i++)
      {
         chain->prev.last_width[i]  = 0;
         chain->prev.last_height[i] = 0;
         chain->prev.vertex_buf[i]  = NULL;
            {
               void *_vbuf = NULL;
               if (SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
                           chain->dev, 4 * sizeof(struct D3D9CGVertex),
                           D3DUSAGE_WRITEONLY, 0,
                           D3DPOOL_DEFAULT,
                           (LPDIRECT3DVERTEXBUFFER9*)&_vbuf, NULL)))
                  chain->prev.vertex_buf[i] = (LPDIRECT3DVERTEXBUFFER9)_vbuf;
            }

         if (!chain->prev.vertex_buf[i])
            return false;

         chain->prev.tex[i] = NULL;
         {
            void *_tbuf = NULL;
            if (SUCCEEDED(IDirect3DDevice9_CreateTexture(chain->dev,
                        info->tex_w, info->tex_h, 1, 0,
                        (D3DFORMAT)fmt, D3DPOOL_MANAGED,
                        (struct IDirect3DTexture9**)&_tbuf, NULL)))
               chain->prev.tex[i] = (LPDIRECT3DTEXTURE9)_tbuf;
         }

         if (!chain->prev.tex[i])
            return false;

         IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)chain->prev.tex[i]);
         IDirect3DDevice9_SetSamplerState(dev,
               0, D3DSAMP_MINFILTER, init_filter);
         IDirect3DDevice9_SetSamplerState(dev,
               0, D3DSAMP_MAGFILTER, init_filter);
         IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
         IDirect3DDevice9_SetTexture(chain->dev, 0, NULL);
      }
   }

   d3d9_cg_load_program((cg_renderchain_t*)cg_chain, &pass, info->pass->source.path, true);

   if (!d3d9_cg_renderchain_init_shader_fvf(chain, &pass))
      return false;
   shader_pass_vector_list_append(chain->passes, pass);
   return true;
}

static bool d3d9_cg_renderchain_init(
      d3d9_video_t *d3d,
      LPDIRECT3DDEVICE9 dev,
      const D3DVIEWPORT9 *out_vp,
      const struct LinkInfo *info,
      bool rgb32)
{
   cg_renderchain_t *chain        = (cg_renderchain_t*)d3d->renderchain_data;
   unsigned fmt                   = (rgb32) ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;

   if (!d3d9_cg_renderchain_init_shader(d3d, chain))
   {
      RARCH_ERR("[D3D9 Cg] Failed to initialize shader subsystem.\n");
      return false;
   }

   chain->chain.dev            = dev;
   chain->chain.out_vp         = (D3DVIEWPORT9*)out_vp;
   chain->chain.pixel_size     = (fmt == RETRO_PIXEL_FORMAT_RGB565) ? 2 : 4;

   if (!d3d9_cg_renderchain_create_first_pass(dev, chain, &chain->chain, info, fmt))
      return false;
   if (!d3d9_cg_load_program((cg_renderchain_t*)chain, &chain->stock_shader, NULL, false))
      return false;

   cgD3D9BindProgram((CGprogram)chain->stock_shader.fprg);
   cgD3D9BindProgram((CGprogram)chain->stock_shader.vprg);
   CG_DEBUG_CHECK(chain->cgCtx, "renderchain_init BindProgram");

   return true;
}

static bool d3d9_cg_renderchain_add_pass(void *data, const struct LinkInfo *info)
{
   LPDIRECT3DTEXTURE9      tex = NULL;
   LPDIRECT3DVERTEXBUFFER9 vertbuf = NULL;
   struct shader_pass pass;
   cg_renderchain_t *chain     = (cg_renderchain_t*)data;
   d3d9_cg_renderchain_t *_chain = &chain->chain;

   pass.info                   = *info;
   pass.last_width             = 0;
   pass.last_height            = 0;
   pass.attrib_map             = (struct unsigned_vector_list*)
      unsigned_vector_list_new();
   pass.pool                   = D3DPOOL_DEFAULT;

   d3d9_cg_load_program((cg_renderchain_t*)chain, &pass, info->pass->source.path, true);

   if (!d3d9_cg_renderchain_init_shader_fvf(_chain, &pass))
      return false;

   if (!SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
               _chain->dev, 4 * sizeof(struct Vertex),
               D3DUSAGE_WRITEONLY, 0,
               D3DPOOL_DEFAULT,
               (LPDIRECT3DVERTEXBUFFER9*)&vertbuf, NULL)))
      vertbuf = NULL;

   if (!vertbuf)
      return false;

   pass.vertex_buf = vertbuf;

   {
      void *_tbuf = NULL;
      if (SUCCEEDED(IDirect3DDevice9_CreateTexture(_chain->dev,
                  info->tex_w, info->tex_h, 1,
                  D3DUSAGE_RENDERTARGET,
                  (_chain->passes->data[
                  _chain->passes->count - 1].info.pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
                  ? D3DFMT_A32B32G32R32F : D3DFMT_A8R8G8B8,
                  D3DPOOL_DEFAULT,
                  (struct IDirect3DTexture9**)&_tbuf, NULL)))
         tex = (LPDIRECT3DTEXTURE9)_tbuf;
   }

   if (!tex)
      return false;

   pass.tex        = tex;

   IDirect3DDevice9_SetTexture(_chain->dev, 0, (IDirect3DBaseTexture9*)pass.tex);
   IDirect3DDevice9_SetSamplerState(_chain->dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(_chain->dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetTexture(_chain->dev, 0, (IDirect3DBaseTexture9*)NULL);

   shader_pass_vector_list_append(_chain->passes, pass);

   return true;
}

static void d3d9_cg_renderchain_calc_and_set_shader_mvp(
      CGprogram data, /* stock vertex program */
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   math_matrix_4x4 matrix;
   CGparameter cgp = cgGetNamedParameter(data, "modelViewProj");

   /* Folded: transpose(ortho(0,W,0,H,0,1) * rot_z(angle)) */
   {
      float angle = rotation * (D3D_PI / 2.0);
      float c     = cosf(angle);
      float s     = sinf(angle);
      float rw    = 2.0f / vp_width;
      float rh    = 2.0f / vp_height;
      memset(&matrix, 0, sizeof(matrix));
      MAT_ELEM_4X4(matrix, 0, 0) =  rw * c;
      MAT_ELEM_4X4(matrix, 1, 0) = -rh * s;
      MAT_ELEM_4X4(matrix, 3, 0) = -c + s;
      MAT_ELEM_4X4(matrix, 0, 1) =  rw * s;
      MAT_ELEM_4X4(matrix, 1, 1) =  rh * c;
      MAT_ELEM_4X4(matrix, 3, 1) = -s - c;
      MAT_ELEM_4X4(matrix, 2, 2) =  1.0f;
      MAT_ELEM_4X4(matrix, 3, 3) =  1.0f;
   }

   if (cgp)
   {
      cgD3D9SetUniformMatrix(cgp, (D3DMATRIX*)&matrix);
      CG_DEBUG_CHECK(NULL, "calc_and_set_shader_mvp SetUniformMatrix");
   }
}

static void d3d9_cg_renderchain_render_pass(
      d3d9_cg_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned pass_index,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation,
      struct video_shader *shader,
      D3DVIEWPORT9 *vp)
{
   unsigned i;
   CGprogram fprg  = (CGprogram)pass->fprg;
   CGprogram vprg  = (CGprogram)pass->vprg;
   int32_t filter  = d3d_translate_filter(pass->info.pass->filter);

   IDirect3DDevice9_SetViewport(chain->dev, vp);

   /* Update vertex buffer if dimensions changed */
   if (pass->last_width != width || pass->last_height != height)
   {
      struct D3D9CGVertex vert[4];
      void *verts        = NULL;
      float          _u  = (float)(width)  / pass->info.tex_w;
      float          _v  = (float)(height) / pass->info.tex_h;

      pass->last_width   = width;
      pass->last_height  = height;

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
         vert[i].x -= 0.5f;
         vert[i].y += 0.5f;
      }

      IDirect3DVertexBuffer9_Lock(pass->vertex_buf, 0, 0, &verts, D3DLOCK_DISCARD);
      if (verts)
      {
         memcpy(verts, vert, sizeof(vert));
         IDirect3DVertexBuffer9_Unlock(pass->vertex_buf);
      }
   }

   d3d9_cg_renderchain_calc_and_set_shader_mvp(vprg, vp_width, vp_height, rotation);

   /* Set shader parameters */
   {
      float frame_cnt;
      float video_size[2];
      float texture_size[2];
      float output_size[2];

      CGparameter vp_video_size   = cgGetNamedParameter(vprg, "IN.video_size");
      CGparameter vp_texture_size = cgGetNamedParameter(vprg, "IN.texture_size");
      CGparameter vp_output_size  = cgGetNamedParameter(vprg, "IN.output_size");
      CGparameter vp_frame_count  = cgGetNamedParameter(vprg, "IN.frame_count");
      CGparameter fp_video_size   = cgGetNamedParameter(fprg, "IN.video_size");
      CGparameter fp_texture_size = cgGetNamedParameter(fprg, "IN.texture_size");
      CGparameter fp_output_size  = cgGetNamedParameter(fprg, "IN.output_size");
      CGparameter fp_frame_count  = cgGetNamedParameter(fprg, "IN.frame_count");

      video_size[0]        = width;
      video_size[1]        = height;
      texture_size[0]      = pass->info.tex_w;
      texture_size[1]      = pass->info.tex_h;
      output_size[0]       = vp_width;
      output_size[1]       = vp_height;

      frame_cnt            = chain->frame_count;

      if (pass->info.pass->frame_count_mod)
         frame_cnt         = chain->frame_count
            % pass->info.pass->frame_count_mod;

      if (vp_video_size)
         cgD3D9SetUniform(vp_video_size, &video_size);
      if (vp_texture_size)
         cgD3D9SetUniform(vp_texture_size, &texture_size);
      if (vp_output_size)
         cgD3D9SetUniform(vp_output_size, &output_size);
      if (vp_frame_count)
         cgD3D9SetUniform(vp_frame_count, &frame_cnt);
      CG_DEBUG_CHECK(NULL, "set_shader_params vertex uniforms");

      if (fp_video_size)
         cgD3D9SetUniform(fp_video_size, &video_size);
      if (fp_texture_size)
         cgD3D9SetUniform(fp_texture_size, &texture_size);
      if (fp_output_size)
         cgD3D9SetUniform(fp_output_size, &output_size);
      if (fp_frame_count)
         cgD3D9SetUniform(fp_frame_count, &frame_cnt);
      CG_DEBUG_CHECK(NULL, "set_shader_params fragment uniforms");

      if (shader)
      {
         for (i = 0; i < shader->num_parameters; i++)
         {
            CGparameter cgp_vp = cgGetNamedParameter(vprg, shader->parameters[i].id);
            CGparameter cgp_fp = cgGetNamedParameter(fprg, shader->parameters[i].id);

            if (cgp_vp)
               cgD3D9SetUniform(cgp_vp, &shader->parameters[i].current);
            if (cgp_fp)
               cgD3D9SetUniform(cgp_fp, &shader->parameters[i].current);
         }
         CG_DEBUG_CHECK(NULL, "set_shader_params user parameters");
      }
   }

   /* Bind and draw */
   cgD3D9BindProgram(fprg);
   cgD3D9BindProgram(vprg);
   CG_DEBUG_CHECK(NULL, "renderchain_render_pass BindProgram");

   IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)pass->tex);
   IDirect3DDevice9_SetSamplerState(chain->dev, 0, D3DSAMP_MINFILTER, filter);
   IDirect3DDevice9_SetSamplerState(chain->dev, 0, D3DSAMP_MAGFILTER, filter);

   IDirect3DDevice9_SetVertexDeclaration(chain->dev, pass->vertex_decl);
   for (i = 0; i < 4; i++)
      IDirect3DDevice9_SetStreamSource(chain->dev, i, pass->vertex_buf,
            0, sizeof(struct D3D9CGVertex));

   d3d9_cg_renderchain_bind_orig(chain, chain->dev, pass);
   d3d9_cg_renderchain_bind_prev(chain, chain->dev, pass);

   for (i = 0; i < chain->luts->count; i++)
   {
      CGparameter vparam;
      CGparameter fparam = cgGetNamedParameter(fprg, chain->luts->data[i].id);
      int bound_index    = -1;

      if (fparam)
      {
         unsigned index  = cgGetParameterResourceIndex(fparam);
         bound_index     = index;
         d3d9_cg_renderchain_add_lut_internal(chain, index, i);
      }

      vparam = cgGetNamedParameter(vprg, chain->luts->data[i].id);

      if (vparam)
      {
         unsigned index = cgGetParameterResourceIndex(vparam);
         if (index != (unsigned)bound_index)
            d3d9_cg_renderchain_add_lut_internal(chain, index, i);
      }
   }

   if (pass_index >= 3)
      d3d9_cg_renderchain_bind_pass(chain, chain->dev, pass, pass_index);

   IDirect3DDevice9_BeginScene(chain->dev);
   IDirect3DDevice9_DrawPrimitive(chain->dev, D3DPT_TRIANGLESTRIP, 0, 2);
   IDirect3DDevice9_EndScene(chain->dev);

   /* Unbind all textures and vertex streams */
   IDirect3DDevice9_SetSamplerState(chain->dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
   IDirect3DDevice9_SetSamplerState(chain->dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

   for (i = 0; i < chain->bound_tex->count; i++)
   {
      IDirect3DDevice9_SetSamplerState(chain->dev,
            chain->bound_tex->data[i], D3DSAMP_MINFILTER, D3DTEXF_POINT);
      IDirect3DDevice9_SetSamplerState(chain->dev,
            chain->bound_tex->data[i], D3DSAMP_MAGFILTER, D3DTEXF_POINT);
      IDirect3DDevice9_SetTexture(chain->dev,
            chain->bound_tex->data[i], (IDirect3DBaseTexture9*)NULL);
   }

   for (i = 0; i < chain->bound_vert->count; i++)
      IDirect3DDevice9_SetStreamSource(chain->dev, chain->bound_vert->data[i], 0, 0, 0);

   chain->bound_tex->count  = 0;
   chain->bound_vert->count = 0;
}

static void d3d9_cg_renderchain_render(
      d3d9_video_t *d3d,
      const void *frame_data,
      unsigned width, unsigned height,
      unsigned pitch, unsigned rotation)
{
   LPDIRECT3DSURFACE9 back_buffer, target;
   unsigned i, current_width, current_height, out_width = 0, out_height = 0;
   struct shader_pass *last_pass  = NULL;
   struct shader_pass *first_pass = NULL;
   cg_renderchain_t *_chain       = (cg_renderchain_t*)d3d->renderchain_data;
   d3d9_cg_renderchain_t *chain      = (d3d9_cg_renderchain_t*)&_chain->chain;

   /* Start render: copy prev frame state into pass 0 */
   chain->passes->data[0].tex         = chain->prev.tex[chain->prev.ptr];
   chain->passes->data[0].vertex_buf  = chain->prev.vertex_buf[chain->prev.ptr];
   chain->passes->data[0].last_width  = chain->prev.last_width[chain->prev.ptr];
   chain->passes->data[0].last_height = chain->prev.last_height[chain->prev.ptr];

   current_width              = width;
   current_height             = height;

   first_pass                 = (struct shader_pass*)&chain->passes->data[0];

   switch (first_pass->info.pass->fbo.type_x)
   {
      case RARCH_SCALE_VIEWPORT:
         out_width = first_pass->info.pass->fbo.scale_x * chain->out_vp->Width;
         break;
      case RARCH_SCALE_ABSOLUTE:
         out_width = first_pass->info.pass->fbo.abs_x;
         break;
      case RARCH_SCALE_INPUT:
         out_width = first_pass->info.pass->fbo.scale_x * current_width;
         break;
   }
   switch (first_pass->info.pass->fbo.type_y)
   {
      case RARCH_SCALE_VIEWPORT:
         out_height = first_pass->info.pass->fbo.scale_y * chain->out_vp->Height;
         break;
      case RARCH_SCALE_ABSOLUTE:
         out_height = first_pass->info.pass->fbo.abs_y;
         break;
      case RARCH_SCALE_INPUT:
         out_height = first_pass->info.pass->fbo.scale_y * current_height;
         break;
   }

   /* Blit frame data into the first pass texture */
   {
      D3DLOCKED_RECT d3dlr;
      d3dlr.Pitch = 0;
      d3dlr.pBits = NULL;

      if (SUCCEEDED(IDirect3DTexture9_LockRect(first_pass->tex, 0, &d3dlr, NULL,
            (first_pass->last_width == width && first_pass->last_height == height)
            ? 0 : D3DLOCK_NOSYSLOCK)) && d3dlr.pBits)
      {
         unsigned y;

         if (first_pass->last_width != width || first_pass->last_height != height)
            memset(d3dlr.pBits, 0, first_pass->info.tex_h * d3dlr.Pitch);

         for (y = 0; y < height; y++)
         {
            const uint8_t *in = (const uint8_t*)frame_data + y * pitch;
            uint8_t      *out = (uint8_t*)d3dlr.pBits      + y * d3dlr.Pitch;
            memcpy(out, in, width * chain->pixel_size);
         }
         IDirect3DTexture9_UnlockRect(first_pass->tex, 0);
      }
   }

   /* Grab back buffer. */
   IDirect3DDevice9_GetRenderTarget(chain->dev, 0, (LPDIRECT3DSURFACE9*)&back_buffer);

   /* In-between render target passes. */
   for (i = 0; i < chain->passes->count - 1; i++)
   {
      D3DVIEWPORT9   viewport = {0};
      struct shader_pass *from_pass  = (struct shader_pass*)&chain->passes->data[i];
      struct shader_pass *to_pass    = (struct shader_pass*)&chain->passes->data[i + 1];

      IDirect3DTexture9_GetSurfaceLevel(
		      (LPDIRECT3DTEXTURE9)to_pass->tex, 0, (IDirect3DSurface9**)&target);
      IDirect3DDevice9_SetRenderTarget(chain->dev, 0, target);

      switch (from_pass->info.pass->fbo.type_x)
      {
         case RARCH_SCALE_VIEWPORT:
            out_width = from_pass->info.pass->fbo.scale_x * chain->out_vp->Width;
            break;
         case RARCH_SCALE_ABSOLUTE:
            out_width = from_pass->info.pass->fbo.abs_x;
            break;
         case RARCH_SCALE_INPUT:
            out_width = from_pass->info.pass->fbo.scale_x * current_width;
            break;
      }
      switch (from_pass->info.pass->fbo.type_y)
      {
         case RARCH_SCALE_VIEWPORT:
            out_height = from_pass->info.pass->fbo.scale_y * chain->out_vp->Height;
            break;
         case RARCH_SCALE_ABSOLUTE:
            out_height = from_pass->info.pass->fbo.abs_y;
            break;
         case RARCH_SCALE_INPUT:
            out_height = from_pass->info.pass->fbo.scale_y * current_height;
            break;
      }

      /* Clear out whole FBO. */
      viewport.Width  = to_pass->info.tex_w;
      viewport.Height = to_pass->info.tex_h;
      viewport.MinZ   = 0.0f;
      viewport.MaxZ   = 1.0f;

      IDirect3DDevice9_SetViewport(chain->dev, (D3DVIEWPORT9*)&viewport);
      IDirect3DDevice9_Clear(chain->dev, 0, 0, D3DCLEAR_TARGET,
            0, 1, 0);

      viewport.Width  = out_width;
      viewport.Height = out_height;

      d3d9_cg_renderchain_render_pass(chain, from_pass,
            i + 1,
            current_width, current_height,
            out_width, out_height,
            out_width, out_height, 0,
            &d3d->shader, &viewport);

      current_width  = out_width;
      current_height = out_height;
      IDirect3DSurface9_Release(target);
   }

   /* Final pass */
   IDirect3DDevice9_SetRenderTarget(chain->dev, 0, back_buffer);

   last_pass = (struct shader_pass*)&chain->passes->
      data[chain->passes->count - 1];

   switch (last_pass->info.pass->fbo.type_x)
   {
      case RARCH_SCALE_VIEWPORT:
         out_width = last_pass->info.pass->fbo.scale_x * chain->out_vp->Width;
         break;
      case RARCH_SCALE_ABSOLUTE:
         out_width = last_pass->info.pass->fbo.abs_x;
         break;
      case RARCH_SCALE_INPUT:
         out_width = last_pass->info.pass->fbo.scale_x * current_width;
         break;
   }
   switch (last_pass->info.pass->fbo.type_y)
   {
      case RARCH_SCALE_VIEWPORT:
         out_height = last_pass->info.pass->fbo.scale_y * chain->out_vp->Height;
         break;
      case RARCH_SCALE_ABSOLUTE:
         out_height = last_pass->info.pass->fbo.abs_y;
         break;
      case RARCH_SCALE_INPUT:
         out_height = last_pass->info.pass->fbo.scale_y * current_height;
         break;
   }

   d3d9_cg_renderchain_render_pass(chain, last_pass,
         chain->passes->count,
         current_width, current_height,
         out_width, out_height,
         chain->out_vp->Width,
         chain->out_vp->Height,
         rotation,
         &d3d->shader, chain->out_vp);

   chain->frame_count++;

   if (back_buffer)
      IDirect3DSurface9_Release(back_buffer);

   /* End render: save pass 0 dimensions and advance prev pointer */
   chain->prev.last_width[chain->prev.ptr]  = chain->passes->data[0].last_width;
   chain->prev.last_height[chain->prev.ptr] = chain->passes->data[0].last_height;
   chain->prev.ptr                          = (chain->prev.ptr + 1) & TEXTURESMASK;

   cgD3D9BindProgram((CGprogram)_chain->stock_shader.fprg);
   cgD3D9BindProgram((CGprogram)_chain->stock_shader.vprg);
   CG_DEBUG_CHECK(_chain->cgCtx, "renderchain_render post-render BindProgram");

   d3d9_cg_renderchain_calc_and_set_shader_mvp(
         (CGprogram)_chain->stock_shader.vprg,
         chain->out_vp->Width,
         chain->out_vp->Height, 0);
}

static uint32_t d3d9_cg_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_CG);

   return flags;
}

static void d3d9_cg_deinit_chain(d3d9_video_t *d3d)
{
   if (!d3d)
      return;

   d3d9_cg_renderchain_free(d3d->renderchain_data);

   d3d->renderchain_data   = NULL;
}

static void d3d9_cg_deinitialize(d3d9_video_t *d3d)
{
   if (!d3d)
      return;

   font_driver_free_osd();

   d3d9_cg_deinit_chain(d3d);
   d3d9_vertex_buffer_free(d3d->menu_display.buffer,
         d3d->menu_display.decl);

   d3d->menu_display.buffer = NULL;
   d3d->menu_display.decl   = NULL;

   if (d3d9_cg_menu_pipeline_vbo)
   {
      IDirect3DVertexBuffer9_Release(d3d9_cg_menu_pipeline_vbo);
      d3d9_cg_menu_pipeline_vbo = NULL;
   }

   if (d3d9_cg_white_texture)
   {
      IDirect3DTexture9_Release(d3d9_cg_white_texture);
      d3d9_cg_white_texture = NULL;
   }
}

static bool d3d9_cg_init_base(d3d9_video_t *d3d, const video_info_t *info)
{
   D3DPRESENT_PARAMETERS d3dpp;
   HWND focus_window  = win32_get_window();

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

static void d3d9_cg_log_info(const struct LinkInfo *info)
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

static bool d3d9_cg_init_multipass(d3d9_video_t *d3d, const char *shader_path)
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

static bool d3d9_cg_process_shader(d3d9_video_t *d3d)
{
   struct video_shader_pass *pass   = NULL;
   const char *shader_path = d3d->shader_path;
   if (shader_path && *shader_path)
   {
      if (!d3d9_cg_init_multipass(d3d, shader_path))
      {
         RARCH_ERR("[D3D9] Failed to parse shader preset.\n");
         return false;
      }
      return true;
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

static bool d3d9_cg_init_chain(d3d9_video_t *d3d,
      unsigned input_scale,
      bool rgb32)
{
   unsigned i = 0;
   struct LinkInfo link_info;
   unsigned current_width, current_height, out_width = 0, out_height = 0;
   cg_renderchain_t *renderchain;

   /* Setup information for first pass. */
   link_info.tex_w = input_scale * RARCH_SCALE_BASE;
   link_info.tex_h = input_scale * RARCH_SCALE_BASE;
   link_info.pass  = &d3d->shader.pass[0];

   renderchain = (cg_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
   {
      RARCH_ERR("[D3D9 Cg] Renderchain could not be initialized.\n");
      return false;
   }
   renderchain->chain.passes     = shader_pass_vector_list_new();
   renderchain->chain.luts       = lut_info_vector_list_new();
   renderchain->chain.bound_tex  = unsigned_vector_list_new();
   renderchain->chain.bound_vert = unsigned_vector_list_new();
   d3d->renderchain_data         = renderchain;

   if (
         !d3d9_cg_renderchain_init(
            d3d,
            d3d->dev, &d3d->out_vp, &link_info,
            rgb32)
      )
   {
      RARCH_ERR("[D3D9 Cg] Failed to init render chain.\n");
      return false;
   }

   d3d9_cg_log_info(&link_info);

   current_width  = link_info.tex_w;
   current_height = link_info.tex_h;

   for (i = 1; i < d3d->shader.passes; i++)
   {
      switch (link_info.pass->fbo.type_x)
      {
         case RARCH_SCALE_VIEWPORT:
            out_width = link_info.pass->fbo.scale_x * d3d->out_vp.Width;
            break;
         case RARCH_SCALE_ABSOLUTE:
            out_width = link_info.pass->fbo.abs_x;
            break;
         case RARCH_SCALE_INPUT:
            out_width = link_info.pass->fbo.scale_x * current_width;
            break;
      }
      switch (link_info.pass->fbo.type_y)
      {
         case RARCH_SCALE_VIEWPORT:
            out_height = link_info.pass->fbo.scale_y * d3d->out_vp.Height;
            break;
         case RARCH_SCALE_ABSOLUTE:
            out_height = link_info.pass->fbo.abs_y;
            break;
         case RARCH_SCALE_INPUT:
            out_height = link_info.pass->fbo.scale_y * current_height;
            break;
      }

      link_info.pass  = &d3d->shader.pass[i];
      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      current_width   = out_width;
      current_height  = out_height;

      if (!d3d9_cg_renderchain_add_pass(
               d3d->renderchain_data, &link_info))
      {
         RARCH_ERR("[D3D9 Cg] Failed to add pass.\n");
         return false;
      }
      d3d9_cg_log_info(&link_info);
   }

   {
      unsigned i;
      settings_t *settings = config_get_ptr();
      bool video_smooth    = settings->bools.video_smooth;

      for (i = 0; i < d3d->shader.luts; i++)
      {
         if (!d3d9_cg_renderchain_add_lut(
                  (d3d9_cg_renderchain_t*)d3d->renderchain_data,
                  d3d->shader.lut[i].id, d3d->shader.lut[i].path,
                  d3d->shader.lut[i].filter == RARCH_FILTER_UNSPEC
                  ? video_smooth
                  : (d3d->shader.lut[i].filter == RARCH_FILTER_LINEAR)))
         {
            RARCH_ERR("[D3D9 Cg] Failed to init LUTs.\n");
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

static void d3d9_cg_set_osd_msg(void *data,
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

static void d3d9_cg_set_viewport(void *data,
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

static bool d3d9_cg_initialize(d3d9_video_t *d3d, const video_info_t *info)
{
   unsigned width, height;
   bool ret             = true;
   settings_t *settings = config_get_ptr();

   if (!g_pD3D9)
      ret = d3d9_cg_init_base(d3d, info);
   else if (d3d->needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;
      d3d9_make_d3dpp(d3d, info, &d3dpp);
      if (!d3d9_reset(d3d->dev, &d3dpp))
      {
         d3d9_cg_deinitialize(d3d);
         IDirect3D9_Release(g_pD3D9);
         g_pD3D9 = NULL;

         ret = d3d9_cg_init_base(d3d, info);
         if (ret)
            RARCH_LOG("[D3D9 Cg] Recovered from dead state.\n");
      }

#ifdef HAVE_MENU
      menu_driver_init(info->is_threaded);
#endif
   }

   if (!ret)
      return ret;

   if (!d3d9_cg_init_chain(d3d, info->input_scale, info->rgb32))
   {
      RARCH_ERR("[D3D9 Cg] Failed to initialize render chain.\n");
      return false;
   }

   video_driver_get_size(&width, &height);
   d3d9_cg_set_viewport(d3d,
      width, height, false, true);

   font_driver_init_osd(d3d, info,
         false,
         info->is_threaded,
         FONT_DRIVER_RENDER_D3D9_CG_API);

   {
      static const D3DVERTEXELEMENT9 VertexElements[4] = {
         {0, offsetof(Vertex, x),  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_POSITION, 0},
         {0, offsetof(Vertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_TEXCOORD, 0},
         {0, offsetof(Vertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,
            D3DDECLUSAGE_COLOR, 0},
         D3DDECL_END()
      };
      if (!SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(d3d->dev,
                  (const D3DVERTEXELEMENT9*)VertexElements,
                  (IDirect3DVertexDeclaration9**)&d3d->menu_display.decl)))
         return false;
   }

   d3d->menu_display.offset = 0;
   d3d->menu_display.size   = 1024;
   {
      void *_vbuf = NULL;
      if (SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
                  d3d->dev, d3d->menu_display.size * sizeof(Vertex),
                  D3DUSAGE_WRITEONLY,
                  0, /* FVF=0: using vertex declarations, not FVF */
                  D3DPOOL_DEFAULT,
                  (LPDIRECT3DVERTEXBUFFER9*)&_vbuf, NULL)))
         d3d->menu_display.buffer = _vbuf;
      else
         d3d->menu_display.buffer = NULL;
   }

   if (!d3d->menu_display.buffer)
      return false;

   /* Create a 1x1 opaque-white fallback texture for untextured draws
    * (solid-color rectangles, dividers, etc.).  Without this, draw
    * calls that have no texture would sample from whatever texture
    * happened to be bound last, producing visual artefacts. */
   if (!d3d9_cg_white_texture)
   {
      void *_tbuf = NULL;
      if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d3d->dev,
                  1, 1, 1, 0,
                  D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                  (struct IDirect3DTexture9**)&_tbuf, NULL)))
         d3d9_cg_white_texture = (LPDIRECT3DTEXTURE9)_tbuf;

      if (d3d9_cg_white_texture)
      {
         D3DLOCKED_RECT lr;
         if (SUCCEEDED(IDirect3DTexture9_LockRect(
                     d3d9_cg_white_texture, 0, &lr, NULL, 0)))
         {
            *((uint32_t*)lr.pBits) = 0xFFFFFFFF;
            IDirect3DTexture9_UnlockRect(d3d9_cg_white_texture, 0);
         }
      }
   }

   /* Pre-computed D3D left-handed orthographic projection (0,1,0,1,0,1) */
   {
      static const math_matrix_4x4 k_ortho = {{
         2, 0, 0, 0,   0, 2, 0, 0,   0, 0, 1, 0,   -1, -1, 0, 1
      }};
      d3d->mvp_transposed = k_ortho;
      d3d->mvp            = k_ortho;
   }

   if (d3d->translate_x)
   {
      float vp_x = -(d3d->translate_x/(float)d3d->out_vp.Width);
      MAT_ELEM_4X4(d3d->mvp, 0, 3) = -1.0f + vp_x - 2.0f * 1 / (0 - 1);
   }

   if (d3d->translate_y)
   {
      float vp_y = -(d3d->translate_y/(float)d3d->out_vp.Height);
      MAT_ELEM_4X4(d3d->mvp, 1, 3) = 1.0f + vp_y + 2.0f * 1 / (0 - 1);
   }

   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SCISSORTESTENABLE, TRUE);

   return true;
}

static bool d3d9_cg_restore(d3d9_video_t *d3d)
{
   d3d9_cg_deinitialize(d3d);

   if (!d3d9_cg_initialize(d3d, &d3d->video_info))
   {
      RARCH_ERR("[D3D9 Cg] Restore error.\n");
      return false;
   }

   d3d->needs_restore = false;

   return true;
}

static bool d3d9_cg_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return false;

   if (d3d->shader_path && *d3d->shader_path)
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
         RARCH_WARN("[D3D9] Only Cg shaders are supported. Falling back to stock.\n");
   }

   if (!d3d9_cg_process_shader(d3d) || !d3d9_cg_restore(d3d))
   {
      RARCH_ERR("[D3D9 Cg] Failed to set shader.\n");
      return false;
   }

   return true;
}

static bool d3d9_cg_init_internal(d3d9_video_t *d3d,
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
      ? (unsigned)(mon_rect.right  - mon_rect.left) : info->width;
   full_y                = (windowed_full || info->height == 0)
      ? (unsigned)(mon_rect.bottom - mon_rect.top)  : info->height;
#else
   {
      d3d9_get_video_size(d3d, &full_x, &full_y);
   }
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

   if (!d3d9_cg_initialize(d3d, &d3d->video_info))
      return false;

   d3d9_cg_fake_context.get_flags   = d3d9_cg_get_flags;
   d3d9_cg_fake_context.get_metrics = win32_get_metrics;
   video_context_driver_set(&d3d9_cg_fake_context);
   {
      const char *shader_preset   = video_shader_get_current_shader_preset();
      enum rarch_shader_type type = video_shader_parse_type(shader_preset);

      d3d9_cg_set_shader(d3d, type, shader_preset);
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
      RARCH_LOG("[D3D9 Cg] Using GPU: \"%s\".\n", ident.Description);
      RARCH_LOG("[D3D9 Cg] GPU API Version: %s.\n", version_str);
      video_driver_set_gpu_api_version_string(version_str);
   }

   return true;
}

static void *d3d9_cg_init(const video_info_t *info,
      input_driver_t **input, void **input_data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)calloc(1, sizeof(*d3d));

   if (!d3d)
      return NULL;

   if (!d3d9_initialize_symbols(GFX_CTX_DIRECT3D9_API))
   {
      free(d3d);
      return NULL;
   }

   win32_window_reset();
   win32_monitor_init();

   /* Default values */
   d3d->dev                  = NULL;
   d3d->dev_rotation         = 0;
   d3d->needs_restore        = false;
#ifdef HAVE_OVERLAY
   d3d->overlays_enabled     = false;
#endif
   d3d->should_resize        = false;
   d3d->menu                 = NULL;

   if (!d3d9_cg_init_internal(d3d, info, input, input_data))
   {
      RARCH_ERR("[D3D9 Cg] Failed to init D3D.\n");
      free(d3d);
      return NULL;
   }

   d3d->keep_aspect       = info->force_aspect;

   return d3d;
}

#ifdef HAVE_OVERLAY
static void d3d9_cg_free_overlays(d3d9_video_t *d3d)
{
   unsigned i;

   for (i = 0; i < d3d->overlays_size; i++)
   {
      if ((LPDIRECT3DTEXTURE9)d3d->overlays[i].tex)
         IDirect3DTexture9_Release((LPDIRECT3DTEXTURE9)d3d->overlays[i].tex);
      d3d9_vertex_buffer_free(d3d->overlays[i].vert_buf, NULL);
   }
   free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
}

static void d3d9_cg_overlay_tex_geom(
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

static void d3d9_cg_overlay_vertex_geom(
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

static bool d3d9_cg_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, y;
   d3d9_video_t *d3d                  = (d3d9_video_t*)data;
   const struct texture_image *images = (const struct texture_image*)
      image_data;

   if (!d3d)
      return false;

   d3d9_cg_free_overlays(d3d);
   d3d->overlays      = (overlay_t*)calloc(num_images, sizeof(*d3d->overlays));
   d3d->overlays_size = num_images;

   for (i = 0; i < num_images; i++)
   {
      D3DLOCKED_RECT d3dlr;
      unsigned width     = images[i].width;
      unsigned height    = images[i].height;
      overlay_t *overlay = (overlay_t*)&d3d->overlays[i];

      overlay->tex       = NULL;
      {
         void *_tbuf = NULL;
         if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d3d->dev,
                     width, height, 1, 0,
                     D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                     (struct IDirect3DTexture9**)&_tbuf, NULL)))
            overlay->tex = (LPDIRECT3DTEXTURE9)_tbuf;
      }

      if (!overlay->tex)
      {
         RARCH_ERR("[D3D9] Failed to create overlay texture.\n");
         return false;
      }

      if (SUCCEEDED(IDirect3DTexture9_LockRect((LPDIRECT3DTEXTURE9)overlay->tex, 0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
      {
         uint32_t       *dst = (uint32_t*)(d3dlr.pBits);
         const uint32_t *src = images[i].pixels;
         unsigned      pitch = d3dlr.Pitch >> 2;
         for (y = 0; y < height; y++, dst += pitch, src += width)
            memcpy(dst, src, width << 2);
         IDirect3DTexture9_UnlockRect((LPDIRECT3DTEXTURE9)overlay->tex, 0);
      }

      overlay->tex_w         = width;
      overlay->tex_h         = height;

      /* Default. Stretch to whole screen. */
      d3d9_cg_overlay_tex_geom(d3d, i, 0, 0, 1, 1);
      d3d9_cg_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d9_cg_overlay_enable(void *data, bool state)
{
   unsigned i;
   d3d9_video_t            *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays_enabled = state;

   win32_show_cursor(d3d, state);
}

static void d3d9_cg_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d9_cg_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (d3d)
      d3d->overlays[index].alpha_mod = mod;
}

static const video_overlay_interface_t d3d9_cg_overlay_interface = {
   d3d9_cg_overlay_enable,
   d3d9_cg_overlay_load,
   d3d9_cg_overlay_tex_geom,
   d3d9_cg_overlay_vertex_geom,
   d3d9_cg_overlay_full_screen,
   d3d9_cg_overlay_set_alpha,
};

void d3d9_cg_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   *iface = &d3d9_cg_overlay_interface;
}
#endif

/* Release the texture owned by a single overlay. */
static void d3d9_cg_free_overlay(overlay_t *overlay)
{
   if (!overlay)
      return;
   if (overlay->tex)
   {
      IDirect3DTexture9_Release((LPDIRECT3DTEXTURE9)overlay->tex);
      overlay->tex = NULL;
   }
}

static void d3d9_cg_free(void *data)
{
   d3d9_video_t   *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_OVERLAY
   d3d9_cg_free_overlays(d3d);
   if (d3d->overlays)
      free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
#endif

   d3d9_cg_free_overlay(d3d->menu);
   if (d3d->menu)
      free(d3d->menu);
   d3d->menu          = NULL;

   d3d9_cg_deinitialize(d3d);

   if (d3d->shader_path && *d3d->shader_path)
      free(d3d->shader_path);

   IDirect3DDevice9_Release(d3d->dev);
   IDirect3D9_Release(g_pD3D9);
   d3d->shader_path = NULL;
   d3d->dev         = NULL;
   g_pD3D9          = NULL;

   d3d9_deinitialize_symbols();

   win32_monitor_from_window();
   win32_destroy_window();
   free(d3d);
}

/* Standalone overlay render for the D3D9 Cg driver.
 *
 * The common d3d9_cg_overlay_render() sets its MVP via
 * SetVertexShaderConstantF which does not work with Cg shaders
 * (the Cg runtime manages its own constant register mapping).
 * This reimplementation uses the Cg API to set the MVP correctly
 * and avoids any dependency on the common overlay renderer. */
static void d3d9_cg_overlay_render(
      d3d9_video_t *d3d,
      unsigned video_width,
      unsigned video_height,
      overlay_t *overlay,
      bool force_linear)
{
   Vertex quad[4];
   LPDIRECT3DDEVICE9 dev;
   float x, y, w, h;
   D3DCOLOR color;

   if (!d3d || !overlay || !overlay->tex)
      return;

   dev   = d3d->dev;
   x     = overlay->vert_coords[0];
   y     = overlay->vert_coords[1];
   w     = overlay->vert_coords[2];
   h     = overlay->vert_coords[3];
   color = D3DCOLOR_ARGB(
         (int)(overlay->alpha_mod * 0xFF),
         0xFF, 0xFF, 0xFF);

   /* Set the stock Cg program and the MVP via the Cg API.
    * Use the same bottom-up [0,1] ortho as the menu display draw. */
   {
      static const float bottomup_ortho[16] = {
          2.0f,  0.0f, 0.0f, -1.0f,
          0.0f,  2.0f, 0.0f, -1.0f,
          0.0f,  0.0f, 1.0f,  0.0f,
          0.0f,  0.0f, 0.0f,  1.0f
      };
      cg_renderchain_t *_chain = (cg_renderchain_t*)d3d->renderchain_data;
      {
         CGparameter cgp = cgGetNamedParameter(
               (CGprogram)_chain->stock_shader.vprg, "modelViewProj");
         if (cgp)
            cgD3D9SetUniformMatrix(cgp, (const D3DMATRIX*)bottomup_ortho);
      }
      cgD3D9BindProgram((CGprogram)_chain->stock_shader.fprg);
      cgD3D9BindProgram((CGprogram)_chain->stock_shader.vprg);
      CG_DEBUG_CHECK(_chain->cgCtx, "overlay_render BindProgram");
   }

   /* Build a quad from the overlay's normalised coordinates.
    * vert_coords: [0]=x  [1]=y  [2]=w  [3]=h
    * tex_coords:  [0]=u  [1]=v  [2]=tw [3]=th */
   quad[0].x = x;
   quad[0].y = y;
   quad[0].z = 0.5f;
   quad[0].u = overlay->tex_coords[0];
   quad[0].v = overlay->tex_coords[1];
   quad[0].color = color;

   quad[1].x = x + w;
   quad[1].y = y;
   quad[1].z = 0.5f;
   quad[1].u = overlay->tex_coords[0] + overlay->tex_coords[2];
   quad[1].v = overlay->tex_coords[1];
   quad[1].color = color;

   quad[2].x = x;
   quad[2].y = y + h;
   quad[2].z = 0.5f;
   quad[2].u = overlay->tex_coords[0];
   quad[2].v = overlay->tex_coords[1] + overlay->tex_coords[3];
   quad[2].color = color;

   quad[3].x = x + w;
   quad[3].y = y + h;
   quad[3].z = 0.5f;
   quad[3].u = overlay->tex_coords[0] + overlay->tex_coords[2];
   quad[3].v = overlay->tex_coords[1] + overlay->tex_coords[3];
   quad[3].color = color;

   /* Render state: alpha blending on, texture filtering */
   IDirect3DDevice9_SetRenderState(dev,
         D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   IDirect3DDevice9_SetRenderState(dev,
         D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice9_SetRenderState(dev,
         D3DRS_ALPHABLENDENABLE, TRUE);

   IDirect3DDevice9_SetTexture(dev, 0,
         (IDirect3DBaseTexture9*)overlay->tex);
   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_MINFILTER, force_linear
         ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   IDirect3DDevice9_SetSamplerState(dev,
         0, D3DSAMP_MAGFILTER, force_linear
         ? D3DTEXF_LINEAR : D3DTEXF_POINT);

   IDirect3DDevice9_SetVertexDeclaration(dev,
         (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);

   if (overlay->fullscreen)
   {
      D3DVIEWPORT9 vp_full;
      vp_full.X      = 0;
      vp_full.Y      = 0;
      vp_full.Width  = video_width;
      vp_full.Height = video_height;
      vp_full.MinZ   = 0.0f;
      vp_full.MaxZ   = 1.0f;
      IDirect3DDevice9_SetViewport(dev, (D3DVIEWPORT9*)&vp_full);
   }

   IDirect3DDevice9_BeginScene(dev);
   IDirect3DDevice9_DrawPrimitiveUP(dev,
         D3DPT_TRIANGLESTRIP, 2,
         quad, sizeof(Vertex));
   IDirect3DDevice9_EndScene(dev);

   /* DrawPrimitiveUP unbinds the stream source, re-bind it */
   IDirect3DDevice9_SetStreamSource(dev, 0,
         (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
         0, sizeof(Vertex));
}

static bool d3d9_cg_frame(void *data, const void *frame,
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
      HWND window = win32_get_window();
      if (IsIconic(window))
         return true;

      if (!d3d9_cg_restore(d3d))
      {
         RARCH_ERR("[D3D9 Cg] Failed to restore.\n");
         return false;
      }
   }

   if (d3d->should_resize)
   {
      cg_renderchain_t   *_chain = (cg_renderchain_t*)
         d3d->renderchain_data;
      d3d9_cg_renderchain_t *chain  = (d3d9_cg_renderchain_t*)&_chain->chain;
      d3d9_cg_set_viewport(d3d, width, height, false, true);

      if (chain)
         chain->out_vp = (D3DVIEWPORT9*)&d3d->out_vp;

      d3d9_recompute_pass_sizes(chain->dev, chain, d3d);

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
   IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
   IDirect3DDevice9_Clear(d3d->dev, 0, 0, D3DCLEAR_TARGET,
         0, 1, 0);

   {
      cg_renderchain_t *_chain = (cg_renderchain_t*)d3d->renderchain_data;
      CGparameter cgp = cgGetNamedParameter(
            (CGprogram)_chain->stock_shader.vprg, "modelViewProj");
      if (cgp)
      {
         math_matrix_4x4 transposed;
         matrix_4x4_transpose(transposed, d3d->mvp);
         cgD3D9SetUniformMatrix(cgp, (const D3DMATRIX*)&transposed);
         CG_DEBUG_CHECK(_chain->cgCtx, "set_mvp_transpose SetUniformMatrix");
      }
   }
   d3d9_cg_renderchain_render(
            d3d, frame, frame_width, frame_height,
            pitch, d3d->dev_rotation);

   if (black_frame_insertion && !d3d->menu->enabled)
   {
      int n;
      for (n = 0; n < (int)video_info->black_frame_insertion; ++n)
      {
        bool ret = (IDirect3DDevice9_Present(d3d->dev,
                 NULL, NULL, NULL, NULL) != D3DERR_DEVICELOST);
        if (!ret || d3d->needs_restore)
          return true;
        IDirect3DDevice9_Clear(d3d->dev, 0, 0, D3DCLEAR_TARGET,
              0, 1, 0);
      }
   }

#ifdef HAVE_OVERLAY
   if (d3d->overlays_enabled && overlay_behind_menu)
   {
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_cg_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      d3d9_cg_overlay_render(d3d, width, height, d3d->menu, false);

      d3d->menu_display.offset = 0;
      IDirect3DDevice9_SetVertexDeclaration(d3d->dev, (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);
      IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
            (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
            0, sizeof(Vertex));
      IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
      {
         cg_renderchain_t *_chain = (cg_renderchain_t*)d3d->renderchain_data;
         if (_chain)
         {
            cgD3D9BindProgram((CGprogram)_chain->stock_shader.fprg);
            cgD3D9BindProgram((CGprogram)_chain->stock_shader.vprg);
            CG_DEBUG_CHECK(_chain->cgCtx, "frame menu BindProgram");
         }
      }
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
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_cg_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
   {
      cg_renderchain_t *_chain = (cg_renderchain_t*)d3d->renderchain_data;
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
            D3DRS_ALPHABLENDENABLE, TRUE);
      IDirect3DDevice9_SetRenderState(d3d->dev,
            D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
      IDirect3DDevice9_SetRenderState(d3d->dev,
            D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
      IDirect3DDevice9_SetRenderState(d3d->dev,
            D3DRS_ZENABLE, FALSE);
      IDirect3DDevice9_SetRenderState(d3d->dev,
            D3DRS_CULLMODE, D3DCULL_NONE);
      if (_chain)
      {
         cgD3D9BindProgram((CGprogram)_chain->stock_shader.fprg);
         cgD3D9BindProgram((CGprogram)_chain->stock_shader.vprg);
         CG_DEBUG_CHECK(_chain->cgCtx, "widgets BindProgram");
      }
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

static void d3d9_cg_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   d3d->keep_aspect   = true;
   d3d->should_resize = true;
}

static void d3d9_cg_apply_state_changes(void *data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (d3d)
      d3d->should_resize = true;
}

void d3d9_cg_set_menu_texture_enable(void *data,
      bool state, bool full_screen)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   if (!d3d || !d3d->menu)
      return;

   d3d->menu->enabled            = state;
   d3d->menu->fullscreen         = full_screen;
}

static void d3d9_cg_set_menu_texture_frame(void *data,
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

      d3d->menu->tex = NULL;
      {
         void *_tbuf = NULL;
         if (SUCCEEDED(IDirect3DDevice9_CreateTexture(d3d->dev,
                     width, height, 1, 0,
                     D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                     (struct IDirect3DTexture9**)&_tbuf, NULL)))
            d3d->menu->tex = (LPDIRECT3DTEXTURE9)_tbuf;
      }

      if (!d3d->menu->tex)
      {
         RARCH_ERR("[D3D9] Failed to create menu texture.\n");
         return;
      }

      d3d->menu->tex_w          = width;
      d3d->menu->tex_h          = height;
   }

   d3d->menu->alpha_mod = alpha;

   if (!SUCCEEDED(IDirect3DTexture9_LockRect((LPDIRECT3DTEXTURE9)d3d->menu->tex,
         0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
      return;
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

static void d3d9_cg_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   /* TODO/FIXME - why is this called here? */
   win32_show_cursor(data, !fullscreen);
}

static struct video_shader *d3d9_cg_get_current_shader(void *data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (!d3d)
      return NULL;
   return &d3d->shader;
}

static const video_poke_interface_t d3d9_cg_poke_interface = {
   d3d9_cg_get_flags,
   d3d9_cg_load_texture,
   d3d9_cg_unload_texture,
   d3d9_cg_set_video_mode,
#if defined(__WINRT__)
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
   d3d9_cg_set_aspect_ratio,
   d3d9_cg_apply_state_changes,
   d3d9_cg_set_menu_texture_frame,
   d3d9_cg_set_menu_texture_enable,
   d3d9_cg_set_osd_msg,
   win32_show_cursor,
   NULL, /* grab_mouse_toggle */
   d3d9_cg_get_current_shader,
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL  /* set_hdr_subpixel_layout */
};

static void d3d9_cg_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &d3d9_cg_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool d3d9_cg_gfx_widgets_enabled(void *data)
{
   return true;
}
#endif

static void d3d9_cg_set_resize(d3d9_video_t *d3d,
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

static bool d3d9_cg_alive(void *data)
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
      d3d9_cg_set_resize(d3d, temp_width, temp_height);
      d3d9_cg_restore(d3d);
   }

   ret = !quit;

   if (  temp_width  != 0 &&
         temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   return ret;
}

static void d3d9_cg_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   d3d9_video_t     *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

   d3d->video_info.vsync = !state;
   d3d->needs_restore    = true;
   d3d9_cg_restore(d3d);
}

void d3d9_cg_set_rotation(void *data, unsigned rot)
{
   d3d9_video_t        *d3d = (d3d9_video_t*)data;
   if (d3d)
      d3d->dev_rotation = rot;
}

void d3d9_cg_viewport_info(void *data, struct video_viewport *vp)
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

static INLINE bool d3d9_cg_device_get_render_target_data(
      LPDIRECT3DDEVICE9 dev,
      LPDIRECT3DSURFACE9 src, LPDIRECT3DSURFACE9 dst)
{
   return (   dev
           && SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(
               dev, src, dst)));
}

bool d3d9_cg_read_viewport(void *data, uint8_t *buffer, bool is_idle)
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
            !SUCCEEDED(IDirect3DDevice9_GetRenderTarget(d3dr, 0, (LPDIRECT3DSURFACE9*)&target))
         || !SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(d3dr,
            width, height,
            D3DFMT_X8R8G8B8,
            (D3DPOOL)D3DPOOL_SYSTEMMEM,
            (LPDIRECT3DSURFACE9*)&dest, NULL))
         || !d3d9_cg_device_get_render_target_data(d3dr, target, dest)
         )
   {
      ret = false;
      goto end;
   }

   if (!SUCCEEDED(IDirect3DSurface9_LockRect(dest, &rect, NULL, D3DLOCK_READONLY)))
   {
      ret = false;
      goto end;
   }

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
            uint32_t px = pixels[x];
            *buffer++ = (px >>  0) & 0xff;
            *buffer++ = (px >>  8) & 0xff;
            *buffer++ = (px >> 16) & 0xff;
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

static bool d3d9_cg_has_windowed(void *data)
{
   return true;
}

video_driver_t video_d3d9_cg = {
   d3d9_cg_init,
   d3d9_cg_frame,
   d3d9_cg_set_nonblock_state,
   d3d9_cg_alive,
   NULL, /* focus */
   win32_suspend_screensaver,
   d3d9_cg_has_windowed,
   d3d9_cg_set_shader,
   d3d9_cg_free,
   "d3d9_cg",
   d3d9_cg_set_viewport,
   d3d9_cg_set_rotation,
   d3d9_cg_viewport_info,
   d3d9_cg_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d9_cg_get_overlay_interface,
#endif
   d3d9_cg_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   d3d9_cg_gfx_widgets_enabled
#endif
};
