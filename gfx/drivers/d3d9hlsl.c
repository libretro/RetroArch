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
#include <streams/file_stream.h>
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

#include "../video_driver.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include <defines/d3d_defines.h>
#include <gfx/math/matrix_4x4.h>
#include "../common/d3d_common.h"

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

#include "d3d_shaders/shaders_common.h"

static const char *stock_hlsl_program = CG(
      void main_vertex
      (
         float3 position : POSITION,
         float4 color    : COLOR,
         float2 texCoord : TEXCOORD0,

         uniform float4x4 modelViewProj,

         out float4 oPosition : POSITION,
         out float4 oColor : COLOR,
         out float2 otexCoord : TEXCOORD
      )
      {
         oPosition = mul(modelViewProj, float4(position, 1.0f));
         oColor = color;
         otexCoord = texCoord;
      }

      struct output
      {
         float4 color: COLOR;
      };

      struct input
      {
         float2 video_size;
         float2 texture_size;
         float2 output_size;
         float frame_count;
         float frame_direction;
         float frame_rotation;
      };

      output main_fragment(float4 color : COLOR, float2 texCoord : TEXCOORD0,
      uniform sampler2D decal : TEXUNIT0, uniform input IN)
      {
         output OUT;
         OUT.color = color * tex2D(decal, texCoord);
         return OUT;
      }
);

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

typedef struct d3d9_hlsl_renderchain
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
} d3d9_hlsl_renderchain_t;

static INLINE bool d3d9_hlsl_renderchain_add_lut(d3d9_hlsl_renderchain_t *chain,
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

   lut = NULL;
   IDirect3DDevice9_CreateTexture(chain->dev,
         image.width, image.height, 1,
         0, D3D9_ARGB8888_FORMAT,
         D3DPOOL_MANAGED,
         (struct IDirect3DTexture9**)&lut, NULL);

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

static INLINE bool d3d9_hlsl_renderchain_set_pass_size(
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
      IDirect3DDevice9_CreateTexture(dev,
            width, height, 1,
            D3DUSAGE_RENDERTARGET,
            (pass2->info.pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
            ? D3DFMT_A32B32G32R32F
            : D3D9_ARGB8888_FORMAT,
            D3DPOOL_DEFAULT,
            (struct IDirect3DTexture9**)&pass->tex, NULL);

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
      d3d9_hlsl_renderchain_t *chain,
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

   if (!d3d9_hlsl_renderchain_set_pass_size(dev,
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

      if (!d3d9_hlsl_renderchain_set_pass_size(dev,
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

/* TODO/FIXME - Temporary workaround for D3D9 not being able to poll flags during init */
static gfx_ctx_driver_t d3d9_hlsl_fake_context;

/* =====================================================================
 * SM3.0 CTAB (Constant Table) parser
 *
 * Parses the CTAB embedded in compiled SM3.0 shader bytecode to find
 * hardware register indices by uniform name.
 * ===================================================================== */

#define CTAB_REGSET_FLOAT4  2
#define CTAB_REGSET_SAMPLER 3
#define CTAB_NOT_FOUND      ((int)-1)

static int d3d9_hlsl_ctab_find_register(
      const DWORD *bytecode, size_t bytecode_dwords,
      const char *name, int *out_regset, int *out_count)
{
   size_t pos;
   const DWORD CTAB_TAG = 0x42415443; /* 'CTAB' */

   if (out_regset)
      *out_regset = -1;
   if (out_count)
      *out_count = 0;
   if (!bytecode || bytecode_dwords < 2 || !name || !*name)
      return CTAB_NOT_FOUND;

   for (pos = 1; pos < bytecode_dwords; )
   {
      DWORD token  = bytecode[pos];
      DWORD opcode = token & 0xFFFF;

      if (opcode == 0xFFFF)
         break;

      if (opcode == 0xFFFE)
      {
         DWORD comment_dwords = (token >> 16) & 0x7FFF;
         size_t comment_bytes;

         if (pos + 1 + comment_dwords > bytecode_dwords)
            break;

         comment_bytes = comment_dwords * sizeof(DWORD);

         if (comment_dwords >= 6 && bytecode[pos + 1] == CTAB_TAG)
         {
            const uint8_t *ctab_start = (const uint8_t*)&bytecode[pos + 1];
            size_t ctab_total_bytes   = comment_bytes;
            const uint8_t *hdr        = ctab_start + 4;
            size_t hdr_avail          = ctab_total_bytes - 4;
            DWORD num_constants;
            DWORD const_info_offset;
            DWORD ci;

            if (hdr_avail < 28)
               return CTAB_NOT_FOUND;

            num_constants     = *(const DWORD*)(hdr + 12);
            const_info_offset = *(const DWORD*)(hdr + 16);

            for (ci = 0; ci < num_constants; ci++)
            {
               size_t entry_off = const_info_offset + ci * 20;
               const uint8_t *entry;
               DWORD name_offset;
               WORD  reg_set, reg_index;
               const char *cname;

               if (entry_off + 20 > hdr_avail)
                  break;

               entry       = hdr + entry_off;
               name_offset = *(const DWORD*)(entry + 0);
               reg_set     = *(const WORD*)(entry + 4);
               reg_index   = *(const WORD*)(entry + 6);

               if (name_offset >= hdr_avail)
                  continue;

               cname = (const char*)(hdr + name_offset);

               /* D3DCompile with backwards compat may prefix names with '$' */
               if (string_is_equal(cname, name)
                     || (cname[0] == '$' && string_is_equal(cname + 1, name)))
               {
                  if (out_regset)
                     *out_regset = (int)reg_set;
                  if (out_count)
                     *out_count = (int)*(const WORD*)(entry + 8);
                  return (int)reg_index;
               }
            }
            return CTAB_NOT_FOUND;
         }

         pos += 1 + comment_dwords;
         continue;
      }

      pos++;
   }

   return CTAB_NOT_FOUND;
}


static INLINE void d3d9_hlsl_set_vs_const(LPDIRECT3DDEVICE9 dev,
      int reg, const float *data, unsigned float4_count)
{
   if (reg >= 0)
      IDirect3DDevice9_SetVertexShaderConstantF(dev, reg, data, float4_count);
}

static INLINE void d3d9_hlsl_set_ps_const(LPDIRECT3DDEVICE9 dev,
      int reg, const float *data, unsigned float4_count)
{
   if (reg >= 0)
      IDirect3DDevice9_SetPixelShaderConstantF(dev, reg, data, float4_count);
}

typedef struct hlsl_uniform_map
{
   int mvp;
   int video_size;
   int texture_size;
   int output_size;
   int frame_count;
   int orig_video_size;
   int orig_texture_size;
   int orig_texture;
   int prev_video_size[TEXTURES - 1];
   int prev_texture_size[TEXTURES - 1];
   int prev_texture[TEXTURES - 1];
   int pass_video_size[GFX_MAX_SHADERS];
   int pass_texture_size[GFX_MAX_SHADERS];
   int pass_texture[GFX_MAX_SHADERS];
} hlsl_uniform_map_t;

static void hlsl_uniform_map_init(hlsl_uniform_map_t *map)
{
   memset(map, 0xFF, sizeof(*map)); /* -1 = not found */
}

static void hlsl_uniform_map_from_bytecode(
      hlsl_uniform_map_t *map,
      const DWORD *bytecode, size_t bytecode_dwords)
{
   int i;
   char attr[64];
   static const char *prev_names[] = {
      "PREV", "PREV1", "PREV2", "PREV3", "PREV4", "PREV5", "PREV6"
   };

   hlsl_uniform_map_init(map);
   if (!bytecode)
      return;

   map->mvp          = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, "modelViewProj", NULL, NULL);
   map->video_size   = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, "IN.video_size", NULL, NULL);
   map->texture_size = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, "IN.texture_size", NULL, NULL);
   map->output_size  = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, "IN.output_size", NULL, NULL);
   map->frame_count  = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, "IN.frame_count", NULL, NULL);

   /* If individual IN.* lookups failed, check if the struct is
    * registered as a single "IN" entry. D3DCompile packs struct
    * members into consecutive float4 registers:
    *   c[N+0] = { video_size.x, video_size.y, texture_size.x, texture_size.y }
    *   c[N+1] = { output_size.x, output_size.y, frame_count, frame_direction }
    */
   if (map->video_size < 0 && map->texture_size < 0)
   {
      int in_cnt = 0;
      int in_reg = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, "IN", NULL, &in_cnt);
      if (in_reg >= 0)
      {
         if (in_cnt >= 3)
         {
            /* Per-member packing (cnt >= 3):
             * c[N+0] = { video_size.x, video_size.y, 0, 0 }
             * c[N+1] = { texture_size.x, texture_size.y, 0, 0 }
             * c[N+2] = { output_size.x, output_size.y, 0, 0 }
             * For cnt==4:
             * c[N+3] = { frame_count, frame_direction, 0, 0 }
             * For cnt==3, frame_count is packed into c[N+2].z */
            map->video_size   = in_reg;
            map->texture_size = in_reg + 1;
            map->output_size  = in_reg + 2;
            map->frame_count  = (in_cnt >= 4) ? in_reg + 3 : in_reg + 2;
         }
         else
         {
            /* Tight packing (cnt <= 2):
             * c[N+0] = { video_size.x, video_size.y, texture_size.x, texture_size.y }
             * c[N+1] = { output_size.x, output_size.y, frame_count, frame_direction } */
            map->video_size   = in_reg;
            map->texture_size = in_reg;
            map->output_size  = in_reg + 1;
            map->frame_count  = in_reg + 1;
         }
      }
   }
   map->orig_video_size   = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, "ORIG.video_size", NULL, NULL);
   map->orig_texture_size = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, "ORIG.texture_size", NULL, NULL);
   map->orig_texture      = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, "ORIG.texture", NULL, NULL);
   /* Fallback: decomposed sampler names (ORIG__texture instead of ORIG.texture) */
   if (map->orig_texture < 0)
      map->orig_texture   = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, "ORIG__texture", NULL, NULL);

   for (i = 0; i < TEXTURES - 1; i++)
   {
      size_t _len = strlcpy(attr, prev_names[i], sizeof(attr));
      strlcpy(attr + _len, ".video_size",   sizeof(attr) - _len);
      map->prev_video_size[i]   = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, attr, NULL, NULL);
      strlcpy(attr + _len, ".texture_size", sizeof(attr) - _len);
      map->prev_texture_size[i] = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, attr, NULL, NULL);
      strlcpy(attr + _len, ".texture",      sizeof(attr) - _len);
      map->prev_texture[i]      = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, attr, NULL, NULL);
      /* Fallback: decomposed sampler name */
      if (map->prev_texture[i] < 0)
      {
         strlcpy(attr + _len, "__texture",  sizeof(attr) - _len);
         map->prev_texture[i]   = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, attr, NULL, NULL);
      }
   }

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      size_t _len = snprintf(attr, sizeof(attr), "PASS%u", i + 1);
      strlcpy(attr + _len, ".video_size",   sizeof(attr) - _len);
      map->pass_video_size[i]   = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, attr, NULL, NULL);
      strlcpy(attr + _len, ".texture_size", sizeof(attr) - _len);
      map->pass_texture_size[i] = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, attr, NULL, NULL);
      strlcpy(attr + _len, ".texture",      sizeof(attr) - _len);
      map->pass_texture[i]      = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, attr, NULL, NULL);
      /* Fallback: decomposed sampler name */
      if (map->pass_texture[i] < 0)
      {
         strlcpy(attr + _len, "__texture",  sizeof(attr) - _len);
         map->pass_texture[i]   = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, attr, NULL, NULL);
      }
   }
}

typedef struct hlsl_pass_data
{
   DWORD *vs_bytecode;
   DWORD *ps_bytecode;
   size_t vs_bytecode_dwords;
   size_t ps_bytecode_dwords;
   hlsl_uniform_map_t vs_map;
   hlsl_uniform_map_t ps_map;
} hlsl_pass_data_t;

static void hlsl_pass_data_free(hlsl_pass_data_t *pd)
{
   if (pd->vs_bytecode) { free(pd->vs_bytecode); pd->vs_bytecode = NULL; }
   if (pd->ps_bytecode) { free(pd->ps_bytecode); pd->ps_bytecode = NULL; }
   pd->vs_bytecode_dwords = 0;
   pd->ps_bytecode_dwords = 0;
}

/* BEGIN HLSL RENDERCHAIN */

#define RARCH_HLSL_MAX_SHADERS 16

typedef struct hlsl_renderchain
{
   struct d3d9_hlsl_renderchain chain;
   struct shader_pass stock_shader;
   /* Per-pass HLSL uniform data for shader presets */
   hlsl_pass_data_t pass_data[GFX_MAX_SHADERS + 1];
   unsigned pass_data_count;
   hlsl_pass_data_t stock_data;
} hlsl_renderchain_t;

/* Pipeline vertex buffer for menu shader effects (VIDEO_SHADER_MENU, etc.).
 * Stored as a file-static since the d3d9 menu_display struct
 * does not have a pipeline_vbo member like d3d10_video_t does. */
static LPDIRECT3DVERTEXBUFFER9 d3d9_hlsl_menu_pipeline_vbo = NULL;
static LPDIRECT3DVERTEXDECLARATION9 d3d9_hlsl_overlay_decl = NULL;

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
static INLINE void d3d9_hlsl_set_param_1f(
      const DWORD *bytecode, size_t bytecode_dwords,
      bool is_vertex,
      LPDIRECT3DDEVICE9 dev, const char *name, const void *value);

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

static INT32 gfx_display_prim_to_d3d9_hlsl_enum(
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

            IDirect3DDevice9_DrawPrimitive(dev, D3DPT_TRIANGLESTRIP,
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
            d3d->menu_display.offset == 0
            ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE);
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
               _chain->stock_data.vs_bytecode,
               _chain->stock_data.vs_bytecode_dwords, true,
               d3d->dev, "IN.frame_count", &t);
         d3d9_hlsl_set_param_1f(
               _chain->stock_data.ps_bytecode,
               _chain->stock_data.ps_bytecode_dwords, false,
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
   /* Scratch buffer to avoid per-line malloc/free in font rendering */
   Vertex                       *scratch_verts;
   unsigned                      scratch_capacity; /* in Vertex count */
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
   font->texture    = NULL;
   IDirect3DDevice9_CreateTexture(d3d->dev,
         font->tex_width, font->tex_height, 1,
         0, D3DFMT_A8R8G8B8,
         D3DPOOL_MANAGED,
         (struct IDirect3DTexture9**)&font->texture, NULL);

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

   free(font->scratch_verts);
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

/* Get or grow the scratch vertex buffer for font rendering.
 * Avoids per-line calloc/free on the hot path. */
static INLINE Vertex *d3d9_font_get_scratch(
      d3d9_font_t *font, unsigned needed)
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
         font->texture    = NULL;
         IDirect3DDevice9_CreateTexture(d3d->dev,
               font->tex_width, font->tex_height, 1,
               0, D3DFMT_A8R8G8B8,
               D3DPOOL_MANAGED,
               (struct IDirect3DTexture9**)&font->texture, NULL);
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
               Vertex *verts       = d3d9_font_get_scratch(font, msg_len * 6);

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
                           0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                     IDirect3DDevice9_DrawPrimitiveUP(d3d->dev,
                           D3DPT_TRIANGLELIST,
                           vert_count / 3,
                           verts,
                           sizeof(Vertex));

                     IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
                           (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
                           0, sizeof(Vertex));
                  }
               }
            }

            /* Main text pass */
            {
               float pos_x          = x;
               float pos_y          = line_y;
               int lx               = roundf(pos_x * width);
               int ly               = roundf((1.0f - pos_y) * height);
               unsigned vert_count  = 0;
               Vertex *verts        = d3d9_font_get_scratch(font, msg_len * 6);

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
                           0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                     IDirect3DDevice9_SetSamplerState(d3d->dev,
                           0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

                     IDirect3DDevice9_DrawPrimitiveUP(d3d->dev,
                           D3DPT_TRIANGLELIST,
                           vert_count / 3,
                           verts,
                           sizeof(Vertex));

                     IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
                           (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
                           0, sizeof(Vertex));
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

/* Look up a shader uniform register from compiled bytecode CTAB.
 * Returns the register index or -1 if not found. */
/* Set a float shader uniform by name. */
static INLINE void d3d9_hlsl_set_param_1f(
      const DWORD *bytecode, size_t bytecode_dwords,
      bool is_vertex,
      LPDIRECT3DDEVICE9 dev, const char *name, const void *value)
{
   int reg;
   if (!bytecode || !bytecode_dwords || !name || !*name)
      return;
   reg = d3d9_hlsl_ctab_find_register(bytecode, bytecode_dwords, name, NULL, NULL);
   if (reg >= 0)
   {
      float v4[4] = { *(const float*)value, 0.0f, 0.0f, 0.0f };
      if (is_vertex)
         d3d9_hlsl_set_vs_const(dev, reg, v4, 1);
      else
         d3d9_hlsl_set_ps_const(dev, reg, v4, 1);
   }
}

/* Set a 4x4 matrix shader uniform by name. */
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

static DWORD *d3d9_hlsl_dup_bytecode(D3DBlob blob, size_t *out_dwords);

static bool d3d9_hlsl_load_program_ex(
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      const char *prog,
      hlsl_pass_data_t *pd)
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

   if (pd)
   {
      pd->ps_bytecode = d3d9_hlsl_dup_bytecode(code_f, &pd->ps_bytecode_dwords);
      pd->vs_bytecode = d3d9_hlsl_dup_bytecode(code_v, &pd->vs_bytecode_dwords);
      hlsl_uniform_map_from_bytecode(&pd->vs_map, pd->vs_bytecode, pd->vs_bytecode_dwords);
      hlsl_uniform_map_from_bytecode(&pd->ps_map, pd->ps_bytecode, pd->ps_bytecode_dwords);
   }

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

static bool hlsl_d3d9_renderchain_init_shader_fvf(
      d3d9_hlsl_renderchain_t *chain,
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

static void d3d9_hlsl_ctab_dump(const DWORD *bytecode, size_t bytecode_dwords,
      const char *label)
{
   size_t pos;
   const DWORD CTAB_TAG = 0x42415443;
   if (!bytecode || bytecode_dwords < 2) return;
   for (pos = 1; pos < bytecode_dwords; )
   {
      DWORD token = bytecode[pos];
      DWORD opcode = token & 0xFFFF;
      if (opcode == 0xFFFF) break;
      if (opcode == 0xFFFE)
      {
         DWORD comment_dwords = (token >> 16) & 0x7FFF;
         if (pos + 1 + comment_dwords > bytecode_dwords) break;
         if (comment_dwords >= 6 && bytecode[pos + 1] == CTAB_TAG)
         {
            const uint8_t *cs = (const uint8_t*)&bytecode[pos + 1];
            size_t ctb = comment_dwords * sizeof(DWORD);
            const uint8_t *hdr = cs + 4;
            size_t ha = ctb - 4;
            DWORD nc, cio, ci;
            if (ha < 28) return;
            nc = *(const DWORD*)(hdr + 12);
            cio = *(const DWORD*)(hdr + 16);
            RARCH_LOG("[D3D9 HLSL] CTAB %s (%u constants):\n", label, (unsigned)nc);
            for (ci = 0; ci < nc; ci++)
            {
               size_t eo = cio + ci * 20;
               if (eo + 20 > ha) break;
               {
                  const uint8_t *e = hdr + eo;
                  DWORD no = *(const DWORD*)(e + 0);
                  WORD rs = *(const WORD*)(e + 4);
                  WORD ri = *(const WORD*)(e + 6);
                  WORD rc = *(const WORD*)(e + 8);
                  const char *cn = (no < ha) ? (const char*)(hdr + no) : "?";
                  RARCH_LOG("  [%u] '%s' set=%u reg=c%u cnt=%u\n",
                        ci, cn, (unsigned)rs, (unsigned)ri, (unsigned)rc);
               }
            }
            return;
         }
         pos += 1 + comment_dwords; continue;
      }
      pos++;
   }
}
static bool d3d9_hlsl_load_program_from_file_ex(
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      const char *prog,
      hlsl_pass_data_t *pd);

static bool hlsl_d3d9_renderchain_create_first_pass(
      LPDIRECT3DDEVICE9 dev,
      hlsl_renderchain_t *hlsl_chain,
      const struct LinkInfo *info,
      unsigned _fmt)
{
   int i;
   struct shader_pass pass       = { 0 };
   d3d9_hlsl_renderchain_t *chain     = &hlsl_chain->chain;
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

   {
      int32_t filter             = d3d_translate_filter(info->pass->filter);
      for (i = 0; i < TEXTURES; i++)
      {
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

         chain->prev.tex[i] = NULL;
         IDirect3DDevice9_CreateTexture(chain->dev,
               info->tex_w, info->tex_h, 1, 0,
               (D3DFORMAT)fmt,
               D3DPOOL_MANAGED,
               (struct IDirect3DTexture9**)&chain->prev.tex[i], NULL);

         if (!chain->prev.tex[i])
            return false;

         IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)chain->prev.tex[i]);
         IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_MINFILTER, filter);
         IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_MAGFILTER, filter);
         IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_ADDRESSU,  D3DTADDRESS_BORDER);
         IDirect3DDevice9_SetSamplerState(dev,   0, D3DSAMP_ADDRESSV,  D3DTADDRESS_BORDER);
         IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)NULL);
      }
   }

   /* Try to compile the per-pass shader.  If the source path is empty
    * (stock-shader mode), this returns false and pass.vprg/fprg stay NULL
    * — exactly the same as before. */
   hlsl_uniform_map_init(&hlsl_chain->pass_data[0].vs_map);
   hlsl_uniform_map_init(&hlsl_chain->pass_data[0].ps_map);
   hlsl_chain->pass_data[0].vs_bytecode = NULL;
   hlsl_chain->pass_data[0].ps_bytecode = NULL;
   d3d9_hlsl_load_program_from_file_ex(chain->dev,
         &pass, info->pass->source.path,
         &hlsl_chain->pass_data[0]);
   hlsl_chain->pass_data_count = 1;

   if (!hlsl_d3d9_renderchain_init_shader_fvf(chain, &pass))
      return false;
   shader_pass_vector_list_append(chain->passes, pass);
   return true;
}

static void d3d9_hlsl_renderchain_render_pass(
      d3d9_video_t *d3d,
      hlsl_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned pass_index,
      unsigned width, unsigned height,
      D3DVIEWPORT9 *vp,
      unsigned rotation)
{
   unsigned i;
   const hlsl_pass_data_t *pd = NULL;
   int32_t filter;
   unsigned vp_width          = vp->Width;
   unsigned vp_height         = vp->Height;
   const float *mvp_data;

   IDirect3DDevice9_SetViewport(chain->chain.dev, vp);

   /* Compute MVP */
   if (rotation == 0)
      mvp_data = (const float*)&d3d->mvp;
   else
   {
      /* Flat arrays matching register/column layout for
       * SetVertexShaderConstantF + HLSL mul(matrix, vector).
       * Each group of 4 = one register = one column.
       * clip.x = c0.x*x + c1.x*y + c2.x*z + c3.x*w
       * clip.y = c0.y*x + c1.y*y + c2.y*z + c3.y*w */
      static const float rot90[16]  = {
          0, 2, 0, 0, -2, 0, 0, 0, 0, 0, 1, 0, 1,-1, 0, 1 };
      static const float rot180[16] = {
         -2, 0, 0, 0,  0,-2, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1 };
      static const float rot270[16] = {
          0,-2, 0, 0,  2, 0, 0, 0, 0, 0, 1, 0,-1, 1, 0, 1 };
      switch (rotation)
      {
         case 1: mvp_data = rot90;  break;
         case 2: mvp_data = rot180; break;
         case 3: mvp_data = rot270; break;
         default: mvp_data = (const float*)&d3d->mvp; break;
      }
   }

   /* === Set vertices === */
   if (pass->last_width != width || pass->last_height != height)
   {
      struct Vertex vert[4];
      void *verts       = NULL;
      float _u          = (float)(width)  / pass->info.tex_w;
      float _v          = (float)(height) / pass->info.tex_h;

      pass->last_width  = width;
      pass->last_height = height;

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

      IDirect3DVertexBuffer9_Lock(pass->vertex_buf, 0, 0, &verts, 0);
      memcpy(verts, vert, sizeof(vert));
      IDirect3DVertexBuffer9_Unlock(pass->vertex_buf);
   }

   /* Determine the pass data to use for uniform upload */
   if (pass->vprg && pass->fprg
         && pass_index > 0 && pass_index <= chain->pass_data_count)
      pd = &chain->pass_data[pass_index - 1];

   /* === Render pass === */
   filter       = d3d_translate_filter(pass->info.pass->filter);

   if (pd)
   {
      IDirect3DDevice9_SetVertexShader(chain->chain.dev,
            (LPDIRECT3DVERTEXSHADER9)pass->vprg);
      IDirect3DDevice9_SetPixelShader(chain->chain.dev,
            (LPDIRECT3DPIXELSHADER9)pass->fprg);
   }
   else
   {
      IDirect3DDevice9_SetVertexShader(chain->chain.dev,
            (LPDIRECT3DVERTEXSHADER9)(&chain->stock_shader)->vprg);
      IDirect3DDevice9_SetPixelShader(chain->chain.dev,
            (LPDIRECT3DPIXELSHADER9)(&chain->stock_shader)->fprg);
   }

   IDirect3DDevice9_SetTexture(chain->chain.dev, 0,
         (IDirect3DBaseTexture9*)pass->tex);

   /* Upload MVP to c0 (stock shader) */
   IDirect3DDevice9_SetVertexShaderConstantF(chain->chain.dev,
         0, mvp_data, 4);

   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_MINFILTER, filter);
   IDirect3DDevice9_SetSamplerState(chain->chain.dev,
         0, D3DSAMP_MAGFILTER, filter);

   IDirect3DDevice9_SetVertexDeclaration(
         chain->chain.dev, pass->vertex_decl);
   IDirect3DDevice9_SetStreamSource(
         chain->chain.dev, 0, pass->vertex_buf,
         0, sizeof(struct Vertex));

   /* Per-pass shader binding (only when we have compiled programs) */
   if (pd)
   {
      if (pd->vs_map.mvp >= 0)
         IDirect3DDevice9_SetVertexShaderConstantF(chain->chain.dev,
               pd->vs_map.mvp, mvp_data, 4);

      {
         float video_size[2]   = {
            (float)pass->last_width, (float)pass->last_height };
         float texture_size[2] = {
            (float)pass->info.tex_w, (float)pass->info.tex_h };
         float output_size[2]  = {
            (float)vp_width,
            (float)vp_height };
         float frame_cnt;

         frame_cnt = (float)chain->chain.frame_count;
         if (pass->info.pass->frame_count_mod)
            frame_cnt = (float)(chain->chain.frame_count
                  % pass->info.pass->frame_count_mod);

         if (pd->ps_map.video_size >= 0
               && pd->ps_map.video_size == pd->ps_map.texture_size)
         {
            float packed0[4] = { video_size[0], video_size[1],
               texture_size[0], texture_size[1] };
            float packed1[4] = { output_size[0], output_size[1],
               frame_cnt, 1.0f };
            d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.video_size, packed0, 1);
            d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.output_size, packed1, 1);
         }
         else
         {
            float vs4[4] = { video_size[0], video_size[1], 0.0f, 0.0f };
            float ts4[4] = { texture_size[0], texture_size[1], 0.0f, 0.0f };
            d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.video_size,   vs4, 1);
            d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.texture_size, ts4, 1);
            if (pd->ps_map.output_size >= 0
                  && pd->ps_map.output_size == pd->ps_map.frame_count)
            {
               float packed[4] = { output_size[0], output_size[1], frame_cnt, 1.0f };
               d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.output_size, packed, 1);
            }
            else
            {
               float os4[4] = { output_size[0], output_size[1], 0.0f, 0.0f };
               float fc4[4] = { frame_cnt, 0.0f, 0.0f, 0.0f };
               d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.output_size,  os4, 1);
               d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.frame_count,  fc4, 1);
            }
         }

         /* Same for VS */
         if (pd->vs_map.video_size >= 0
               && pd->vs_map.video_size == pd->vs_map.texture_size)
         {
            float packed0[4] = { video_size[0], video_size[1],
               texture_size[0], texture_size[1] };
            float packed1[4] = { output_size[0], output_size[1],
               frame_cnt, 1.0f };
            d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.video_size, packed0, 1);
            d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.output_size, packed1, 1);
         }
         else
         {
            float vs4[4] = { video_size[0], video_size[1], 0.0f, 0.0f };
            float ts4[4] = { texture_size[0], texture_size[1], 0.0f, 0.0f };
            d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.video_size,   vs4, 1);
            d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.texture_size, ts4, 1);
            if (pd->vs_map.output_size >= 0
                  && pd->vs_map.output_size == pd->vs_map.frame_count)
            {
               float packed[4] = { output_size[0], output_size[1], frame_cnt, 1.0f };
               d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.output_size, packed, 1);
            }
            else
            {
               float os4[4] = { output_size[0], output_size[1], 0.0f, 0.0f };
               float fc4[4] = { frame_cnt, 0.0f, 0.0f, 0.0f };
               d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.output_size,  os4, 1);
               d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.frame_count,  fc4, 1);
            }
         }
      }

      /* Bind ORIG texture */
      {
         struct shader_pass *first_pass = (struct shader_pass*)
            &chain->chain.passes->data[0];
         int ps_idx = pd->ps_map.orig_texture;

         if (ps_idx >= 0)
         {
            int32_t orig_filter = d3d_translate_filter(
                  first_pass->info.pass->filter);
            IDirect3DDevice9_SetTexture(chain->chain.dev,
                  ps_idx, (IDirect3DBaseTexture9*)first_pass->tex);
            IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                  ps_idx, D3DSAMP_MINFILTER, orig_filter);
            IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                  ps_idx, D3DSAMP_MAGFILTER, orig_filter);
            IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                  ps_idx, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
            IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                  ps_idx, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
            unsigned_vector_list_append(chain->chain.bound_tex, ps_idx);
         }

         {
            float vs[4] = { (float)first_pass->last_width,
               (float)first_pass->last_height, 0.0f, 0.0f };
            float ts[4] = { (float)first_pass->info.tex_w,
               (float)first_pass->info.tex_h, 0.0f, 0.0f };
            d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.orig_video_size,   vs, 1);
            d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.orig_texture_size, ts, 1);
            d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.orig_video_size,   vs, 1);
            d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.orig_texture_size, ts, 1);
         }
      }

      /* Bind PREV textures */
      {
         int32_t prev_filter = d3d_translate_filter(
               chain->chain.passes->data[0].info.pass->filter);
         float ts[4] = { (float)chain->chain.passes->data[0].info.tex_w,
            (float)chain->chain.passes->data[0].info.tex_h, 0.0f, 0.0f };

         for (i = 0; i < TEXTURES - 1; i++)
         {
            int ps_idx = pd->ps_map.prev_texture[i];
            if (ps_idx >= 0)
            {
               LPDIRECT3DTEXTURE9 tex = chain->chain.prev.tex[
                  (chain->chain.prev.ptr - (i + 1)) & TEXTURESMASK];
               IDirect3DDevice9_SetTexture(chain->chain.dev,
                     ps_idx, (IDirect3DBaseTexture9*)tex);
               IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                     ps_idx, D3DSAMP_MINFILTER, prev_filter);
               IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                     ps_idx, D3DSAMP_MAGFILTER, prev_filter);
               IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                     ps_idx, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
               IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                     ps_idx, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
               unsigned_vector_list_append(chain->chain.bound_tex, ps_idx);
            }

            {
               float vs[4] = {
                  (float)chain->chain.prev.last_width[
                     (chain->chain.prev.ptr - (i + 1)) & TEXTURESMASK],
                  (float)chain->chain.prev.last_height[
                     (chain->chain.prev.ptr - (i + 1)) & TEXTURESMASK],
                  0.0f, 0.0f };
               d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.prev_video_size[i],   vs, 1);
               d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.prev_texture_size[i], ts, 1);
               d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.prev_video_size[i],   vs, 1);
               d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.prev_texture_size[i], ts, 1);
            }
         }
      }

      /* Bind LUT textures */
      for (i = 0; i < chain->chain.luts->count; i++)
      {
         int ps_idx = d3d9_hlsl_ctab_find_register(
               pd->ps_bytecode, pd->ps_bytecode_dwords,
               chain->chain.luts->data[i].id, NULL, NULL);
         if (ps_idx >= 0)
         {
            int32_t lut_filter = chain->chain.luts->data[i].smooth ? 2 : 1;
            IDirect3DDevice9_SetTexture(chain->chain.dev, ps_idx, (IDirect3DBaseTexture9*)chain->chain.luts->data[i].tex);
            IDirect3DDevice9_SetSamplerState(chain->chain.dev, ps_idx, D3DSAMP_MAGFILTER, lut_filter);
            IDirect3DDevice9_SetSamplerState(chain->chain.dev, ps_idx, D3DSAMP_MINFILTER, lut_filter);
            IDirect3DDevice9_SetSamplerState(chain->chain.dev, ps_idx, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
            IDirect3DDevice9_SetSamplerState(chain->chain.dev, ps_idx, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
            unsigned_vector_list_append(chain->chain.bound_tex, ps_idx);
         }
      }

      /* Bind intermediate PASSn textures (2+ indices behind) */
      if (pass_index >= 3)
      {
         for (i = 1; i < pass_index - 1; i++)
         {
            struct shader_pass *cp = (struct shader_pass*)
               &chain->chain.passes->data[i];
            int ps_idx = pd->ps_map.pass_texture[i - 1];

            if (ps_idx >= 0)
            {
               int32_t pf = d3d_translate_filter(cp->info.pass->filter);
               IDirect3DDevice9_SetTexture(chain->chain.dev,
                     ps_idx, (IDirect3DBaseTexture9*)cp->tex);
               IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                     ps_idx, D3DSAMP_MINFILTER, pf);
               IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                     ps_idx, D3DSAMP_MAGFILTER, pf);
               IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                     ps_idx, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
               IDirect3DDevice9_SetSamplerState(chain->chain.dev,
                     ps_idx, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
               unsigned_vector_list_append(chain->chain.bound_tex, ps_idx);
            }

            {
               float vs[4] = { (float)cp->last_width,
                  (float)cp->last_height, 0.0f, 0.0f };
               float ts[4] = { (float)cp->info.tex_w,
                  (float)cp->info.tex_h, 0.0f, 0.0f };
               d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.pass_video_size[i-1],   vs, 1);
               d3d9_hlsl_set_vs_const(chain->chain.dev, pd->vs_map.pass_texture_size[i-1], ts, 1);
               d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.pass_video_size[i-1],   vs, 1);
               d3d9_hlsl_set_ps_const(chain->chain.dev, pd->ps_map.pass_texture_size[i-1], ts, 1);
            }
         }
      }

      /* Upload shader parameters */
      for (i = 0; i < d3d->shader.num_parameters; i++)
      {
         float val[4] = { d3d->shader.parameters[i].current, 0.0f, 0.0f, 0.0f };
         int ps_reg   = d3d9_hlsl_ctab_find_register(
               pd->ps_bytecode, pd->ps_bytecode_dwords,
               d3d->shader.parameters[i].id, NULL, NULL);
         int vs_reg   = d3d9_hlsl_ctab_find_register(
               pd->vs_bytecode, pd->vs_bytecode_dwords,
               d3d->shader.parameters[i].id, NULL, NULL);
         d3d9_hlsl_set_ps_const(chain->chain.dev, ps_reg, val, 1);
         d3d9_hlsl_set_vs_const(chain->chain.dev, vs_reg, val, 1);
      }
   }

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

   /* Unbind all bound textures and vertex streams. */
   {
      int j;
      for (j = 0; j < (int) chain->chain.bound_tex->count; j++)
      {
         IDirect3DDevice9_SetSamplerState(chain->chain.dev,
               chain->chain.bound_tex->data[j], D3DSAMP_MINFILTER, D3DTEXF_POINT);
         IDirect3DDevice9_SetSamplerState(chain->chain.dev,
               chain->chain.bound_tex->data[j], D3DSAMP_MAGFILTER, D3DTEXF_POINT);
         IDirect3DDevice9_SetTexture(chain->chain.dev,
               chain->chain.bound_tex->data[j], (IDirect3DBaseTexture9*)NULL);
      }

      for (j = 0; j < (int) chain->chain.bound_vert->count; j++)
         IDirect3DDevice9_SetStreamSource(chain->chain.dev, chain->chain.bound_vert->data[j], 0, 0, 0);
   }

   if (chain->chain.bound_tex)
      chain->chain.bound_tex->count = 0;

   if (chain->chain.bound_vert)
      chain->chain.bound_vert->count = 0;
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

   /* Free per-pass bytecodes */
   {
      unsigned i;
      for (i = 0; i < chain->pass_data_count; i++)
         hlsl_pass_data_free(&chain->pass_data[i]);
      chain->pass_data_count = 0;
   }
   hlsl_pass_data_free(&chain->stock_data);
}

static void hlsl_d3d9_renderchain_free(void *data)
{
   hlsl_renderchain_t *chain = (hlsl_renderchain_t*)data;

   if (!chain)
      return;

   /* Destroy resources */
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

   /* Destroy passes and luts */
   if (chain->chain.passes)
   {
      int i;

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

   if (!hlsl_d3d9_renderchain_create_first_pass(dev, chain, info, fmt))
      return false;

   hlsl_uniform_map_init(&chain->stock_data.vs_map);
   hlsl_uniform_map_init(&chain->stock_data.ps_map);
   chain->stock_data.vs_bytecode = NULL;
   chain->stock_data.ps_bytecode = NULL;
   if (!d3d9_hlsl_load_program_ex(chain->chain.dev, &chain->stock_shader,
            stock_hlsl_program, &chain->stock_data))
      return false;

   IDirect3DDevice9_SetVertexShader(dev, (LPDIRECT3DVERTEXSHADER9)(&chain->stock_shader)->vprg);
   IDirect3DDevice9_SetPixelShader(dev, (LPDIRECT3DPIXELSHADER9)(&chain->stock_shader)->fprg);

   return true;
}

static void hlsl_d3d9_renderchain_render(
      d3d9_video_t *d3d,
      const void *frame,
      unsigned width, unsigned height,
      unsigned pitch, unsigned rotation)
{
   LPDIRECT3DSURFACE9 back_buffer = NULL;
   LPDIRECT3DSURFACE9 target;
   unsigned i, current_width, current_height,
          out_width = 0, out_height = 0;
   struct shader_pass *last_pass    = NULL;
   struct shader_pass *first_pass   = NULL;
   hlsl_renderchain_t *chain        = (hlsl_renderchain_t*)
      d3d->renderchain_data;

   chain->chain.passes->data[0].tex         = chain->chain.prev.tex[
      chain->chain.prev.ptr];
   chain->chain.passes->data[0].vertex_buf  = chain->chain.prev.vertex_buf[
      chain->chain.prev.ptr];
   chain->chain.passes->data[0].last_width  = chain->chain.prev.last_width[
      chain->chain.prev.ptr];
   chain->chain.passes->data[0].last_height = chain->chain.prev.last_height[
      chain->chain.prev.ptr];

   current_width                  = width;
   current_height                 = height;

   first_pass                     = (struct shader_pass*)
      &chain->chain.passes->data[0];

   /* Blit frame to first pass texture */
   {
      unsigned y;
      D3DLOCKED_RECT d3dlr;
      d3dlr.Pitch  = 0;
      d3dlr.pBits  = NULL;

      IDirect3DTexture9_LockRect(first_pass->tex, 0, &d3dlr, NULL, 0);

      if (first_pass->last_width != width || first_pass->last_height != height)
         memset(d3dlr.pBits, 0, first_pass->info.tex_h * d3dlr.Pitch);

      for (y = 0; y < height; y++)
      {
         const uint8_t *in = (const uint8_t*)frame + y * pitch;
         uint8_t      *out = (uint8_t*)d3dlr.pBits   + y * d3dlr.Pitch;
         memcpy(out, in, width * chain->chain.pixel_size);
      }
      IDirect3DTexture9_UnlockRect(first_pass->tex, 0);
   }

   /* Grab back buffer. */
   if (chain->chain.dev)
   {
      if (!SUCCEEDED(IDirect3DDevice9_GetRenderTarget(chain->chain.dev,
                  0, (LPDIRECT3DSURFACE9*)&back_buffer)))
         back_buffer = NULL;
   }

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

      switch (from_pass->info.pass->fbo.type_x)
      {
         case RARCH_SCALE_VIEWPORT:
            out_width = from_pass->info.pass->fbo.scale_x * chain->chain.out_vp->Width;
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
            out_height = from_pass->info.pass->fbo.scale_y * chain->chain.out_vp->Height;
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

      IDirect3DDevice9_SetViewport(
            chain->chain.dev, (D3DVIEWPORT9*)&viewport);
      IDirect3DDevice9_Clear(
            chain->chain.dev, 0, 0, D3DCLEAR_TARGET,
            0, 1, 0);

      viewport.Width  = out_width;
      viewport.Height = out_height;

      d3d9_hlsl_renderchain_render_pass(
            d3d,
            chain, from_pass,
            i + 1,
            current_width, current_height,
            &viewport, 0);

      current_width  = out_width;
      current_height = out_height;
      IDirect3DSurface9_Release(target);
   }

   /* Final pass */
   IDirect3DDevice9_SetRenderTarget(chain->chain.dev, 0, back_buffer);

   last_pass = (struct shader_pass*)&chain->chain.passes->
      data[chain->chain.passes->count - 1];

   switch (last_pass->info.pass->fbo.type_x)
   {
      case RARCH_SCALE_VIEWPORT:
         out_width = last_pass->info.pass->fbo.scale_x * chain->chain.out_vp->Width;
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
         out_height = last_pass->info.pass->fbo.scale_y * chain->chain.out_vp->Height;
         break;
      case RARCH_SCALE_ABSOLUTE:
         out_height = last_pass->info.pass->fbo.abs_y;
         break;
      case RARCH_SCALE_INPUT:
         out_height = last_pass->info.pass->fbo.scale_y * current_height;
         break;
   }

   d3d9_hlsl_renderchain_render_pass(
         d3d,
         chain, last_pass,
         chain->chain.passes->count,
         current_width, current_height,
         chain->chain.out_vp, rotation);

   chain->chain.frame_count++;

   if (back_buffer)
      IDirect3DSurface9_Release(back_buffer);

   chain->chain.prev.last_width[chain->chain.prev.ptr]  = chain->chain.passes->data[0].last_width;
   chain->chain.prev.last_height[chain->chain.prev.ptr] = chain->chain.passes->data[0].last_height;
   chain->chain.prev.ptr                                = (chain->chain.prev.ptr + 1) & TEXTURESMASK;

   IDirect3DDevice9_SetVertexShader(chain->chain.dev, (LPDIRECT3DVERTEXSHADER9)(&chain->stock_shader)->vprg);
   IDirect3DDevice9_SetPixelShader(chain->chain.dev, (LPDIRECT3DPIXELSHADER9)(&chain->stock_shader)->fprg);

   if (chain->stock_data.vs_map.mvp >= 0)
      IDirect3DDevice9_SetVertexShaderConstantF(chain->chain.dev,
            chain->stock_data.vs_map.mvp, (const float*)&d3d->mvp, 4);
}

static bool hlsl_d3d9_renderchain_add_pass(
      hlsl_renderchain_t *chain,
      const struct LinkInfo *info)
{
   struct shader_pass pass;
   LPDIRECT3DTEXTURE9      tex;
   LPDIRECT3DVERTEXBUFFER9 vertbuf = NULL;

   pass.info                   = *info;
   pass.last_width             = 0;
   pass.last_height            = 0;
   pass.attrib_map             = (struct unsigned_vector_list*)
      unsigned_vector_list_new();
   pass.pool                   = D3DPOOL_DEFAULT;

   {
      unsigned idx = chain->pass_data_count;
      if (idx < GFX_MAX_SHADERS + 1)
      {
         hlsl_uniform_map_init(&chain->pass_data[idx].vs_map);
         hlsl_uniform_map_init(&chain->pass_data[idx].ps_map);
         chain->pass_data[idx].vs_bytecode = NULL;
         chain->pass_data[idx].ps_bytecode = NULL;
         d3d9_hlsl_load_program_from_file_ex(
               chain->chain.dev, &pass, info->pass->source.path,
               &chain->pass_data[idx]);
         chain->pass_data_count++;
      }
      else
         d3d9_hlsl_load_program_from_file(
               chain->chain.dev, &pass, info->pass->source.path);
   }

   if (!hlsl_d3d9_renderchain_init_shader_fvf(&chain->chain, &pass))
      return false;

   if (!SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
               chain->chain.dev,
               4 * sizeof(struct Vertex),
               D3DUSAGE_WRITEONLY, 0,
               D3DPOOL_DEFAULT,
               (LPDIRECT3DVERTEXBUFFER9*)&vertbuf, NULL)))
      return false;

   pass.vertex_buf = vertbuf;

   tex = NULL;
   IDirect3DDevice9_CreateTexture(chain->chain.dev,
         info->tex_w,
         info->tex_h,
         1,
         D3DUSAGE_RENDERTARGET,
         (chain->chain.passes->data[
         chain->chain.passes->count - 1].info.pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
         ? D3DFMT_A32B32G32R32F : D3D9_ARGB8888_FORMAT,
         D3DPOOL_DEFAULT,
         (struct IDirect3DTexture9**)&tex, NULL);

   if (!tex)
      return false;

   pass.tex        = tex;

   IDirect3DDevice9_SetTexture(chain->chain.dev, 0, (IDirect3DBaseTexture9*)pass.tex);
   IDirect3DDevice9_SetSamplerState(chain->chain.dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetSamplerState(chain->chain.dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   IDirect3DDevice9_SetTexture(chain->chain.dev, 0, (IDirect3DBaseTexture9*)NULL);

   shader_pass_vector_list_append(chain->chain.passes, pass);

   return true;
}

/* =====================================================================
 * CTAB-aware shader binding for multi-pass shader presets.
 *
 * These functions are used when a shader preset is loaded and per-pass
 * programs are available.  They use the CTAB uniform maps to set
 * parameters via SetVertexShaderConstantF / SetPixelShaderConstantF
 * and bind ORIG/PREV/PASS/LUT textures at the correct sampler slots.
 *
 * The stock-shader path does NOT use these functions — it continues
 * to use the existing no-op stubs, preserving identical behaviour.
 * ===================================================================== */

static DWORD *d3d9_hlsl_dup_bytecode(D3DBlob blob, size_t *out_dwords)
{
   size_t byte_size = blob->lpVtbl->GetBufferSize(blob);
   size_t dwords    = byte_size / sizeof(DWORD);
   DWORD *buf       = (DWORD*)malloc(byte_size);
   if (buf)
   {
      memcpy(buf, blob->lpVtbl->GetBufferPointer(blob), byte_size);
      *out_dwords = dwords;
   }
   else
      *out_dwords = 0;
   return buf;
}

/* Read a file into a malloc'd null-terminated string. Caller frees. */
static char *d3d9_hlsl_read_file(const char *path, size_t *out_len)
{
   int64_t len;
   char *buf = NULL;
   RFILE *fp = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!fp)
      return NULL;

   len = filestream_get_size(fp);
   if (len <= 0)
   {
      filestream_close(fp);
      return NULL;
   }

   buf = (char*)malloc((size_t)len + 1);
   if (!buf)
   {
      filestream_close(fp);
      return NULL;
   }

   filestream_read(fp, buf, (size_t)len);
   buf[len] = '\0';
   filestream_close(fp);

   if (out_len)
      *out_len = (size_t)len;
   return buf;
}

/* Simple #include preprocessor for Cg/HLSL shader files.
 *
 * Scans the source for lines matching:
 *   #include "filename"
 * and replaces them with the contents of the referenced file,
 * resolved relative to base_dir.  Handles recursive includes
 * up to 16 levels deep. Returns a malloc'd string. */
/* Simple preprocessor state for #ifdef/#ifndef/#else/#endif.
 * Supports nesting up to 32 levels deep. */
#define PREPROC_MAX_DEPTH 32

typedef struct {
   const char *names[64];
   const char *values[64]; /* NULL = defined but no value (flag only) */
   unsigned count;
} preproc_defines_t;

static void preproc_defines_init(preproc_defines_t *d)
{
   unsigned i;
   for (i = 0; i < d->count; i++)
   {
      free((void*)d->names[i]);
      d->names[i] = NULL;
      d->values[i] = NULL;
   }
   d->count = 0;
}

static bool preproc_is_defined(const preproc_defines_t *d, const char *name, size_t name_len)
{
   unsigned i;
   for (i = 0; i < d->count; i++)
      if (strlen(d->names[i]) == name_len && strncmp(d->names[i], name, name_len) == 0)
         return true;
   return false;
}


static void preproc_add_define(preproc_defines_t *d, const char *name, const char *value)
{
   if (d->count < 64)
   {
      d->names[d->count]  = name;
      d->values[d->count] = value;
      d->count++;
   }
}

static bool d3d9_hlsl_is_ident_char(char c);

static preproc_defines_t s_defines = { {NULL}, {NULL}, 0 };

static char *d3d9_hlsl_preprocess_includes(
      const char *source, const char *base_dir, unsigned depth)
{
   size_t src_len = strlen(source);
   size_t cap     = src_len * 2 + 1;
   size_t pos     = 0;
   char *out      = (char*)malloc(cap);
   const char *p  = source;

   /* Conditional compilation stack */
   bool   cond_active[PREPROC_MAX_DEPTH]; /* is this level's branch active? */
   bool   cond_done[PREPROC_MAX_DEPTH];   /* has any branch been taken? */
   int    cond_depth = 0;
   bool   output_enabled = true;

   /* Reset defines at the top-level call (depth 0) so each shader
    * file gets a fresh set of preprocessor definitions. */
   if (depth == 0)
      preproc_defines_init(&s_defines);

   if (!out || depth > 16)
      return out;

   /* Initialize top-level as active */
   cond_active[0] = true;
   cond_done[0]   = true;

   while (*p)
   {
      const char *line_end = strchr(p, '\n');
      size_t line_len;
      const char *s;

      if (!line_end)
         line_end = p + strlen(p);
      else
         line_end++;

      line_len = (size_t)(line_end - p);

      /* Skip leading whitespace to check for preprocessor directives */
      s = p;
      while (s < line_end && (*s == ' ' || *s == '\t'))
         s++;

      if (*s == '#')
      {
         const char *dir = s + 1;
         while (dir < line_end && (*dir == ' ' || *dir == '\t'))
            dir++;

         /* #ifdef NAME */
         if (strncmp(dir, "ifdef", 5) == 0 && (dir[5] == ' ' || dir[5] == '\t'))
         {
            const char *nm = dir + 5;
            const char *nm_end;
            while (*nm == ' ' || *nm == '\t') nm++;
            nm_end = nm;
            while (nm_end < line_end && *nm_end != ' ' && *nm_end != '\t'
                  && *nm_end != '\r' && *nm_end != '\n')
               nm_end++;

            if (cond_depth < PREPROC_MAX_DEPTH - 1)
            {
               cond_depth++;
               if (output_enabled)
               {
                  bool def = preproc_is_defined(&s_defines, nm, (size_t)(nm_end - nm));
                  cond_active[cond_depth] = def;
                  cond_done[cond_depth]   = def;
                  output_enabled          = def;
               }
               else
               {
                  cond_active[cond_depth] = false;
                  cond_done[cond_depth]   = true; /* parent inactive */
               }
            }
            p = line_end;
            continue;
         }

         /* #ifndef NAME */
         if (strncmp(dir, "ifndef", 6) == 0 && (dir[6] == ' ' || dir[6] == '\t'))
         {
            const char *nm = dir + 6;
            const char *nm_end;
            while (*nm == ' ' || *nm == '\t') nm++;
            nm_end = nm;
            while (nm_end < line_end && *nm_end != ' ' && *nm_end != '\t'
                  && *nm_end != '\r' && *nm_end != '\n')
               nm_end++;

            if (cond_depth < PREPROC_MAX_DEPTH - 1)
            {
               cond_depth++;
               if (output_enabled)
               {
                  bool def = preproc_is_defined(&s_defines, nm, (size_t)(nm_end - nm));
                  cond_active[cond_depth] = !def;
                  cond_done[cond_depth]   = !def;
                  output_enabled          = !def;
               }
               else
               {
                  cond_active[cond_depth] = false;
                  cond_done[cond_depth]   = true;
               }
            }
            p = line_end;
            continue;
         }

         /* #if defined(X) / #if !defined(X) / #if defined(X) || defined(Y) */
         if (strncmp(dir, "if", 2) == 0 && (dir[2] == ' ' || dir[2] == '\t'))
         {
            const char *expr = dir + 2;
            bool result = false;

            while (*expr == ' ' || *expr == '\t') expr++;

            /* Evaluate compound defined() expressions with || and && */
            {
               bool have_result = false;
               bool current_op_is_or = true; /* default: first term */

               while (expr < line_end)
               {
                  bool negate = false;
                  bool term_result = false;

                  while (*expr == ' ' || *expr == '\t' || *expr == '(') expr++;
                  if (*expr == '!')
                  {
                     negate = true;
                     expr++;
                     while (*expr == ' ' || *expr == '\t') expr++;
                  }

                  if (strncmp(expr, "defined", 7) == 0)
                  {
                     const char *nm, *nm_end;
                     expr += 7;
                     while (*expr == ' ' || *expr == '\t') expr++;
                     if (*expr == '(') expr++;
                     while (*expr == ' ' || *expr == '\t') expr++;
                     nm = expr;
                     nm_end = nm;
                     while (nm_end < line_end && d3d9_hlsl_is_ident_char(*nm_end))
                        nm_end++;
                     term_result = preproc_is_defined(&s_defines, nm, (size_t)(nm_end - nm));
                     if (negate) term_result = !term_result;
                     expr = nm_end;
                     while (*expr == ')' || *expr == ' ' || *expr == '\t') expr++;
                     have_result = true;
                  }
                  else
                     break; /* Can't parse further */

                  if (current_op_is_or)
                     result = have_result ? (result || term_result) : term_result;
                  else
                     result = result && term_result;

                  /* Check for || or && */
                  if (expr + 1 < line_end && expr[0] == '|' && expr[1] == '|')
                  { current_op_is_or = true; expr += 2; continue; }
                  if (expr + 1 < line_end && expr[0] == '&' && expr[1] == '&')
                  { current_op_is_or = false; expr += 2; continue; }
                  break;
               }

               if (have_result)
               {
                  if (cond_depth < PREPROC_MAX_DEPTH - 1)
                  {
                     cond_depth++;
                     if (output_enabled)
                     {
                        cond_active[cond_depth] = result;
                        cond_done[cond_depth]   = result;
                        output_enabled          = result;
                     }
                     else
                     {
                        cond_active[cond_depth] = false;
                        cond_done[cond_depth]   = true;
                     }
                  }
                  p = line_end;
                  continue;
               }
            }

            /* Unhandled #if expression — treat as unknown (skip block) */
            if (cond_depth < PREPROC_MAX_DEPTH - 1)
            {
               cond_depth++;
               cond_active[cond_depth] = false;
               cond_done[cond_depth]   = false;
               if (output_enabled)
                  output_enabled = false;
            }
            p = line_end;
            continue;
         }

         /* #elif defined(X) / #elif defined(X) || defined(Y) */
         if (strncmp(dir, "elif", 4) == 0 && (dir[4] == ' ' || dir[4] == '\t'))
         {
            if (cond_depth > 0)
            {
               bool parent_active = (cond_depth <= 1) || cond_active[cond_depth - 1];
               if (parent_active && !cond_done[cond_depth])
               {
                  /* Evaluate the elif condition */
                  const char *expr = dir + 4;
                  bool result = false;

                  while (*expr == ' ' || *expr == '\t') expr++;

                  /* Evaluate compound defined() expressions */
                  {
                     bool have_result = false;
                     bool current_op_is_or = true;

                     while (expr < line_end)
                     {
                        bool negate = false;
                        bool term_result = false;

                        while (*expr == ' ' || *expr == '\t' || *expr == '(') expr++;
                        if (*expr == '!')
                        {
                           negate = true;
                           expr++;
                           while (*expr == ' ' || *expr == '\t') expr++;
                        }

                        if (strncmp(expr, "defined", 7) == 0)
                        {
                           const char *nm, *nm_end;
                           expr += 7;
                           while (*expr == ' ' || *expr == '\t') expr++;
                           if (*expr == '(') expr++;
                           while (*expr == ' ' || *expr == '\t') expr++;
                           nm = expr;
                           nm_end = nm;
                           while (nm_end < line_end && d3d9_hlsl_is_ident_char(*nm_end))
                              nm_end++;
                           term_result = preproc_is_defined(&s_defines, nm, (size_t)(nm_end - nm));
                           if (negate) term_result = !term_result;
                           expr = nm_end;
                           while (*expr == ')' || *expr == ' ' || *expr == '\t') expr++;
                           have_result = true;
                        }
                        else
                           break;

                        if (current_op_is_or)
                           result = have_result ? (result || term_result) : term_result;
                        else
                           result = result && term_result;

                        if (expr + 1 < line_end && expr[0] == '|' && expr[1] == '|')
                        { current_op_is_or = true; expr += 2; continue; }
                        if (expr + 1 < line_end && expr[0] == '&' && expr[1] == '&')
                        { current_op_is_or = false; expr += 2; continue; }
                        break;
                     }

                     if (have_result && result)
                     {
                        cond_active[cond_depth] = true;
                        cond_done[cond_depth]   = true;
                        output_enabled          = true;
                     }
                     else
                     {
                        cond_active[cond_depth] = false;
                        output_enabled          = false;
                     }
                  }
               }
               else
               {
                  cond_active[cond_depth] = false;
                  output_enabled          = false;
               }
            }
            p = line_end;
            continue;
         }

         /* #else */
         if (strncmp(dir, "else", 4) == 0
               && (dir[4] == '\r' || dir[4] == '\n' || dir[4] == ' '
                  || dir[4] == '\t' || dir[4] == '\0'))
         {
            if (cond_depth > 0)
            {
               /* Check if parent is active */
               bool parent_active = (cond_depth <= 1) || cond_active[cond_depth - 1];
               if (parent_active && !cond_done[cond_depth])
               {
                  cond_active[cond_depth] = true;
                  cond_done[cond_depth]   = true;
                  output_enabled          = true;
               }
               else
               {
                  cond_active[cond_depth] = false;
                  output_enabled          = false;
               }
            }
            p = line_end;
            continue;
         }

         /* #endif */
         if (strncmp(dir, "endif", 5) == 0
               && (dir[5] == '\r' || dir[5] == '\n' || dir[5] == ' '
                  || dir[5] == '\t' || dir[5] == '\0'))
         {
            if (cond_depth > 0)
               cond_depth--;
            /* Recalculate output_enabled */
            output_enabled = true;
            {
               int i;
               for (i = 1; i <= cond_depth; i++)
                  if (!cond_active[i])
                  { output_enabled = false; break; }
            }
            p = line_end;
            continue;
         }

         /* #define NAME [value] — parse and store for expansion */
         if (output_enabled && strncmp(dir, "define", 6) == 0
               && (dir[6] == ' ' || dir[6] == '\t'))
         {
            const char *nm = dir + 6;
            const char *nm_end;
            while (*nm == ' ' || *nm == '\t') nm++;
            nm_end = nm;
            while (nm_end < line_end && d3d9_hlsl_is_ident_char(*nm_end))
               nm_end++;

            /* Mark as defined for #ifdef checks */
            if (*nm_end != '(')
            {
               size_t nlen = (size_t)(nm_end - nm);
               char *name_copy = (char*)malloc(nlen + 1);
               if (name_copy)
               {
                  memcpy(name_copy, nm, nlen);
                  name_copy[nlen] = '\0';
                  preproc_add_define(&s_defines, name_copy, NULL);
               }
            }
            /* Line is output below by the regular output path */
         }

         /* #include handling (only when output is enabled) */
         if (output_enabled && strncmp(dir, "include", 7) == 0)
         {
            const char *q = dir + 7;
            while (q < line_end && (*q == ' ' || *q == '\t'))
               q++;

            if (*q == '"' || *q == '<')
            {
               char close_char    = (*q == '"') ? '"' : '>';
               const char *name_start = q + 1;
               const char *name_end   = strchr(name_start, close_char);

               if (name_end && name_end < line_end)
               {
                  char inc_path[PATH_MAX_LENGTH];
                  char inc_name[PATH_MAX_LENGTH];
                  size_t name_len_n = (size_t)(name_end - name_start);
                  char *inc_src;

                  if (name_len_n >= sizeof(inc_name))
                     name_len_n = sizeof(inc_name) - 1;
                  memcpy(inc_name, name_start, name_len_n);
                  inc_name[name_len_n] = '\0';

                  fill_pathname_join(inc_path, base_dir, inc_name,
                        sizeof(inc_path));

                  inc_src = d3d9_hlsl_read_file(inc_path, NULL);
                  if (!inc_src)
                     RARCH_WARN("[D3D9 HLSL] Could not open include: %s\n", inc_path);
                  if (inc_src)
                  {
                     char inc_dir[PATH_MAX_LENGTH];
                     char *resolved_inc;
                     strlcpy(inc_dir, inc_path, sizeof(inc_dir));
                     path_basedir(inc_dir);

                     resolved_inc = d3d9_hlsl_preprocess_includes(
                           inc_src, inc_dir, depth + 1);
                     free(inc_src);

                     if (resolved_inc)
                     {
                        size_t resolved_len = strlen(resolved_inc);
                        while (pos + resolved_len + 2 >= cap)
                        {
                           cap *= 2;
                           out  = (char*)realloc(out, cap);
                           if (!out) { free(resolved_inc); return NULL; }
                        }
                        memcpy(out + pos, resolved_inc, resolved_len);
                        pos += resolved_len;
                        if (resolved_len > 0 && resolved_inc[resolved_len - 1] != '\n')
                           out[pos++] = '\n';
                        free(resolved_inc);
                     }

                     p = line_end;
                     continue;
                  }
               }
            }
         }
      }

      /* Output the line if enabled (and not a consumed preprocessor directive) */
      if (output_enabled)
      {
         while (pos + line_len + 1 >= cap)
         {
            cap *= 2;
            out  = (char*)realloc(out, cap);
            if (!out) return NULL;
         }
         memcpy(out + pos, p, line_len);
         pos += line_len;
      }

      p = line_end;
   }

   out[pos] = '\0';
   return out;
}

/* =====================================================================
 * Cg → HLSL source-level fixups
 *
 * 1. Whole-word rename 'texture' → '_texture' (HLSL reserved keyword)
 * 2. Extract 'uniform <type> <name>' parameters from function signatures
 *    and hoist them to global scope.  HLSL requires semantics on every
 *    VS/PS input parameter; Cg's 'uniform' qualifier marks parameters
 *    that are set by the runtime (not vertex-stream data) which have
 *    no semantic.  Moving them to global scope makes them plain globals
 *    that the HLSL compiler treats as constants (matching CTAB layout).
 * ===================================================================== */

static bool d3d9_hlsl_is_ident_char(char c)
{
   return (c >= 'A' && c <= 'Z')
       || (c >= 'a' && c <= 'z')
       || (c >= '0' && c <= '9')
       || c == '_';
}

/* Append a string to a dynamic buffer, growing as needed. */
static bool d3d9_hlsl_buf_append(char **buf, size_t *pos, size_t *cap,
      const char *str, size_t len)
{
   while (*pos + len + 1 >= *cap)
   {
      *cap *= 2;
      *buf  = (char*)realloc(*buf, *cap);
      if (!*buf) return false;
   }
   memcpy(*buf + *pos, str, len);
   *pos += len;
   return true;
}

/* Add TEXCOORD semantics to struct members that lack them.
 * Simple approach: find struct { ... } blocks, check if they qualify,
 * and rewrite the block with semantics added.
 *
 * Returns a malloc'd modified copy, or NULL if no changes needed. */
static char *d3d9_hlsl_add_struct_semantics(const char *source)
{
   /* Known structs to skip — these are uniform/input structs that
    * must NOT get TEXCOORD semantics added to their members.
    * 'input' is the Cg compat struct for video_size/texture_size/etc.
    * 'in_vertex' is a dummy struct for the vertex input macro. */
   static const char *skip_names[] = {
      "input",
      "in_vertex",
      NULL
   };
   const char *p = source;
   int texcoord_counter = 2;

   /* First pass: find if any struct qualifies. If not, return NULL early. */
   bool any_qualify = false;
   {
      const char *s = source;
      while ((s = strstr(s, "struct")) != NULL)
      {
         if ((s == source || !d3d9_hlsl_is_ident_char(s[-1]))
               && !d3d9_hlsl_is_ident_char(s[6]))
         {
            const char *nm = s + 6;
            const char *nm_end;
            size_t nm_len;
            bool skip = false;
            int i;

            while (*nm == ' ' || *nm == '\t' || *nm == '\n' || *nm == '\r') nm++;
            nm_end = nm;
            while (d3d9_hlsl_is_ident_char(*nm_end)) nm_end++;
            nm_len = (size_t)(nm_end - nm);
            if (nm_len == 0) { s++; continue; }

            for (i = 0; skip_names[i]; i++)
               if (strlen(skip_names[i]) == nm_len
                     && strncmp(nm, skip_names[i], nm_len) == 0)
               { skip = true; break; }

            if (!skip)
            {
               /* Find { ... } */
               const char *ob = nm_end;
               while (*ob && *ob != '{' && *ob != ';') ob++;
               if (*ob == '{')
               {
                  const char *cb = ob + 1;
                  int d = 1;
                  bool any_no_sem = false, has_sampler = false, any_mem = false;
                  while (*cb && d > 0)
                  {
                     if (*cb == '{') d++;
                     else if (*cb == '}') d--;
                     else if (d == 1 && strncmp(cb, "sampler", 7) == 0) has_sampler = true;
                     else if (*cb == ';' && d == 1)
                     {
                        any_mem = true;
                        /* Check if this specific member has ':' */
                        {
                           const char *ms = cb - 1;
                           bool mem_has_sem = false;
                           while (ms > ob && *ms != ';' && *ms != '{')
                           {
                              if (*ms == ':') mem_has_sem = true;
                              ms--;
                           }
                           if (!mem_has_sem)
                              any_no_sem = true;
                        }
                     }
                     cb++;
                  }
                  if (any_mem && any_no_sem && !has_sampler)
                     any_qualify = true;
               }
            }
         }
         s++;
      }
   }

   if (!any_qualify)
      return NULL;


   /* Second pass: copy source, rewriting qualifying structs */
   {
      size_t src_len = strlen(source);
      size_t cap = src_len + 1024;
      size_t opos = 0;
      char *out = (char*)malloc(cap);
      if (!out) return NULL;

      p = source;
      while (*p)
      {
         if (strncmp(p, "struct", 6) == 0
               && (p == source || !d3d9_hlsl_is_ident_char(p[-1]))
               && !d3d9_hlsl_is_ident_char(p[6]))
         {
            const char *nm = p + 6;
            const char *nm_end;
            size_t nm_len;
            bool skip = false;
            int i;

            while (*nm == ' ' || *nm == '\t' || *nm == '\n' || *nm == '\r') nm++;
            nm_end = nm;
            while (d3d9_hlsl_is_ident_char(*nm_end)) nm_end++;
            nm_len = (size_t)(nm_end - nm);

            if (nm_len == 0) { p++; continue; }
            for (i = 0; skip_names[i]; i++)
               if (strlen(skip_names[i]) == nm_len
                     && strncmp(nm, skip_names[i], nm_len) == 0)
               { skip = true; break; }

            if (!skip)
            {
               const char *ob = nm_end;
               while (*ob && *ob != '{' && *ob != ';') ob++;
               if (*ob == '{')
               {
                  const char *cb = ob + 1;
                  int d = 1;
                  bool any_no_sem = false, has_sampler = false, any_mem = false;
                  while (*cb && d > 0)
                  {
                     if (*cb == '{') d++;
                     else if (*cb == '}') d--;
                     else if (d == 1 && strncmp(cb, "sampler", 7) == 0) has_sampler = true;
                     else if (*cb == ';' && d == 1)
                     {
                        any_mem = true;
                        {
                           const char *ms = cb - 1;
                           bool mem_has_sem = false;
                           while (ms > ob && *ms != ';' && *ms != '{')
                           {
                              if (*ms == ':') mem_has_sem = true;
                              ms--;
                           }
                           if (!mem_has_sem)
                              any_no_sem = true;
                        }
                     }
                     cb++;
                  }

                  if (any_mem && any_no_sem && !has_sampler)
                  {
                     /* Copy from p to ob (inclusive of '{') */
                     {
                        size_t len = (size_t)(ob + 1 - p);
                        while (opos + len + 1 >= cap)
                        { cap *= 2; out = (char*)realloc(out, cap); if (!out) return NULL; }
                        memcpy(out + opos, p, len);
                        opos += len;
                     }

                     /* Copy struct body, inserting TEXCOORD only before
                      * ';' on members that don't already have ':' */
                     /* First, scan for existing TEXCOORD indices to avoid conflicts */
                     {
                        const char *scan_tc = ob + 1;
                        while (scan_tc < cb - 1)
                        {
                           if (strncmp(scan_tc, "TEXCOORD", 8) == 0)
                           {
                              int idx = 0;
                              const char *dp = scan_tc + 8;
                              while (*dp >= '0' && *dp <= '9')
                              {
                                 idx = idx * 10 + (*dp - '0');
                                 dp++;
                              }
                              if (idx >= texcoord_counter)
                                 texcoord_counter = idx + 1;
                           }
                           scan_tc++;
                        }
                     }
                     {
                        const char *mp = ob + 1;
                        while (mp < cb - 1)
                        {
                           if (*mp == ';')
                           {
                              /* Check if this member already has ':' */
                              bool has_colon = false;
                              {
                                 size_t bi = opos;
                                 while (bi > 0 && out[bi-1] != ';' && out[bi-1] != '{')
                                 {
                                    if (out[bi-1] == ':') has_colon = true;
                                    bi--;
                                 }
                              }
                              if (!has_colon)
                              {
                                 char sem[32];
                                 size_t sl = snprintf(sem, sizeof(sem),
                                       " : TEXCOORD%d", texcoord_counter++);
                                 while (opos + sl + 2 >= cap)
                                 { cap *= 2; out = (char*)realloc(out, cap); if (!out) return NULL; }
                                 memcpy(out + opos, sem, sl);
                                 opos += sl;
                              }
                           }
                           while (opos + 2 >= cap)
                           { cap *= 2; out = (char*)realloc(out, cap); if (!out) return NULL; }
                           out[opos++] = *mp++;
                        }
                     }

                     /* Copy '}' and advance past struct */
                     while (opos + 2 >= cap)
                     { cap *= 2; out = (char*)realloc(out, cap); if (!out) return NULL; }
                     out[opos++] = '}';
                     p = cb;
                     continue;
                  }
               }
            }
         }

         /* Regular char */
         while (opos + 2 >= cap)
         { cap *= 2; out = (char*)realloc(out, cap); if (!out) return NULL; }
         out[opos++] = *p++;
      }

      out[opos] = '\0';
      return out;
   }
}

/* Decompose sampler members out of structs for HLSL SM3.0 compatibility.
 *
 * HLSL SM3.0 doesn't allow samplers to be members of compound types.
 * This pass:
 *  1. Removes 'sampler2D _texture;' lines from struct bodies
 *  2. For each 'uniform <structtype> VARNAME;', emits a standalone
 *     'sampler2D VARNAME__texture;' declaration
 *  3. Replaces 'VARNAME._texture' with 'VARNAME__texture' everywhere
 *
 * Returns a malloc'd modified copy, or NULL if no changes needed. */
static char *d3d9_hlsl_decompose_struct_samplers(const char *source)
{
   /* Collect struct names that contain sampler members */
   const char *sampler_structs[16];
   size_t sampler_struct_lens[16];
   unsigned sampler_struct_count = 0;

   /* Collect instance variable names of those structs */
   const char *instance_names[32];
   size_t instance_name_lens[32];
   unsigned instance_count = 0;

   size_t src_len, cap, opos;
   char *out;
   const char *p;
   bool modified = false;

   /* Pass 1: find structs with sampler members */
   {
      const char *s = source;
      while ((s = strstr(s, "struct")) != NULL)
      {
         if ((s == source || !d3d9_hlsl_is_ident_char(s[-1]))
               && !d3d9_hlsl_is_ident_char(s[6]))
         {
            const char *nm = s + 6;
            const char *nm_end;
            while (*nm == ' ' || *nm == '\t' || *nm == '\n' || *nm == '\r') nm++;
            nm_end = nm;
            while (d3d9_hlsl_is_ident_char(*nm_end)) nm_end++;

            if (nm_end > nm)
            {
               /* Find struct body */
               const char *ob = nm_end;
               while (*ob && *ob != '{' && *ob != ';') ob++;
               if (*ob == '{')
               {
                  const char *cb = ob + 1;
                  int d = 1;
                  bool has_sampler = false;
                  while (*cb && d > 0)
                  {
                     if (*cb == '{') d++;
                     else if (*cb == '}') d--;
                     else if (d == 1 && strncmp(cb, "sampler", 7) == 0)
                        has_sampler = true;
                     cb++;
                  }
                  if (has_sampler && sampler_struct_count < 16)
                  {
                     sampler_structs[sampler_struct_count] = nm;
                     sampler_struct_lens[sampler_struct_count] = (size_t)(nm_end - nm);
                     sampler_struct_count++;
                  }
               }
            }
         }
         s++;
      }
   }

   if (sampler_struct_count == 0)
      return NULL;

   /* Pass 2: find instances of sampler-containing structs
    * Pattern: 'uniform <structname> VARNAME;' or just '<structname> VARNAME;' at global scope */
   {
      const char *s = source;
      unsigned si;
      while (*s)
      {
         for (si = 0; si < sampler_struct_count; si++)
         {
            size_t slen = sampler_struct_lens[si];
            if (strncmp(s, sampler_structs[si], slen) == 0
                  && !d3d9_hlsl_is_ident_char(s[slen])
                  && (s == source || !d3d9_hlsl_is_ident_char(s[-1])
                     || (s >= source + 8
                        && strncmp(s - 8, "uniform ", 8) == 0)))
            {
               /* Find instance name after struct type */
               const char *vn = s + slen;
               const char *vn_end;
               while (*vn == ' ' || *vn == '\t') vn++;
               vn_end = vn;
               while (d3d9_hlsl_is_ident_char(*vn_end)) vn_end++;

               if (vn_end > vn && instance_count < 32)
               {
                  instance_names[instance_count] = vn;
                  instance_name_lens[instance_count] = (size_t)(vn_end - vn);
                  instance_count++;
               }
            }
         }
         s++;
      }
   }

   /* Pass 3: rewrite source */
   src_len = strlen(source);
   cap = src_len + instance_count * 64 + 1024;
   opos = 0;
   out = (char*)malloc(cap);
   if (!out) return NULL;

   p = source;
   while (*p)
   {
      /* Remove sampler member lines from sampler-containing structs.
       * Look for lines containing 'sampler' followed by ';' */
      {
         bool skip_sampler_line = false;
         if (strncmp(p, "sampler", 7) == 0 || (p > source && p[-1] != '\n'
               && strstr(p, "sampler") == p))
         {
            /* Actually, let's detect sampler lines more carefully:
             * at the start of a line (after whitespace), check if we see
             * 'sampler2D' followed by '_texture' and ';' */
         }

         /* Simpler approach: detect 'sampler2D _texture;' or 'sampler2D _texture ;'
          * as a standalone line (with optional leading whitespace) */
         {
            const char *line_start = p;
            /* Check if we're at line start */
            if (p == source || p[-1] == '\n')
            {
               const char *ls = p;
               while (*ls == ' ' || *ls == '\t') ls++;
               /* Skip optional 'uniform' keyword */
               if (strncmp(ls, "uniform", 7) == 0
                     && !d3d9_hlsl_is_ident_char(ls[7]))
               {
                  ls += 7;
                  while (*ls == ' ' || *ls == '\t') ls++;
               }
               if (strncmp(ls, "sampler2D", 9) == 0
                     && !d3d9_hlsl_is_ident_char(ls[9]))
               {
                  const char *after_type = ls + 9;
                  while (*after_type == ' ' || *after_type == '\t') after_type++;
                  if (strncmp(after_type, "_texture", 8) == 0
                        && !d3d9_hlsl_is_ident_char(after_type[8]))
                  {
                     /* Check this is inside a struct (find ';' on this line) */
                     const char *semi = after_type + 8;
                     while (*semi == ' ' || *semi == '\t') semi++;
                     /* Skip optional ': SEMANTIC' annotation */
                     if (*semi == ':')
                     {
                        semi++;
                        while (*semi && *semi != ';' && *semi != '\n') semi++;
                     }
                     if (*semi == ';')
                     {
                        /* Check if we're inside a struct body by looking
                         * for a preceding '{' without a matching '}' */
                        /* Simple heuristic: skip this line */
                        const char *nl = strchr(semi, '\n');
                        if (nl) p = nl + 1; else p = semi + 1 + strlen(semi + 1);
                        modified = true;
                        continue;
                     }
                  }
               }
            }
         }
      }

      /* After 'uniform <structname> VARNAME;', add 'sampler2D VARNAME__texture;' */
      {
         unsigned ii;
         for (ii = 0; ii < instance_count; ii++)
         {
            size_t ilen = instance_name_lens[ii];
            if (strncmp(p, instance_names[ii], ilen) == 0
                  && !d3d9_hlsl_is_ident_char(p[ilen])
                  && (p == source || !d3d9_hlsl_is_ident_char(p[-1])))
            {
               /* Check if followed by ';' (this is the declaration) */
               const char *after = p + ilen;
               while (*after == ' ' || *after == '\t') after++;
               if (*after == ';')
               {
                  /* Copy 'VARNAME;' then add sampler declaration */
                  size_t chunk = (size_t)(after + 1 - p);
                  while (opos + chunk + ilen + 40 >= cap)
                  { cap *= 2; out = (char*)realloc(out, cap); if (!out) return NULL; }
                  memcpy(out + opos, p, chunk);
                  opos += chunk;

                  /* Add newline + sampler declaration */
                  opos += snprintf(out + opos, cap - opos,
                        "\nsampler2D %.*s__texture;",
                        (int)ilen, instance_names[ii]);

                  p = after + 1;
                  modified = true;
                  continue;
               }
            }
         }
      }

      /* Replace ._texture with __texture globally.
       * Any struct member access on _texture should be decomposed
       * since _texture was our renamed sampler member.
       * Special case: in #define macros with ## token pasting,
       * use ##__texture to preserve correct token pasting. */
      if (*p == '.' && strncmp(p + 1, "_texture", 8) == 0
            && !d3d9_hlsl_is_ident_char(p[9]))
      {
         /* Check if preceded by ## (token paste operator) */
         bool has_paste = (opos >= 2 && out[opos-1] == '#' && out[opos-2] == '#');
         /* Also check for ##<identifier>. before ._texture:
          * e.g. ##c._texture — the ## was before the identifier, not before the dot */
         if (!has_paste && opos >= 1 && d3d9_hlsl_is_ident_char(out[opos-1]))
         {
            /* Scan back past the identifier to see if ## precedes it */
            size_t bi = opos - 1;
            while (bi > 0 && d3d9_hlsl_is_ident_char(out[bi-1])) bi--;
            if (bi >= 2 && out[bi-1] == '#' && out[bi-2] == '#')
               has_paste = true;
         }

         while (opos + 14 >= cap)
         { cap *= 2; out = (char*)realloc(out, cap); if (!out) return NULL; }

         if (has_paste)
         {
            memcpy(out + opos, "##__texture", 11);
            opos += 11;
         }
         else
         {
            memcpy(out + opos, "__texture", 9);
            opos += 9;
         }
         p += 9; /* skip ._texture */
         modified = true;
         continue;
      }

      /* Regular char */
      while (opos + 2 >= cap)
      { cap *= 2; out = (char*)realloc(out, cap); if (!out) return NULL; }
      out[opos++] = *p++;
   }

   if (!modified)
   {
      free(out);
      return NULL;
   }

   out[opos] = '\0';
   return out;
}

#ifdef _MSC_VER
#pragma optimize("", off) /* Avoid MSVC ICE on complex function */
#endif
static char *d3d9_hlsl_fixup_cg_source(const char *source)
{
   size_t src_len     = strlen(source);
   size_t cap         = src_len * 2 + 4096;
   size_t pos         = 0;
   char  *out         = (char*)malloc(cap);
   const char *p      = source;
   int    paren_depth = 0;
   int    brace_depth = 0;  /* track { } to know when we're at global scope */
   size_t last_func_start = 0;

   /* Track struct names for constructor replacement */
   const char *struct_names[64];
   size_t      struct_name_lens[64];
   unsigned    struct_count = 0;

   if (!out)
      return NULL;

   while (*p)
   {
      /* Skip #define lines entirely — don't apply any fixups to
       * preprocessor macro definitions. Check if we're at a line start. */
      {
         bool at_line_start = (p == source || p[-1] == '\n');
         if (at_line_start)
         {
            const char *d = p;
            while (*d == ' ' || *d == '\t') d++;
            if (*d == '#')
            {
               const char *dir = d + 1;
               while (*dir == ' ' || *dir == '\t') dir++;
               if (strncmp(dir, "define", 6) == 0
                     && (dir[6] == ' ' || dir[6] == '\t'))
               {
                  /* Find the macro name */
                  const char *mname = dir + 6;
                  const char *mname_end;
                  const char *eol;

                  while (*mname == ' ' || *mname == '\t') mname++;
                  mname_end = mname;
                  while (d3d9_hlsl_is_ident_char(*mname_end)) mname_end++;

                  /* Find end of line (with continuation) */
                  eol = p;
                  while (*eol)
                  {
                     const char *nl = strchr(eol, '\n');
                     if (!nl) { eol += strlen(eol); break; }
                     if (nl > eol && nl[-1] == '\\')
                     { eol = nl + 1; continue; }
                     eol = nl + 1;
                     break;
                  }

                  /* Check if this is COMPAT_IN_VERTEX or COMPAT_IN_FRAGMENT */
                  {
                     bool is_vertex = ((size_t)(mname_end - mname) == 16
                           && strncmp(mname, "COMPAT_IN_VERTEX", 16) == 0);
                     bool is_fragment = ((size_t)(mname_end - mname) == 18
                           && strncmp(mname, "COMPAT_IN_FRAGMENT", 18) == 0);

                  if (is_vertex || is_fragment)
                  {
                     /* Parse the macro value as a comma-separated param list.
                      * Split into params WITH semantics (kept in #define) and
                      * params WITHOUT (emitted as global declarations before). */
                     const char *val_start = mname_end;
                     while (*val_start == ' ' || *val_start == '\t') val_start++;

                     /* Collect params */
                     {
                        const char *vp = val_start;
                        const char *val_end = eol;
                        char globals_buf[2048];
                        char kept_buf[2048];
                        size_t gpos = 0, kpos = 0;

                        /* Trim trailing whitespace/newline from value */
                        while (val_end > val_start
                              && (val_end[-1] == '\n' || val_end[-1] == '\r'
                                 || val_end[-1] == ' ' || val_end[-1] == '\t'))
                           val_end--;

                        while (vp < val_end)
                        {
                           const char *param_start = vp;
                           const char *param_end;
                           bool has_colon = false;

                           /* Find end of this param */
                           while (vp < val_end && *vp != ',')
                           {
                              if (*vp == ':') has_colon = true;
                              vp++;
                           }
                           param_end = vp;
                           if (vp < val_end) vp++; /* skip comma */

                           /* Trim whitespace */
                           while (param_start < param_end
                                 && (*param_start == ' ' || *param_start == '\t'))
                              param_start++;
                           while (param_end > param_start
                                 && (param_end[-1] == ' ' || param_end[-1] == '\t'))
                              param_end--;

                           if (param_end <= param_start)
                              continue;

                           if (has_colon)
                           {
                              /* Already has semantic — keep as-is */
                              if (kpos > 0)
                              { kept_buf[kpos++] = ','; kept_buf[kpos++] = ' '; }
                              memcpy(kept_buf + kpos, param_start,
                                    (size_t)(param_end - param_start));
                              kpos += (size_t)(param_end - param_start);
                           }
                           else
                           {
                              /* No semantic. If 'uniform', extract as global.
                               * Otherwise (struct param like 'in_vertex VIN'),
                               * keep in #define but DO NOT add semantic —
                               * just extract as a global uniform too. */
                              const char *ds = param_start;
                              bool is_uniform = false;
                              if (strncmp(ds, "uniform", 7) == 0
                                    && !d3d9_hlsl_is_ident_char(ds[7]))
                              {
                                 is_uniform = true;
                                 ds += 7;
                                 while (*ds == ' ' || *ds == '\t') ds++;
                              }

                              /* Check has two tokens (type + name) */
                              {
                                 const char *tk = ds;
                                 while (tk < param_end && d3d9_hlsl_is_ident_char(*tk)) tk++;
                                 while (tk < param_end && (*tk == ' ' || *tk == '\t')) tk++;
                                 if (tk < param_end && d3d9_hlsl_is_ident_char(*tk))
                                 {
                                    if (is_uniform)
                                    {
                                       /* Uniform param — always extract as global */
                                       gpos += snprintf(globals_buf + gpos,
                                             sizeof(globals_buf) - gpos,
                                             "uniform %.*s;\n",
                                             (int)(param_end - ds), ds);
                                    }
                                    else if (is_vertex)
                                    {
                                       /* VS non-uniform struct param (in_vertex VIN)
                                        * — extract as global since the struct has
                                        * no meaningful semantics (just a dummy). */
                                       gpos += snprintf(globals_buf + gpos,
                                             sizeof(globals_buf) - gpos,
                                             "static %.*s;\n",
                                             (int)(param_end - ds), ds);
                                    }
                                    else
                                    {
                                       /* Non-uniform struct param (e.g. in_vertex VIN,
                                        * out_vertex VOUT) — keep in #define as-is.
                                        * These are vertex/rasterizer inputs with
                                        * per-member semantics. */
                                       if (kpos > 0)
                                       { kept_buf[kpos++] = ','; kept_buf[kpos++] = ' '; }
                                       memcpy(kept_buf + kpos, param_start,
                                             (size_t)(param_end - param_start));
                                       kpos += (size_t)(param_end - param_start);
                                    }
                                 }
                                 else
                                 {
                                    /* Single token (macro?) — keep in #define */
                                    if (kpos > 0)
                                    { kept_buf[kpos++] = ','; kept_buf[kpos++] = ' '; }
                                    memcpy(kept_buf + kpos, param_start,
                                          (size_t)(param_end - param_start));
                                    kpos += (size_t)(param_end - param_start);
                                 }
                              }
                           }
                        }
                        kept_buf[kpos] = '\0';
                        globals_buf[gpos] = '\0';

                        /* Output: globals first (with dedup), then rewritten #define */
                        if (gpos > 0)
                        {
                           /* Check each global line for duplicates before emitting */
                           char *gl = globals_buf;
                           while (*gl)
                           {
                              char *gl_end = strchr(gl, '\n');
                              size_t gl_len;
                              if (!gl_end) gl_end = gl + strlen(gl);
                              else gl_end++;
                              gl_len = (size_t)(gl_end - gl);

                              /* Check if this declaration exists in output */
                              out[pos] = '\0';
                              if (!strstr(out, gl))
                                 d3d9_hlsl_buf_append(&out, &pos, &cap, gl, gl_len);

                              gl = gl_end;
                           }
                        }

                        {
                           char def_line[2048];
                           size_t dl = snprintf(def_line, sizeof(def_line),
                                 "#define %.*s %s\n",
                                 (int)(mname_end - mname), mname,
                                 kept_buf);
                           d3d9_hlsl_buf_append(&out, &pos, &cap,
                                 def_line, dl);
                        }
                     }

                     p = eol;
                     continue;
                  }
                  } /* is_vertex || is_fragment */

                  /* Regular #define — copy verbatim */
                  {
                     size_t ll = (size_t)(eol - p);
                     /* Copy #define line, but apply texture -> _texture rename */
                     {
                        const char *dp = p;
                        const char *de = p + ll;
                        while (dp < de)
                        {
                           if (strncmp(dp, "texture", 7) == 0
                                 && (dp == p || !d3d9_hlsl_is_ident_char(dp[-1]))
                                 && !d3d9_hlsl_is_ident_char(dp[7]))
                           {
                              if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "_texture", 8))
                                 goto fail;
                              dp += 7;
                           }
                           else
                           {
                              if (!d3d9_hlsl_buf_append(&out, &pos, &cap, dp, 1))
                                 goto fail;
                              dp++;
                           }
                        }
                     }
                     p = eol;
                     continue;
                  }
               }
            }
         }
      }

      /* --- Track struct declarations for constructor replacement --- */
      if (brace_depth == 0 && paren_depth == 0
            && strncmp(p, "struct", 6) == 0
            && (p == source || !d3d9_hlsl_is_ident_char(p[-1]))
            && !d3d9_hlsl_is_ident_char(p[6]))
      {
         const char *nm = p + 6;
         while (*nm == ' ' || *nm == '\t' || *nm == '\r' || *nm == '\n')
            nm++;
         if (d3d9_hlsl_is_ident_char(*nm))
         {
            const char *nm_end = nm;
            while (d3d9_hlsl_is_ident_char(*nm_end)) nm_end++;
            if (struct_count < 64)
            {
               struct_names[struct_count] = nm;
               struct_name_lens[struct_count] = (size_t)(nm_end - nm);
               struct_count++;
            }
         }
         /* Don't consume — fall through to normal output */
      }

      /* --- Fix 1: whole-word 'texture' -> '_texture' --- */
      if (strncmp(p, "texture", 7) == 0
            && (p == source || !d3d9_hlsl_is_ident_char(p[-1]))
            && !d3d9_hlsl_is_ident_char(p[7]))
      {
         if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "_texture", 8))
            goto fail;
         p += 7;
         continue;
      }

      /* --- Fix 1e: whole-word 'mod' -> 'fmod' ---
       * Cg has mod(x,y), HLSL uses fmod(x,y). */
      if (strncmp(p, "mod", 3) == 0
            && (p == source || !d3d9_hlsl_is_ident_char(p[-1]))
            && !d3d9_hlsl_is_ident_char(p[3]))
      {
         if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "fmod", 4))
            goto fail;
         p += 3;
         continue;
      }

      /* --- Fix 1f: standalone 'sampler' -> 'sampler2D' ---
       * Cg uses 'sampler' as a generic type interchangeable with sampler2D.
       * HLSL treats 'sampler' as SamplerState which is incompatible.
       * Only match 'sampler' NOT followed by 1D/2D/3D/CUBE. */
      if (strncmp(p, "sampler", 7) == 0
            && (p == source || !d3d9_hlsl_is_ident_char(p[-1]))
            && !d3d9_hlsl_is_ident_char(p[7])
            && strncmp(p + 7, "2D", 2) != 0
            && strncmp(p + 7, "1D", 2) != 0
            && strncmp(p + 7, "3D", 2) != 0
            && strncmp(p + 7, "CUBE", 4) != 0)
      {
         if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "sampler2D", 9))
            goto fail;
         p += 7;
         continue;
      }

      /* --- Fix 1g: whole-word 'fract' -> 'frac' ---
       * Cg has fract(x), HLSL uses frac(x). */
      if (strncmp(p, "fract", 5) == 0
            && (p == source || !d3d9_hlsl_is_ident_char(p[-1]))
            && !d3d9_hlsl_is_ident_char(p[5]))
      {
         if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "frac", 4))
            goto fail;
         p += 5;
         continue;
      }

      /* --- Fix 1h: GLSL type renames ---
       * Some Cg shaders use GLSL types. */
      if ((p == source || !d3d9_hlsl_is_ident_char(p[-1])))
      {
         /* vec4 -> float4 */
         if (strncmp(p, "vec4", 4) == 0 && !d3d9_hlsl_is_ident_char(p[4]))
         { if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "float4", 6)) goto fail; p += 4; continue; }
         /* vec3 -> float3 */
         if (strncmp(p, "vec3", 4) == 0 && !d3d9_hlsl_is_ident_char(p[4]))
         { if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "float3", 6)) goto fail; p += 4; continue; }
         /* vec2 -> float2 */
         if (strncmp(p, "vec2", 4) == 0 && !d3d9_hlsl_is_ident_char(p[4]))
         { if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "float2", 6)) goto fail; p += 4; continue; }
         /* mat4 -> float4x4 */
         if (strncmp(p, "mat4", 4) == 0 && !d3d9_hlsl_is_ident_char(p[4]))
         { if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "float4x4", 8)) goto fail; p += 4; continue; }
         /* mat3 -> float3x3 */
         if (strncmp(p, "mat3", 4) == 0 && !d3d9_hlsl_is_ident_char(p[4]))
         { if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "float3x3", 8)) goto fail; p += 4; continue; }
         /* mix -> lerp */
         if (strncmp(p, "mix", 3) == 0 && !d3d9_hlsl_is_ident_char(p[3]))
         { if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "lerp", 4)) goto fail; p += 3; continue; }
      }

      /* --- Fix 1a: 'const' -> 'static const' at global scope ---
       * D3DCompile with backwards compat treats global 'const' as a CTAB
       * uniform instead of embedding the literal value. 'static const'
       * forces the value to be embedded in the bytecode. */
      if (brace_depth == 0 && paren_depth == 0
            && strncmp(p, "const", 5) == 0
            && (p == source || !d3d9_hlsl_is_ident_char(p[-1]))
            && !d3d9_hlsl_is_ident_char(p[5]))
      {
         /* Check it's not already 'static const' */
         bool already_static = false;
         if (p >= source + 7 && strncmp(p - 7, "static ", 7) == 0)
            already_static = true;
         /* Check for 'const static' — swap to 'static const' */
         {
            const char *after = p + 5;
            while (*after == ' ' || *after == '\t') after++;
            if (strncmp(after, "static", 6) == 0
                  && !d3d9_hlsl_is_ident_char(after[6]))
            {
               /* Emit 'static const' and skip past 'const static' */
               if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "static const", 12))
                  goto fail;
               p = after + 6;
               continue;
            }
         }
         if (!already_static)
         {
            if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "static const", 12))
               goto fail;
            p += 5;
            continue;
         }
      }

      /* --- Fix 1a2: add 'static' to global float/int declarations with initializers ---
       * D3DCompile treats global 'float x = val;' as a CTAB uniform,
       * ignoring the initializer. 'static float x = val;' embeds it.
       * Only at global scope, not preceded by 'uniform' or 'static'. */
      if (brace_depth == 0 && paren_depth == 0
            && (strncmp(p, "float", 5) == 0 || strncmp(p, "int", 3) == 0)
            && (p == source || !d3d9_hlsl_is_ident_char(p[-1])))
      {
         /* Determine the type keyword length */
         size_t tlen = 0;
         if (strncmp(p, "float4x4", 8) == 0 && !d3d9_hlsl_is_ident_char(p[8]))
            tlen = 8;
         else if (strncmp(p, "float4", 6) == 0 && !d3d9_hlsl_is_ident_char(p[6]))
            tlen = 6;
         else if (strncmp(p, "float3x3", 8) == 0 && !d3d9_hlsl_is_ident_char(p[8]))
            tlen = 8;
         else if (strncmp(p, "float3", 6) == 0 && !d3d9_hlsl_is_ident_char(p[6]))
            tlen = 6;
         else if (strncmp(p, "float2", 6) == 0 && !d3d9_hlsl_is_ident_char(p[6]))
            tlen = 6;
         else if (strncmp(p, "float", 5) == 0 && !d3d9_hlsl_is_ident_char(p[5]))
            tlen = 5;
         else if (strncmp(p, "int", 3) == 0 && !d3d9_hlsl_is_ident_char(p[3]))
            tlen = 3;

         if (tlen > 0)
         {
            /* Check: skip whitespace, identifier, skip whitespace, '=' */
            const char *tk = p + tlen;
            while (*tk == ' ' || *tk == '\t') tk++;
            if (d3d9_hlsl_is_ident_char(*tk))
            {
               while (d3d9_hlsl_is_ident_char(*tk)) tk++;
               while (*tk == ' ' || *tk == '\t') tk++;
               if (*tk == '=' && tk[1] != '=') /* '=' but not '==' */
               {
                  /* Check not already preceded by 'static' or 'uniform' */
                  bool skip = false;
                  if (p >= source + 7 && strncmp(p - 7, "static ", 7) == 0)
                     skip = true;
                  if (p >= source + 8 && strncmp(p - 8, "uniform ", 8) == 0)
                     skip = true;
                  /* Also check output buffer for 'static ' or 'uniform ' */
                  if (!skip && pos >= 7 && strncmp(out + pos - 7, "static ", 7) == 0)
                     skip = true;
                  if (!skip && pos >= 8 && strncmp(out + pos - 8, "uniform ", 8) == 0)
                     skip = true;
                  if (!skip && pos >= 6 && strncmp(out + pos - 6, "const ", 6) == 0)
                     skip = true;

                  /* Skip if this variable is a shader parameter declared
                   * via '#pragma parameter'.  Making it static would bake
                   * the default value into the bytecode and prevent the
                   * menu from adjusting it at runtime. */
                  if (!skip)
                  {
                     const char *vn = p + tlen;
                     const char *vn_end;
                     while (*vn == ' ' || *vn == '\t') vn++;
                     vn_end = vn;
                     while (d3d9_hlsl_is_ident_char(*vn_end)) vn_end++;
                     if (vn_end > vn)
                     {
                        size_t vn_len = (size_t)(vn_end - vn);
                        const char *sp = source;
                        while ((sp = strstr(sp, "#pragma parameter")) != NULL)
                        {
                           const char *pp = sp + 17; /* strlen("#pragma parameter") */
                           while (*pp == ' ' || *pp == '\t') pp++;
                           if (strncmp(pp, vn, vn_len) == 0
                                 && !d3d9_hlsl_is_ident_char(pp[vn_len]))
                           {
                              skip = true;
                              break;
                           }
                           sp = pp;
                        }
                     }
                  }

                  if (!skip)
                  {
                     if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "static ", 7))
                        goto fail;
                     /* Fall through to copy the type keyword normally */
                  }
               }
            }
         }
      }

      /* --- Fix 1b: strip 'uniform' from struct member declarations ---
       * Cg allows 'uniform sampler2D tex' inside structs; HLSL doesn't. */
      if (brace_depth > 0 && paren_depth == 0
            && strncmp(p, "uniform", 7) == 0
            && (p == source || !d3d9_hlsl_is_ident_char(p[-1]))
            && !d3d9_hlsl_is_ident_char(p[7]))
      {
         /* Skip the 'uniform' keyword and trailing whitespace */
         p += 7;
         while (*p == ' ' || *p == '\t') p++;
         continue;
      }

      /* --- Fix 1c: replace Cg struct constructors ---
       * Cg: coords = tex_coords(a, b, c);
       * HLSL: { tex_coords _sc = {a, b, c}; coords = _sc; }
       *
       * Detect: inside function body, identifier matching known struct
       * name followed by '(', preceded by '=' in the output. */
      if (brace_depth > 0 && paren_depth == 0
            && d3d9_hlsl_is_ident_char(*p))
      {
         unsigned si;
         for (si = 0; si < struct_count; si++)
         {
            size_t slen = struct_name_lens[si];
            if (strncmp(p, struct_names[si], slen) == 0
                  && !d3d9_hlsl_is_ident_char(p[slen])
                  && (p == source || !d3d9_hlsl_is_ident_char(p[-1])))
            {
               /* Check if followed by '(' (with optional whitespace) */
               const char *after_name = p + slen;
               while (*after_name == ' ' || *after_name == '\t')
                  after_name++;

               if (*after_name == '(')
               {
                  /* Check if preceded by '=' in the output (assignment context) */
                  size_t bi = pos;
                  while (bi > 0 && (out[bi-1] == ' ' || out[bi-1] == '\t'))
                     bi--;
                  if (bi > 0 && out[bi-1] == '=')
                  {
                     /* Find the matching ')' */
                     const char *args_start = after_name + 1;
                     const char *args_end   = args_start;
                     int depth = 1;
                     while (*args_end && depth > 0)
                     {
                        if (*args_end == '(') depth++;
                        else if (*args_end == ')') depth--;
                        if (depth > 0) args_end++;
                     }

                     if (depth == 0)
                     {
                        /* Find the variable name before '=' by scanning
                         * backwards in the output past '=' and whitespace */
                        size_t var_end = bi - 1; /* before '=' */
                        size_t var_start;
                        char var_name[256];
                        size_t var_len;
                        char replacement[4096];
                        size_t rlen;

                        while (var_end > 0 && (out[var_end-1] == ' '
                                 || out[var_end-1] == '\t'))
                           var_end--;
                        var_start = var_end;
                        while (var_start > 0
                              && (d3d9_hlsl_is_ident_char(out[var_start-1])
                                 || out[var_start-1] == '.'))
                           var_start--;

                        var_len = var_end - var_start;
                        if (var_len > 0 && var_len < sizeof(var_name))
                        {
                           size_t args_len = (size_t)(args_end - args_start);

                           memcpy(var_name, out + var_start, var_len);
                           var_name[var_len] = '\0';

                           /* Build: { structname _sc = {args}; var = _sc; }
                            * But we need to replace the "var =" already in the output.
                            * Rewind output to var_start and write the replacement. */
                           rlen = snprintf(replacement, sizeof(replacement),
                                 "{ %.*s _sc = {%.*s}; %s = _sc; }",
                                 (int)slen, struct_names[si],
                                 (int)args_len, args_start,
                                 var_name);

                           /* Rewind output to var_start */
                           pos = var_start;

                           if (!d3d9_hlsl_buf_append(&out, &pos, &cap,
                                    replacement, rlen))
                              goto fail;

                           /* Skip past ')' in source, and the ';' if present */
                           p = args_end + 1; /* skip ')' */
                           while (*p == ' ' || *p == '\t') p++;
                           if (*p == ';') p++; /* skip ';' */

                           goto struct_ctor_done;
                        }
                     }
                  }
               }
            }
         }
struct_ctor_done:
         if (si < struct_count)
            continue; /* constructor was replaced */
      }

      /* --- Fix 1d: expand float3/4(scalar) to float3/4(s,s,...) ---
       * Cg supports float3(x) as a splat constructor. HLSL requires
       * the correct number of arguments.
       * Note: float2(expr) is NOT expanded because it's usually
       * float2(float2_expr) which is a valid HLSL cast. */
      if (strncmp(p, "float", 5) == 0
            && (p[5] >= '3' && p[5] <= '4')
            && p[6] == '('
            && (p == source || !d3d9_hlsl_is_ident_char(p[-1])))
      {
         int n_components = p[5] - '0';
         /* Check if this is a single-argument call (no comma at depth 0) */
         const char *args_start = p + 7;
         const char *scan = args_start;
         int depth = 1;
         bool has_comma = false;
         while (*scan && depth > 0)
         {
            if (*scan == '(') depth++;
            else if (*scan == ')') { if (--depth == 0) break; }
            else if (*scan == ',' && depth == 1) { has_comma = true; break; }
            scan++;
         }

         if (!has_comma && depth == 0 && scan > args_start)
         {
            /* Single argument — Cg splat constructor float4(x) → all components.
             * Use a cast (float4)(x) which handles both scalar and bool args
             * correctly in HLSL. */
            size_t arg_len = (size_t)(scan - args_start);
            char arg[512];
            if (arg_len < sizeof(arg))
            {
               char cast_buf[32];
               size_t cast_len;
               memcpy(arg, args_start, arg_len);
               arg[arg_len] = '\0';

               /* Emit ((floatN)(arg)) */
               cast_len = snprintf(cast_buf, sizeof(cast_buf),
                     "((float%d)(", n_components);
               if (!d3d9_hlsl_buf_append(&out, &pos, &cap, cast_buf, cast_len))
                  goto fail;
               if (!d3d9_hlsl_buf_append(&out, &pos, &cap, arg, arg_len))
                  goto fail;
               if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "))", 2))
                  goto fail;

               p = scan + 1; /* skip past ')' */
               continue;
            }
         }

         /* Check for floatN(X, X, X, X) where all args are identical.
          * Cg allows float4(float4_val, float4_val, ...) taking first
          * N components. HLSL does not. Replace with just X. */
         if (has_comma)
         {
            /* Re-scan to collect all arguments */
            const char *a_start[4];
            size_t a_len[4];
            int a_count = 0;
            const char *as = args_start;
            int d2 = 1;
            bool all_same = true;

            a_start[0] = as;
            while (*as && d2 > 0 && a_count < 4)
            {
               if (*as == '(') d2++;
               else if (*as == ')') { if (--d2 == 0) break; }
               else if (*as == ',' && d2 == 1)
               {
                  /* Trim whitespace from current arg */
                  const char *ae = as;
                  while (ae > a_start[a_count]
                        && (ae[-1] == ' ' || ae[-1] == '\t'))
                     ae--;
                  while (a_start[a_count] < ae
                        && (*a_start[a_count] == ' '
                           || *a_start[a_count] == '\t'))
                     a_start[a_count]++;
                  a_len[a_count] = (size_t)(ae - a_start[a_count]);
                  a_count++;
                  if (a_count < 4)
                     a_start[a_count] = as + 1;
               }
               as++;
            }
            /* Last argument */
            if (d2 == 0 && a_count < 4)
            {
               const char *ae = as;
               while (ae > a_start[a_count]
                     && (ae[-1] == ' ' || ae[-1] == '\t'))
                  ae--;
               while (a_start[a_count] < ae
                     && (*a_start[a_count] == ' '
                        || *a_start[a_count] == '\t'))
                  a_start[a_count]++;
               a_len[a_count] = (size_t)(ae - a_start[a_count]);
               a_count++;
            }

            if (a_count == n_components && a_count >= 3)
            {
               int ai;
               for (ai = 1; ai < a_count; ai++)
               {
                  if (a_len[ai] != a_len[0]
                        || strncmp(a_start[0], a_start[ai], a_len[0]) != 0)
                  { all_same = false; break; }
               }

               if (all_same && a_len[0] > 0
                     && d3d9_hlsl_is_ident_char(a_start[0][0])
                     && !(a_start[0][0] >= '0' && a_start[0][0] <= '9'))
               {
                  if (!d3d9_hlsl_buf_append(&out, &pos, &cap, "(", 1))
                     goto fail;
                  if (!d3d9_hlsl_buf_append(&out, &pos, &cap,
                           a_start[0], a_len[0]))
                     goto fail;
                  if (!d3d9_hlsl_buf_append(&out, &pos, &cap, ")", 1))
                     goto fail;
                  p = as + 1; /* skip past ')' */
                  continue;
               }
            }
         }
      }

      /* Track braces for scope detection */
      if (*p == '{')
      {
         brace_depth++;
         if (!d3d9_hlsl_buf_append(&out, &pos, &cap, p, 1))
            goto fail;
         p++;
         continue;
      }
      if (*p == '}')
      {
         if (brace_depth > 0) brace_depth--;
         if (!d3d9_hlsl_buf_append(&out, &pos, &cap, p, 1))
            goto fail;
         p++;
         continue;
      }

      /* Track paren depth — only for function definitions, not macro calls.
       * A function definition '(' is preceded by the function name identifier
       * and the line looks like: <type> <name>( ... )
       * We detect this by checking that:
       *  - we're at paren_depth 0
       *  - the '(' is preceded by an identifier (function name)
       *  - the line does NOT start with '#' (not a preprocessor line)
       *  - the line does NOT start with 'uniform' (not a uniform declaration) */
      if (*p == '(')
      {
         if (paren_depth == 0 && brace_depth == 0)
         {
            /* Check if this looks like a function definition */
            bool is_func_sig = false;

            /* Look at the line start to rule out preprocessor/uniform lines */
            {
               size_t ls = pos;
               while (ls > 0 && out[ls - 1] != '\n')
                  ls--;
               /* Skip whitespace at line start */
               {
                  size_t cs = ls;
                  while (cs < pos && (out[cs] == ' ' || out[cs] == '\t'))
                     cs++;
                  /* Not a preprocessor line and not 'uniform' */
                  if (cs < pos && out[cs] != '#'
                        && strncmp(out + cs, "uniform", 7) != 0)
                  {
                     /* Check that '(' is preceded by an identifier
                      * AND the function is main_vertex or main_fragment
                      * (don't extract params from helper functions).
                      * Scan backward past whitespace/newlines since the
                      * name may be on a previous line: main_vertex\n  ( */
                     {
                        size_t ne = pos;
                        while (ne > 0 && (out[ne - 1] == ' '
                              || out[ne - 1] == '\t'
                              || out[ne - 1] == '\r'
                              || out[ne - 1] == '\n'))
                           ne--;
                        if (ne > 0 && d3d9_hlsl_is_ident_char(out[ne - 1]))
                        {
                           if ((ne >= 11 && strncmp(out + ne - 11, "main_vertex", 11) == 0
                                    && (ne == 11 || !d3d9_hlsl_is_ident_char(out[ne - 12])))
                                 || (ne >= 13 && strncmp(out + ne - 13, "main_fragment", 13) == 0
                                    && (ne == 13 || !d3d9_hlsl_is_ident_char(out[ne - 14]))))
                              is_func_sig = true;
                        }
                     }
                  }
               }
               last_func_start = ls;
            }

            if (is_func_sig)
               paren_depth++;
            /* else: don't increment paren_depth for macro calls */
         }
         else
            paren_depth++;

         if (!d3d9_hlsl_buf_append(&out, &pos, &cap, p, 1))
            goto fail;
         p++;
         continue;
      }

      if (*p == ')')
      {
         if (paren_depth > 0)
            paren_depth--;
         if (!d3d9_hlsl_buf_append(&out, &pos, &cap, p, 1))
            goto fail;
         p++;
         continue;
      }

      /* --- Fix 2: remove params without semantics from function sigs ---
       *
       * Catches both 'uniform <type> <name>' and plain '<type> <name>'
       * where <name> has no ': SEMANTIC' annotation. These are Cg-style
       * runtime parameters that can't be vertex-stream inputs in HLSL.
       *
       * We detect: inside parens, after '(' or ',', skip whitespace,
       * if we see 'uniform' consume it, then look for '<ident> <ident>'
       * NOT followed by ':'. */
      if (paren_depth > 0 && brace_depth == 0)
      {
         /* Check if we're at the start of a parameter (after '(' or ',') */
         bool at_param_start = false;
         {
            /* Scan backwards in output for the last non-whitespace char */
            size_t si = pos;
            while (si > 0 && (out[si-1] == ' ' || out[si-1] == '\t'
                        || out[si-1] == '\r' || out[si-1] == '\n'))
               si--;
            if (si > 0 && (out[si-1] == '(' || out[si-1] == ','))
               at_param_start = true;
         }

         if (at_param_start && d3d9_hlsl_is_ident_char(*p))
         {
            /* We're at the start of a parameter. Scan ahead to determine
             * if this is a '<type> <name>' without ': SEMANTIC'.
             * 
             * Find the end of this param (next ',' or ')' at depth 0) */
            const char *param_start = p;
            const char *param_end   = p;
            bool has_semantic       = false;
            {
               int d = 0;
               const char *s = p;
               while (*s)
               {
                  if (*s == '(') d++;
                  else if (*s == ')')
                  { if (d == 0) break; d--; }
                  else if (*s == ',' && d == 0)
                     break;
                  else if (*s == ':' && d == 0)
                     has_semantic = true;
                  s++;
               }
               param_end = s;
            }

            if (!has_semantic)
            {
               /* This param has no semantic — it's a Cg uniform-style param.
                * Extract it, optionally strip leading 'uniform', hoist to global.
                *
                * IMPORTANT: only extract if the declaration has at least
                * two whitespace-separated tokens (type + name). A single
                * identifier is likely a macro (e.g. COMPAT_IN_VERTEX)
                * that D3DCompile will expand. */
               const char *decl_start = param_start;
               const char *decl_end   = param_end;
               const char *trim;
               size_t decl_len;
               char decl_buf[512];
               char hoist[540];
               size_t hoist_len;
               bool already_global;
               bool has_two_tokens = false;

               /* Check and skip 'uniform' keyword */
               if (strncmp(decl_start, "uniform", 7) == 0
                     && !d3d9_hlsl_is_ident_char(decl_start[7]))
               {
                  decl_start += 7;
                  while (*decl_start == ' ' || *decl_start == '\t'
                        || *decl_start == '\r' || *decl_start == '\n')
                     decl_start++;
               }

               /* Skip 'in', 'out', 'inout' parameter qualifiers (Cg-specific).
                * If present, this param is an interpolant, not a uniform —
                * keep it in the function signature, don't extract. */
               if (strncmp(decl_start, "inout", 5) == 0
                     && !d3d9_hlsl_is_ident_char(decl_start[5]))
               {
                  goto skip_extraction;
               }
               else if (strncmp(decl_start, "in", 2) == 0
                     && !d3d9_hlsl_is_ident_char(decl_start[2]))
               {
                  goto skip_extraction;
               }
               else if (strncmp(decl_start, "out", 3) == 0
                     && !d3d9_hlsl_is_ident_char(decl_start[3]))
               {
                  goto skip_extraction;
               }

               /* Check for at least two tokens: skip first ident, skip space,
                * check for second ident */
               {
                  const char *tk = decl_start;
                  /* Skip first identifier */
                  while (tk < decl_end && d3d9_hlsl_is_ident_char(*tk))
                     tk++;
                  /* Skip whitespace */
                  while (tk < decl_end && (*tk == ' ' || *tk == '\t'
                           || *tk == '\r' || *tk == '\n'))
                     tk++;
                  /* Check for second identifier */
                  if (tk < decl_end && d3d9_hlsl_is_ident_char(*tk))
                     has_two_tokens = true;
               }

               if (!has_two_tokens)
                  goto skip_extraction;

               /* Trim trailing whitespace */
               trim = decl_end - 1;
               while (trim > decl_start
                     && (*trim == ' ' || *trim == '\t'
                        || *trim == '\r' || *trim == '\n'))
                  trim--;
               decl_len = (size_t)(trim - decl_start + 1);

               if (decl_len > 0 && decl_len < sizeof(decl_buf) - 1)
               {
                  memcpy(decl_buf, decl_start, decl_len);
                  decl_buf[decl_len] = '\0';

                  hoist_len = snprintf(hoist, sizeof(hoist),
                        "uniform %s;\n", decl_buf);

                  /* Check if already declared globally */
                  out[pos] = '\0';
                  {
                     char decl_semi[520];
                     snprintf(decl_semi, sizeof(decl_semi), "%s;", decl_buf);
                     already_global = (strstr(out, hoist) != NULL)
                                   || (strstr(out, decl_semi) != NULL);
                  }

                  if (!already_global)
                  {
                     while (pos + hoist_len + 1 >= cap)
                     { cap *= 2; out = (char*)realloc(out, cap);
                       if (!out) return NULL; }
                     memmove(out + last_func_start + hoist_len,
                           out + last_func_start, pos - last_func_start);
                     memcpy(out + last_func_start, hoist, hoist_len);
                     pos += hoist_len;
                     last_func_start += hoist_len;
                  }

                  /* Remove param from source, handle commas */
                  {
                     const char *after = decl_end;
                     if (*after == ',')
                     {
                        after++;
                        while (*after == ' ' || *after == '\t'
                              || *after == '\r' || *after == '\n')
                           after++;
                     }
                     else if (*after == ')')
                     {
                        while (pos > 0 && (out[pos-1] == ' '
                                 || out[pos-1] == '\t'
                                 || out[pos-1] == '\r'
                                 || out[pos-1] == '\n'))
                           pos--;
                        if (pos > 0 && out[pos-1] == ',')
                           pos--;
                     }
                     p = after;
                  }
                  continue;
               }
            }
skip_extraction:
            ; /* label requires a statement */
         }
      }

      /* Regular char */
      if (!d3d9_hlsl_buf_append(&out, &pos, &cap, p, 1))
         goto fail;
      p++;
   }

   out[pos] = '\0';
   return out;

fail:
   free(out);
   return NULL;
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

/* Dynamically load D3DCompile and D3DPreprocess with
 * D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY to accept Cg syntax. */

typedef HRESULT (WINAPI *PFN_D3DCOMPILE)(
      LPCVOID, SIZE_T, LPCSTR,
      const void*, void*,
      LPCSTR, LPCSTR, UINT, UINT,
      void**, void**);

typedef HRESULT (WINAPI *PFN_D3DPREPROCESS)(
      LPCVOID, SIZE_T, LPCSTR,
      const void*, void*,
      void**, void**);

#ifndef D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY
#define D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY (1 << 12)
#endif

#ifndef D3DCOMPILE_OPTIMIZATION_LEVEL3
#define D3DCOMPILE_OPTIMIZATION_LEVEL3 (1 << 15)
#endif

#ifndef D3DCOMPILE_PREFER_FLOW_CONTROL
#define D3DCOMPILE_PREFER_FLOW_CONTROL (1 << 10)
#endif

static PFN_D3DCOMPILE    d3d9_pCompile    = NULL;
static PFN_D3DPREPROCESS d3d9_pPreprocess = NULL;
static bool d3d9_compiler_tried           = false;

static void d3d9_hlsl_ensure_compiler(void)
{
   HMODULE dll = NULL;
   const char *names[] = {
      "d3dcompiler_47.dll",
      "d3dcompiler_46.dll",
      "d3dcompiler_43.dll",
      NULL
   };
   int i;

   if (d3d9_compiler_tried)
      return;
   d3d9_compiler_tried = true;

   for (i = 0; names[i]; i++)
   {
      dll = GetModuleHandleA(names[i]);
      if (!dll)
         dll = LoadLibraryA(names[i]);
      if (dll)
      {
         d3d9_pCompile    = (PFN_D3DCOMPILE)GetProcAddress(dll, "D3DCompile");
         d3d9_pPreprocess = (PFN_D3DPREPROCESS)GetProcAddress(dll, "D3DPreprocess");
         if (d3d9_pCompile)
         {
            RARCH_LOG("[D3D9 HLSL] Loaded D3DCompile from %s.\n", names[i]);
            break;
         }
      }
   }
   if (!d3d9_pCompile)
      RARCH_WARN("[D3D9 HLSL] Could not find D3DCompile, falling back.\n");
}

/* Full Cg→HLSL compilation pipeline:
 *  1. D3DPreprocess to expand all #define macros
 *  2. d3d9_hlsl_fixup_cg_source to fix texture keyword + uniform params
 *  3. D3DCompile with backwards compat flag
 *
 * If D3DPreprocess is unavailable, skips step 1 and tries fixup on the
 * raw source (works for shaders that don't use macros for uniform params). */
static bool d3d9_hlsl_compile_cg_compat(
      const char *source, size_t source_len,
      const char *source_name,
      const char *entry, const char *target,
      D3DBlob *out_blob)
{
   D3DBlob error_blob = NULL;
   HRESULT hr;

   /* D3D_SHADER_MACRO compatible: { Name, Definition }, null-terminated */
   struct { const char *n; const char *d; } cg_defines[] = {
      { "CG", "1" },
      { NULL, NULL }
   };

   if (!out_blob)
      return false;

   d3d9_hlsl_ensure_compiler();

   if (!d3d9_pCompile)
      return d3d_compile(source, source_len, source_name,
            entry, target, out_blob);

   /* Our own preprocessor already handled #include, #ifdef, #ifndef,
    * #else, #endif.  The caller (load_program_from_file_ex) also applied
    * Cg→HLSL fixups, struct semantic insertion, and sampler decomposition,
    * so we compile directly without further source transformations. */
   {
      hr = d3d9_pCompile(
            source, source_len,
            source_name,
            NULL, NULL,
            entry, target,
            D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY,
            0,
            (void**)out_blob,
            (void**)&error_blob);

      if (FAILED(hr))
      {
         /* First attempt failed — retry with maximum optimization and
          * flow-control preference.  Complex Cg shaders with many unrolled
          * texture fetches (e.g. 29-tap FIR filters) can exceed SM3.0's
          * temp register limit at default optimization.  Level 3 + flow
          * control lets the compiler use loops and reduce register pressure. */
         if (error_blob)
         {
            error_blob->lpVtbl->Release(error_blob);
            error_blob = NULL;
         }
         if (*out_blob)
         {
            (*out_blob)->lpVtbl->Release(*out_blob);
            *out_blob = NULL;
         }

         hr = d3d9_pCompile(
               source, source_len,
               source_name,
               NULL, NULL,
               entry, target,
               D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY
               | D3DCOMPILE_OPTIMIZATION_LEVEL3
               | D3DCOMPILE_PREFER_FLOW_CONTROL,
               0,
               (void**)out_blob,
               (void**)&error_blob);
      }

      if (FAILED(hr))
      {
         if (error_blob)
         {
            const char *err_str = (const char*)error_blob->lpVtbl->GetBufferPointer(error_blob);
            RARCH_ERR("[D3D9 HLSL] D3DCompile failed: %s\n", err_str);
            error_blob->lpVtbl->Release(error_blob);
         }
         return false;
      }

      if (error_blob)
         error_blob->lpVtbl->Release(error_blob);
      return true;
   }
}

/* Ensure all members of the vertex shader output struct are initialized.
 *
 * D3DCompile requires every member of the return struct to be written.
 * Cg allows partial initialization.  This pass finds `return OUT;` in
 * main_vertex and inserts `OUT.member = 0;` for any member not assigned
 * elsewhere in the function body.
 *
 * Returns a malloc'd modified copy, or NULL if no changes were made. */
static char *d3d9_hlsl_init_vs_output_members(const char *source)
{
   /* Step 1: find 'main_vertex' function and its return type */
   const char *mv = strstr(source, "main_vertex");
   const char *ret_type_start, *ret_type_end;
   char ret_type[128];
   size_t ret_type_len;
   const char *func_body_start, *func_body_end;
   const char *struct_start, *struct_body_start, *struct_body_end;

   /* Struct member info */
   struct { const char *name; size_t len; } members[32];
   unsigned member_count = 0;
   unsigned unassigned_count = 0;
   char unassigned_buf[2048];
   size_t upos = 0;

   if (!mv) return NULL;

   /* Find return type: scan backwards from 'main_vertex' past whitespace */
   ret_type_end = mv;
   while (ret_type_end > source
         && (ret_type_end[-1] == ' ' || ret_type_end[-1] == '\t'
            || ret_type_end[-1] == '\n' || ret_type_end[-1] == '\r'))
      ret_type_end--;
   ret_type_start = ret_type_end;
   while (ret_type_start > source && d3d9_hlsl_is_ident_char(ret_type_start[-1]))
      ret_type_start--;

   ret_type_len = (size_t)(ret_type_end - ret_type_start);
   if (ret_type_len == 0 || ret_type_len >= sizeof(ret_type))
      return NULL;
   memcpy(ret_type, ret_type_start, ret_type_len);
   ret_type[ret_type_len] = '\0';

   /* Step 2: find the struct definition for the return type */
   {
      char pattern[140];
      snprintf(pattern, sizeof(pattern), "struct %s", ret_type);
      struct_start = strstr(source, pattern);
      if (!struct_start) return NULL;
      /* Verify it's a whole word match */
      {
         const char *after = struct_start + strlen(pattern);
         if (d3d9_hlsl_is_ident_char(*after))
            return NULL;
      }
   }

   struct_body_start = strchr(struct_start, '{');
   if (!struct_body_start) return NULL;
   struct_body_start++;

   /* Find matching '}' */
   {
      int depth = 1;
      const char *s = struct_body_start;
      while (*s && depth > 0)
      {
         if (*s == '{') depth++;
         else if (*s == '}') depth--;
         s++;
      }
      struct_body_end = s - 1;
   }

   /* Step 3: parse struct members — extract names */
   {
      const char *mp = struct_body_start;
      while (mp < struct_body_end)
      {
         /* Find each ';' — the member name is the identifier before
          * the optional ': SEMANTIC' before the ';' */
         const char *semi = strchr(mp, ';');
         if (!semi || semi >= struct_body_end) break;

         /* Scan backwards from ';' past semantic if present */
         {
            const char *end = semi;
            const char *name_end, *name_start;

            /* Skip past ': SEMANTIC' */
            {
               const char *colon = NULL;
               const char *scan = mp;
               while (scan < semi)
               {
                  if (*scan == ':') colon = scan;
                  scan++;
               }
               if (colon) end = colon;
            }

            /* Skip trailing whitespace */
            while (end > mp && (end[-1] == ' ' || end[-1] == '\t')) end--;
            name_end = end;

            /* Scan backwards to find start of identifier */
            name_start = name_end;
            while (name_start > mp && d3d9_hlsl_is_ident_char(name_start[-1]))
               name_start--;

            if (name_end > name_start && member_count < 32)
            {
               members[member_count].name = name_start;
               members[member_count].len = (size_t)(name_end - name_start);
               member_count++;
            }
         }

         mp = semi + 1;
      }
   }

   if (member_count == 0) return NULL;

   /* Step 4: find main_vertex function body */
   {
      const char *ob = mv;
      int depth;
      while (*ob && *ob != '{') ob++;
      if (!*ob) return NULL;
      func_body_start = ob + 1;
      depth = 1;
      {
         const char *s = func_body_start;
         while (*s && depth > 0)
         {
            if (*s == '{') depth++;
            else if (*s == '}') depth--;
            s++;
         }
         func_body_end = s;
      }
   }

   /* Step 5: check for aggregate initialization (OUT = { ... } or
    * ret_type OUT = { ... }).  If found, all members are initialized. */
   {
      const char *scan = func_body_start;
      while (scan < func_body_end)
      {
         if (*scan == '=' && scan + 1 < func_body_end)
         {
            const char *after = scan + 1;
            while (*after == ' ' || *after == '\t' || *after == '\n' || *after == '\r')
               after++;
            if (*after == '{')
            {
               /* Check that this is an assignment to the output variable,
                * not some other variable.  Look backward for an identifier. */
               const char *before = scan - 1;
               while (before > func_body_start && (*before == ' ' || *before == '\t'))
                  before--;
               if (before >= func_body_start && d3d9_hlsl_is_ident_char(*before))
               {
                  /* Found identifier = { ... } — assume aggregate init */
                  return NULL; /* No changes needed */
               }
            }
         }
         scan++;
      }
   }

   /* Step 6: check which members are assigned (look for 'OUT.member' or 'OUT .member') */
   {
      unsigned i;
      for (i = 0; i < member_count; i++)
      {
         char pattern[192];
         bool found = false;
         /* Build "OUT.membername" pattern and search within function body */
         /* Try common output variable names: OUT, Out, output */
         {
            const char *scan = func_body_start;
            while (scan < func_body_end)
            {
               /* Look for '.membername' preceded by an identifier */
               if (*scan == '.' && strncmp(scan + 1,
                        members[i].name, members[i].len) == 0
                     && !d3d9_hlsl_is_ident_char(scan[1 + members[i].len]))
               {
                  /* Check it's an assignment — look for '=' after the member,
                   * possibly with a swizzle (.xy, .xyzw, etc.) in between */
                  const char *eq = scan + 1 + members[i].len;
                  /* Skip optional swizzle: .x, .xy, .xyz, .xyzw, .rg, etc. */
                  if (*eq == '.')
                  {
                     eq++;
                     while (d3d9_hlsl_is_ident_char(*eq)) eq++;
                  }
                  while (*eq == ' ' || *eq == '\t') eq++;
                  if (*eq == '=' && eq[1] != '=')
                  { found = true; break; }
               }
               scan++;
            }
         }

         if (!found)
         {
            /* This member is not assigned — add initialization */
            size_t added = snprintf(unassigned_buf + upos,
                  sizeof(unassigned_buf) - upos,
                  "OUT.%.*s = 0;\n",
                  (int)members[i].len, members[i].name);
            upos += added;
            unassigned_count++;
         }
      }
   }

   if (unassigned_count == 0) return NULL;

   /* Step 6: insert initializations before 'return OUT;' */
   {
      const char *ret_stmt = strstr(func_body_start, "return ");
      size_t src_len, pre_len, tail_len, new_len;
      char *out;

      if (!ret_stmt || ret_stmt >= func_body_end) return NULL;

      src_len = strlen(source);
      pre_len = (size_t)(ret_stmt - source);
      tail_len = src_len - pre_len;
      new_len = src_len + upos;

      out = (char*)malloc(new_len + 1);
      if (!out) return NULL;

      memcpy(out, source, pre_len);
      memcpy(out + pre_len, unassigned_buf, upos);
      memcpy(out + pre_len + upos, ret_stmt, tail_len);
      out[new_len] = '\0';
      return out;
   }
}

/* Convert consecutively-invoked single-parameter macros into [loop] for
 * loops.  Many Cg shaders unroll filter kernels via macros like:
 *
 *   #define macroloop(i) <body using i>
 *   macroloop(0)
 *   macroloop(1)
 *   ...
 *   macroloop(28)
 *
 * The Cg compiler handles this, but D3DCompile for SM3.0 often exceeds
 * the 32-temp-register limit on the fully expanded code.  Converting
 * back to a real loop lets the HLSL compiler emit loop instructions.
 *
 * Detection:
 *  1. Find '#define NAME(PARAM) BODY' where BODY uses PARAM
 *  2. Find a run of NAME(0) NAME(1) ... NAME(N) with N >= 4
 *  3. Replace the run with '[loop] for (int PARAM = 0; PARAM <= N; PARAM++) { BODY }'
 *  4. Comment out the original #define
 *
 * Returns a malloc'd modified copy, or NULL if no changes were made. */
static char *d3d9_hlsl_convert_macro_loops(const char *source)
{
   /* Find #define candidates: single-param function-like macros */
   const char *p;
   char macro_name[128];
   char macro_param[64];
   const char *macro_body_start = NULL;
   size_t macro_body_len = 0;
   size_t macro_name_len = 0;
   size_t macro_param_len = 0;
   const char *macro_define_start = NULL;
   const char *macro_define_end = NULL;

   p = source;
   while (*p)
   {
      /* Look for #define at start of line */
      if ((p == source || p[-1] == '\n') && *p == '#')
      {
         const char *dir = p + 1;
         while (*dir == ' ' || *dir == '\t') dir++;
         if (strncmp(dir, "define", 6) == 0 && !d3d9_hlsl_is_ident_char(dir[6]))
         {
            const char *nm = dir + 6;
            const char *nm_end, *pp, *pp_end, *body, *body_end;

            while (*nm == ' ' || *nm == '\t') nm++;
            nm_end = nm;
            while (d3d9_hlsl_is_ident_char(*nm_end)) nm_end++;

            /* Check for (param) */
            if (*nm_end == '(' && nm_end > nm)
            {
               pp = nm_end + 1;
               while (*pp == ' ' || *pp == '\t') pp++;
               pp_end = pp;
               while (d3d9_hlsl_is_ident_char(*pp_end)) pp_end++;
               if (pp_end > pp && *pp_end == ')')
               {
                  /* Found #define NAME(PARAM) — get the body */
                  body = pp_end + 1;
                  while (*body == ' ' || *body == '\t') body++;

                  /* Find end of body (handle line continuations) */
                  body_end = body;
                  while (*body_end)
                  {
                     if (*body_end == '\n')
                     {
                        if (body_end > body && body_end[-1] == '\\')
                        { body_end++; continue; }
                        break;
                     }
                     body_end++;
                  }

                  /* Check that PARAM appears in BODY */
                  {
                     size_t pl = (size_t)(pp_end - pp);
                     const char *scan = body;
                     bool found = false;
                     while (scan < body_end - pl)
                     {
                        if (strncmp(scan, pp, pl) == 0
                              && (scan == body || !d3d9_hlsl_is_ident_char(scan[-1]))
                              && !d3d9_hlsl_is_ident_char(scan[pl]))
                        { found = true; break; }
                        scan++;
                     }

                     if (found && (size_t)(nm_end - nm) < sizeof(macro_name)
                           && pl < sizeof(macro_param))
                     {
                        /* Now search for consecutive invocations:
                         * NAME(0) NAME(1) ... NAME(N) */
                        int max_seq = -1;
                        const char *seq_start = NULL, *seq_end_ptr = NULL;
                        const char *s = source;

                        while (*s)
                        {
                           if (strncmp(s, nm, (size_t)(nm_end - nm)) == 0
                                 && (s == source || !d3d9_hlsl_is_ident_char(s[-1]))
                                 && s[(size_t)(nm_end - nm)] == '(')
                           {
                              /* Check if this starts a consecutive run from 0 */
                              const char *call = s;
                              const char *after_name = s + (size_t)(nm_end - nm);
                              /* Parse the argument */
                              if (after_name[0] == '(' && after_name[1] == '0'
                                    && after_name[2] == ')')
                              {
                                 /* Found NAME(0) — scan forward for NAME(1), NAME(2)... */
                                 int expected = 1;
                                 const char *run_end = after_name + 3;
                                 seq_start = s;

                                 while (*run_end)
                                 {
                                    const char *next = run_end;
                                    char num_buf[16];
                                    int num_len;
                                    while (*next == ' ' || *next == '\t'
                                          || *next == '\n' || *next == '\r')
                                       next++;
                                    num_len = snprintf(num_buf, sizeof(num_buf),
                                          "%d", expected);
                                    if (strncmp(next, nm, (size_t)(nm_end - nm)) == 0
                                          && next[(size_t)(nm_end - nm)] == '('
                                          && strncmp(next + (size_t)(nm_end - nm) + 1,
                                             num_buf, num_len) == 0
                                          && next[(size_t)(nm_end - nm) + 1 + num_len] == ')')
                                    {
                                       run_end = next + (size_t)(nm_end - nm)
                                          + 1 + num_len + 1;
                                       expected++;
                                    }
                                    else
                                       break;
                                 }

                                 if (expected > 4)
                                 {
                                    max_seq = expected - 1;
                                    seq_end_ptr = run_end;
                                    break; /* Use first run found */
                                 }
                              }
                           }
                           s++;
                        }

                        if (max_seq >= 4 && seq_start && seq_end_ptr)
                        {
                           /* Build the replacement loop */
                           size_t src_len_full = strlen(source);
                           size_t body_l = (size_t)(body_end - body);
                           char *out;
                           size_t cap, opos;
                           size_t nml = (size_t)(nm_end - nm);

                           memcpy(macro_name, nm, nml);
                           macro_name[nml] = '\0';
                           macro_name_len = nml;
                           memcpy(macro_param, pp, (size_t)(pp_end - pp));
                           macro_param[(size_t)(pp_end - pp)] = '\0';
                           macro_param_len = (size_t)(pp_end - pp);
                           macro_body_start = body;
                           macro_body_len = body_l;
                           macro_define_start = p;
                           macro_define_end = body_end;
                           if (*macro_define_end == '\n')
                              macro_define_end++;

                           cap = src_len_full + body_l + 256;
                           out = (char*)malloc(cap);
                           if (!out) return NULL;
                           opos = 0;

                           /* Copy everything before the #define, commenting it out */
                           {
                              size_t pre = (size_t)(macro_define_start - source);
                              memcpy(out, source, pre);
                              opos = pre;
                           }

                           /* Comment out the #define line */
                           {
                              const char *dl = "/* ";
                              size_t dll = 3;
                              size_t def_len = (size_t)(macro_define_end - macro_define_start);
                              /* Strip any trailing newline from the define for the comment */
                              size_t comment_len = def_len;
                              while (comment_len > 0
                                    && (macro_define_start[comment_len-1] == '\n'
                                       || macro_define_start[comment_len-1] == '\r'))
                                 comment_len--;
                              while (opos + dll + comment_len + 10 >= cap)
                              { cap *= 2; out = (char*)realloc(out, cap); }
                              memcpy(out + opos, dl, dll); opos += dll;
                              memcpy(out + opos, macro_define_start, comment_len);
                              opos += comment_len;
                              memcpy(out + opos, " */\n", 4); opos += 4;
                           }

                           /* Copy between #define and the macro call run */
                           {
                              size_t mid = (size_t)(seq_start - macro_define_end);
                              while (opos + mid + 1 >= cap)
                              { cap *= 2; out = (char*)realloc(out, cap); }
                              memcpy(out + opos, macro_define_end, mid);
                              opos += mid;
                           }

                           /* Emit the [loop] for */
                           {
                              char loop_header[256];
                              size_t lh_len = snprintf(loop_header, sizeof(loop_header),
                                    "[loop] for (int %s = 0; %s <= %d; %s++) {\n",
                                    macro_param, macro_param, max_seq, macro_param);
                              while (opos + lh_len + 1 >= cap)
                              { cap *= 2; out = (char*)realloc(out, cap); }
                              memcpy(out + opos, loop_header, lh_len);
                              opos += lh_len;
                           }

                           /* Emit the macro body — need to strip line continuations */
                           {
                              const char *bp_src = macro_body_start;
                              while (bp_src < macro_body_start + macro_body_len)
                              {
                                 if (*bp_src == '\\' && bp_src + 1 < macro_body_start + macro_body_len
                                       && bp_src[1] == '\n')
                                 { bp_src += 2; continue; }
                                 while (opos + 2 >= cap)
                                 { cap *= 2; out = (char*)realloc(out, cap); }
                                 out[opos++] = *bp_src++;
                              }
                           }

                           /* Close the loop */
                           {
                              const char *cl = "\n}\n";
                              while (opos + 4 >= cap)
                              { cap *= 2; out = (char*)realloc(out, cap); }
                              memcpy(out + opos, cl, 3); opos += 3;
                           }

                           /* Copy everything after the macro call run */
                           {
                              size_t tail = src_len_full - (size_t)(seq_end_ptr - source);
                              while (opos + tail + 1 >= cap)
                              { cap *= 2; out = (char*)realloc(out, cap); }
                              memcpy(out + opos, seq_end_ptr, tail);
                              opos += tail;
                           }

                           out[opos] = '\0';
                           return out;
                        }
                     }
                  }
               }
            }
         }
      }
      p++;
   }

   return NULL; /* No changes */
}

/* Load a shader from file AND populate pass_data with CTAB info.
 * Reads the shader source, preprocesses #include directives,
 * fixes Cg-isms, then compiles via d3d_compile. */
static bool d3d9_hlsl_load_program_from_file_ex(
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      const char *prog,
      hlsl_pass_data_t *pd)
{
   D3DBlob code_f = NULL;
   D3DBlob code_v = NULL;
   char *source   = NULL;
   char *resolved = NULL;
   char base_dir[PATH_MAX_LENGTH];
   size_t src_len = 0;

   if (!prog || !*prog)
      return false;

   /* Read source file */
   source = d3d9_hlsl_read_file(prog, &src_len);
   if (!source)
   {
      RARCH_ERR("[D3D9 HLSL] Could not read shader file: %s\n", prog);
      return false;
   }

   /* Extract directory for #include resolution */
   strlcpy(base_dir, prog, sizeof(base_dir));
   path_basedir(base_dir);

   /* Resolve #include directives */
   resolved = d3d9_hlsl_preprocess_includes(source, base_dir, 0);
   free(source);

   if (!resolved)
   {
      RARCH_ERR("[D3D9 HLSL] Include preprocessing failed: %s\n", prog);
      return false;
   }

   /* Convert Cg '#pragma parameter' + '#define' pairs into HLSL uniforms.
    *
    * Cg shaders declare tweakable parameters like:
    *   #pragma parameter AAOFFSET "description" 1.0 0.25 2.0 0.05
    *   #define AAOFFSET 1.0
    *
    * The Cg runtime creates a uniform from #pragma parameter, but
    * D3DCompile ignores #pragma and just expands #define — making
    * AAOFFSET a compile-time constant with no CTAB entry.
    *
    * Fix: for each #pragma parameter NAME, find a matching
    * '#define NAME <number>' and replace it with
    * 'uniform float NAME = <number>;'  so D3DCompile creates a real
    * uniform that appears in the CTAB and can be set at runtime. */
   {
      char *p = resolved;
      while ((p = strstr(p, "#pragma parameter")) != NULL)
      {
         const char *line_start = p;
         const char *pp = p + 17; /* skip "#pragma parameter" */
         const char *name_start, *name_end;
         size_t name_len;

         /* Check that #pragma is at the start of a line (not commented out).
          * Scan backwards to the start of the line and check for '//' */
         {
            const char *ls = p;
            bool in_comment = false;
            while (ls > resolved && ls[-1] != '\n')
               ls--;
            /* Check if there's a '//' between line start and #pragma */
            {
               const char *sc = ls;
               while (sc < p)
               {
                  if (sc[0] == '/' && sc[1] == '/')
                  { in_comment = true; break; }
                  if (*sc != ' ' && *sc != '\t')
                     break;
                  sc++;
               }
            }
            if (in_comment)
            {
               p++;
               continue;
            }
         }

         /* Skip whitespace to get parameter name */
         while (*pp == ' ' || *pp == '\t') pp++;
         name_start = pp;
         while (d3d9_hlsl_is_ident_char(*pp)) pp++;
         name_end = pp;
         name_len = (size_t)(name_end - name_start);

         if (name_len > 0 && name_len < 128)
         {
            char name_buf[128];
            char define_pattern[140];
            char *def_loc;

            memcpy(name_buf, name_start, name_len);
            name_buf[name_len] = '\0';

            /* Build pattern: "#define NAME " (with trailing space) */
            snprintf(define_pattern, sizeof(define_pattern),
                  "#define %s ", name_buf);

            /* Search for matching #define */
            def_loc = strstr(resolved, define_pattern);
            if (!def_loc)
            {
               /* Try with tab separator */
               snprintf(define_pattern, sizeof(define_pattern),
                     "#define %s\t", name_buf);
               def_loc = strstr(resolved, define_pattern);
            }

            if (def_loc)
            {
               /* Found "#define NAME <value>".
                * Extract the value (rest of line). */
               const char *val_start;
               const char *val_end;
               const char *def_line_end;
               char replacement[256];
               size_t def_line_len, repl_len, old_len, tail_len;

               /* Find end of #define line */
               def_line_end = def_loc;
               while (*def_line_end && *def_line_end != '\n'
                     && *def_line_end != '\r')
                  def_line_end++;

               def_line_len = (size_t)(def_line_end - def_loc);

               /* Extract value: skip "#define NAME " to get value part */
               val_start = def_loc + strlen(define_pattern);
               /* Trim leading whitespace from value */
               while (val_start < def_line_end
                     && (*val_start == ' ' || *val_start == '\t'))
                  val_start++;
               val_end = def_line_end;
               /* Strip // comment from value */
               {
                  const char *cmt = val_start;
                  while (cmt < val_end - 1)
                  {
                     if (cmt[0] == '/' && cmt[1] == '/')
                     { val_end = cmt; break; }
                     cmt++;
                  }
               }
               /* Trim trailing whitespace */
               while (val_end > val_start
                     && (val_end[-1] == ' ' || val_end[-1] == '\t'))
                  val_end--;

               if (val_end > val_start)
               {
                  char val_buf[64];
                  size_t val_len = (size_t)(val_end - val_start);
                  if (val_len >= sizeof(val_buf))
                     val_len = sizeof(val_buf) - 1;
                  memcpy(val_buf, val_start, val_len);
                  val_buf[val_len] = '\0';

                  /* Build replacement: "uniform float NAME = VALUE;" */
                  repl_len = snprintf(replacement, sizeof(replacement),
                        "uniform float %s = %s;", name_buf, val_buf);

                  /* Replace the #define line in-place */
                  old_len = strlen(resolved);
                  tail_len = old_len - (size_t)(def_loc - resolved) - def_line_len;

                  if (repl_len <= def_line_len)
                  {
                     /* Replacement fits — copy in place + shift tail */
                     memcpy(def_loc, replacement, repl_len);
                     memmove(def_loc + repl_len,
                           def_loc + def_line_len, tail_len + 1);
                  }
                  else
                  {
                     /* Replacement is longer — realloc */
                     size_t new_len = old_len - def_line_len + repl_len;
                     size_t def_off = (size_t)(def_loc - resolved);
                     char *new_buf  = (char*)malloc(new_len + 1);
                     if (new_buf)
                     {
                        memcpy(new_buf, resolved, def_off);
                        memcpy(new_buf + def_off, replacement, repl_len);
                        memcpy(new_buf + def_off + repl_len,
                              resolved + def_off + def_line_len,
                              tail_len + 1);
                        free(resolved);
                        resolved = new_buf;
                        /* Reset p to continue scanning in new buffer */
                        p = resolved;
                        continue;
                     }
                  }
               }
            }
         }

         /* Advance past this #pragma line */
         while (*p && *p != '\n') p++;
         if (*p) p++;
      }
   }

   /* Add semantics to struct members that lack them.
    * Many Cg shaders define structs without HLSL semantics
    * (e.g. tex_coords with float2 members used as interpolants).
    * This pass auto-assigns TEXCOORD semantics. */
   /* Semantic pass */
   /* Add semantics to struct members that lack them */
   {
      char *with_sem = d3d9_hlsl_add_struct_semantics(resolved);
      if (with_sem)
      {
         free(resolved);
         resolved = with_sem;
      }
   }

   /* Apply Cg→HLSL source-level fixes:
    *  1. 'texture' as identifier → '_texture' (HLSL reserved keyword)
    *  2. 'uniform <type> <name>' in function params → move to global scope
    *     (HLSL requires semantics on all VS/PS input params; Cg 'uniform'
    *      params are set via the runtime, not from vertex streams) */
   {
      char *fixed = d3d9_hlsl_fixup_cg_source(resolved);
      free(resolved);
      resolved = fixed;
      if (!resolved)
      {
         RARCH_ERR("[D3D9 HLSL] Source fixup failed: %s\n", prog);
         return false;
      }
   }

   /* Collapse redundant floatN(X, X, ..., X) constructors where all
    * arguments are identical identifiers.  Cg allows float4(float4_var,
    * float4_var, float4_var, float4_var) taking the first N components;
    * HLSL does not.  Replace with just (X). */
   {
      const char *search = resolved;
      while ((search = strstr(search, "float")) != NULL)
      {
         const char *fp = search;
         /* Check floatN( pattern */
         if ((fp[5] == '3' || fp[5] == '4')
               && fp[6] == '('
               && (fp == resolved || !d3d9_hlsl_is_ident_char(fp[-1])))
         {
            int nc = fp[5] - '0';
            const char *args = fp + 7;
            /* Collect comma-separated args respecting parens */
            const char *a_s[4];
            size_t a_l[4];
            int ac = 0, depth = 1;
            const char *s = args;
            bool ok = true;

            a_s[0] = s;
            while (*s && depth > 0)
            {
               if (*s == '(') depth++;
               else if (*s == ')') { if (--depth == 0) break; }
               else if (*s == ',' && depth == 1)
               {
                  const char *ae = s, *ab = a_s[ac];
                  while (ae > ab && (ae[-1] == ' ' || ae[-1] == '\t')) ae--;
                  while (ab < ae && (*ab == ' ' || *ab == '\t')) ab++;
                  a_s[ac] = ab;
                  a_l[ac] = (size_t)(ae - ab);
                  ac++;
                  if (ac >= nc) { ok = false; break; }
                  a_s[ac] = s + 1;
               }
               s++;
            }
            if (ok && depth == 0 && ac == nc - 1)
            {
               /* Record last arg */
               const char *ae = s, *ab = a_s[ac];
               while (ae > ab && (ae[-1] == ' ' || ae[-1] == '\t')) ae--;
               while (ab < ae && (*ab == ' ' || *ab == '\t')) ab++;
               a_s[ac] = ab;
               a_l[ac] = (size_t)(ae - ab);
               ac++;

               /* Check all args identical and start with letter/underscore */
               if (ac == nc && a_l[0] > 0
                     && d3d9_hlsl_is_ident_char(a_s[0][0])
                     && !(a_s[0][0] >= '0' && a_s[0][0] <= '9'))
               {
                  bool same = true;
                  int ai;
                  for (ai = 1; ai < ac; ai++)
                     if (a_l[ai] != a_l[0]
                           || strncmp(a_s[0], a_s[ai], a_l[0]) != 0)
                     { same = false; break; }

                  if (same)
                  {
                     /* Replace float4(X, X, X, X) with (X) */
                     size_t old_len = (size_t)(s + 1 - fp); /* includes closing ) */
                     size_t new_len = a_l[0] + 2; /* (X) */
                     size_t src_full = strlen(resolved);
                     size_t off = (size_t)(fp - resolved);
                     size_t tail = src_full - off - old_len;
                     char *nb = (char*)malloc(src_full - old_len + new_len + 1);
                     if (nb)
                     {
                        memcpy(nb, resolved, off);
                        nb[off] = '(';
                        memcpy(nb + off + 1, a_s[0], a_l[0]);
                        nb[off + 1 + a_l[0]] = ')';
                        memcpy(nb + off + new_len, fp + old_len, tail + 1);
                        free(resolved);
                        resolved = nb;
                        search = resolved + off + new_len;
                        continue;
                     }
                  }
               }
            }
         }
         search++;
      }
   }

   /* Decompose sampler members out of structs.
    * HLSL SM3.0 doesn't allow samplers in compound types.
    * This removes 'sampler2D _texture;' from struct bodies and
    * creates standalone sampler declarations for each instance. */
   {
      char *decomposed = d3d9_hlsl_decompose_struct_samplers(resolved);
      if (decomposed)
      {
         free(resolved);
         resolved = decomposed;
      }
   }

   /* Ensure sampler declarations exist for ORIG/PREV/PASSPREV textures.
    * Multi-pass Cg shaders reference ORIG__texture, PREVn__texture, etc.
    * via compat macros and struct sampler decomposition.  If the shader
    * passes these as function parameters (e.g. 'in orig ORIG : TEXCOORD2')
    * rather than global uniforms, the decomposition pass won't create
    * standalone sampler declarations.  Scan for references and add
    * missing declarations. */
   {
      static const char *sampler_names[] = {
         "ORIG__texture", "PREV__texture",
         "PREV1__texture", "PREV2__texture", "PREV3__texture",
         "PREV4__texture", "PREV5__texture", "PREV6__texture",
         "PASSPREV1__texture", "PASSPREV2__texture",
         "PASSPREV3__texture", "PASSPREV4__texture",
         NULL
      };
      char decl_buf[1024];
      size_t dpos = 0;
      int si;

      for (si = 0; sampler_names[si]; si++)
      {
         const char *name = sampler_names[si];
         size_t nlen = strlen(name);
         /* Check if this sampler is actually used in the source.
          * Since ORIG__texture only appears in #define lines (as a macro
          * replacement target), we need to check for uses of the MACROS
          * that reference it: ORIG_texture, ORIG_Sample, ORIG_SamplePoint,
          * or direct uses of ORIG__texture in actual code (not #define). */
         bool found_real_ref = false;

         /* Check for direct usage outside #define lines */
         {
            const char *ref = resolved;
            while ((ref = strstr(ref, name)) != NULL)
            {
               /* Verify whole-word */
               if ((ref > resolved && d3d9_hlsl_is_ident_char(ref[-1]))
                     || d3d9_hlsl_is_ident_char(ref[nlen]))
               { ref++; continue; }
               /* Check not inside a #define line */
               {
                  const char *ls = ref;
                  bool in_define = false;
                  while (ls > resolved && ls[-1] != '\n') ls--;
                  while (*ls == ' ' || *ls == '\t') ls++;
                  if (*ls == '#')
                  {
                     const char *dir = ls + 1;
                     while (*dir == ' ' || *dir == '\t') dir++;
                     if (strncmp(dir, "define", 6) == 0)
                        in_define = true;
                  }
                  if (!in_define)
                  { found_real_ref = true; break; }
               }
               ref++;
            }
         }

         /* Also check for usage of macros that expand to this sampler.
          * E.g., ORIG_texture expands to ORIG__texture, and ORIG_Sample
          * uses ORIG__texture internally via tex2D(ORIG, ...).
          * Check for the prefix (e.g., "ORIG" for "ORIG__texture") used
          * in function parameters like 'in orig ORIG' or calls like
          * ORIG_Sample, ORIG_SamplePoint, ORIG_texture. */
         if (!found_real_ref)
         {
            /* Extract prefix: everything before "__texture" */
            const char *suffix = strstr(name, "__texture");
            if (suffix)
            {
               size_t prefix_len = (size_t)(suffix - name);
               char usage_pattern[64];
               /* Check for 'ORIG_Sample(' or 'ORIG_SamplePoint(' or
                * 'ORIG_texture' used as an identifier in actual code */
               snprintf(usage_pattern, sizeof(usage_pattern),
                     "%.*s_Sample", (int)prefix_len, name);
               if (strstr(resolved, usage_pattern))
                  found_real_ref = true;

               if (!found_real_ref)
               {
                  snprintf(usage_pattern, sizeof(usage_pattern),
                        "%.*s_texture", (int)prefix_len, name);
                  /* Check this isn't only in #define lines */
                  {
                     const char *ref = resolved;
                     while ((ref = strstr(ref, usage_pattern)) != NULL)
                     {
                        const char *ls = ref;
                        bool in_define = false;
                        while (ls > resolved && ls[-1] != '\n') ls--;
                        while (*ls == ' ' || *ls == '\t') ls++;
                        if (*ls == '#')
                        {
                           const char *dir = ls + 1;
                           while (*dir == ' ' || *dir == '\t') dir++;
                           if (strncmp(dir, "define", 6) == 0)
                              in_define = true;
                        }
                        if (!in_define)
                        { found_real_ref = true; break; }
                        ref++;
                     }
                  }
               }

               /* Check for 'in orig ORIG' or 'uniform orig ORIG' pattern
                * in function signatures — struct parameter passing */
               if (!found_real_ref)
               {
                  char struct_pattern[64];
                  /* For ORIG__texture, check for 'orig ORIG' */
                  if (prefix_len == 4 && strncmp(name, "ORIG", 4) == 0)
                     snprintf(struct_pattern, sizeof(struct_pattern), "orig ORIG");
                  else
                     snprintf(struct_pattern, sizeof(struct_pattern),
                           "%.*s %.*s", (int)prefix_len, name,
                           (int)prefix_len, name);
                  if (strstr(resolved, struct_pattern))
                     found_real_ref = true;
               }
            }
         }

         if (!found_real_ref)
            continue;
         /* Check if it's already declared as sampler2D */
         {
            char decl_pattern[192];
            snprintf(decl_pattern, sizeof(decl_pattern),
                  "sampler2D %s", name);
            if (strstr(resolved, decl_pattern))
               continue; /* Already declared */
         }
         /* Add declaration */
         dpos += snprintf(decl_buf + dpos, sizeof(decl_buf) - dpos,
               "uniform sampler2D %s;\n", name);
      }

      if (dpos > 0)
      {
         /* Insert declarations at the beginning of the source */
         size_t old_len = strlen(resolved);
         char *nb = (char*)malloc(old_len + dpos + 1);
         if (nb)
         {
            memcpy(nb, decl_buf, dpos);
            memcpy(nb + dpos, resolved, old_len + 1);
            free(resolved);
            resolved = nb;
         }
      }
   }

   src_len = strlen(resolved);

   /* Zero-initialize any unassigned vertex shader output struct members.
    * Cg allows partial struct initialization; HLSL does not. */
   {
      char *inited = d3d9_hlsl_init_vs_output_members(resolved);
      if (inited)
      {
         free(resolved);
         resolved = inited;
         src_len = strlen(resolved);
      }
   }

   /* Convert consecutively-invoked macros into [loop] for loops.
    * This is critical for shaders with large unrolled filter kernels
    * (e.g. 29-tap FIR) that exceed SM3.0's temp register limit. */
   {
      char *looped = d3d9_hlsl_convert_macro_loops(resolved);
      if (looped)
      {
         free(resolved);
         resolved = looped;
         src_len = strlen(resolved);
      }
   }

   if (!d3d9_hlsl_compile_cg_compat(resolved, src_len, prog,
            "main_fragment", "ps_3_0", &code_f))
   {
      RARCH_ERR("[D3D9 HLSL] Could not compile fragment shader (%s).\n", prog);
      free(resolved);
      return false;
   }
   if (!d3d9_hlsl_compile_cg_compat(resolved, src_len, prog,
            "main_vertex", "vs_3_0", &code_v))
   {
      RARCH_ERR("[D3D9 HLSL] Could not compile vertex shader (%s).\n", prog);
      code_f->lpVtbl->Release(code_f);
      free(resolved);
      return false;
   }

   free(resolved);

   pass->ftable = NULL;
   pass->vtable = NULL;

   if (pd)
   {
      pd->ps_bytecode = d3d9_hlsl_dup_bytecode(code_f, &pd->ps_bytecode_dwords);
      pd->vs_bytecode = d3d9_hlsl_dup_bytecode(code_v, &pd->vs_bytecode_dwords);
      hlsl_uniform_map_from_bytecode(&pd->vs_map, pd->vs_bytecode, pd->vs_bytecode_dwords);
      hlsl_uniform_map_from_bytecode(&pd->ps_map, pd->ps_bytecode, pd->ps_bytecode_dwords);
   }

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

/* END HLSL RENDERCHAIN */

static uint32_t d3d9_hlsl_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_HLSL);
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_CG);

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

   if (d3d9_hlsl_overlay_decl)
   {
      IDirect3DVertexDeclaration9_Release(d3d9_hlsl_overlay_decl);
      d3d9_hlsl_overlay_decl = NULL;
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
      return d3d9_init_multipass(d3d, shader_path);

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
   link_info.tex_w      = input_scale * RARCH_SCALE_BASE;
   link_info.tex_h      = input_scale * RARCH_SCALE_BASE;
   link_info.pass       = &d3d->shader.pass[0];

   {
      hlsl_renderchain_t *renderchain =
         (hlsl_renderchain_t*)calloc(1, sizeof(*renderchain));
      if (!renderchain)
         return false;

      renderchain->chain.passes     = shader_pass_vector_list_new();
      renderchain->chain.luts       = lut_info_vector_list_new();
      renderchain->chain.bound_tex  = unsigned_vector_list_new();
      renderchain->chain.bound_vert = unsigned_vector_list_new();

      d3d->renderchain_data         = renderchain;
   }

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
      d3d9_hlsl_renderchain_t *chain   = (d3d9_hlsl_renderchain_t*)&_chain->chain;

      for (i = 0; i < d3d->shader.luts; i++)
      {
         if (!d3d9_hlsl_renderchain_add_lut(
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
static void d3d9_hlsl_free_overlays(d3d9_video_t *d3d)
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

static void d3d9_hlsl_overlay_tex_geom(
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

static void d3d9_hlsl_overlay_vertex_geom(
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

static bool d3d9_hlsl_overlay_load(void *data,
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

      overlay->tex       = NULL;
      IDirect3DDevice9_CreateTexture(d3d->dev,
                  width, height, 1,
                  0,
                  D3D9_ARGB8888_FORMAT,
                  D3DPOOL_MANAGED,
                  (struct IDirect3DTexture9**)&overlay->tex, NULL);

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
      d3d9_hlsl_overlay_tex_geom(d3d, i, 0, 0, 1, 1);
      d3d9_hlsl_overlay_vertex_geom(d3d, i, 0, 0, 1, 1);
   }

   return true;
}

static void d3d9_hlsl_overlay_enable(void *data, bool state)
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

static void d3d9_hlsl_overlay_full_screen(void *data, bool enable)
{
   unsigned i;
   d3d9_video_t *d3d = (d3d9_video_t*)data;

   for (i = 0; i < d3d->overlays_size; i++)
      d3d->overlays[i].fullscreen = enable;
}

static void d3d9_hlsl_overlay_set_alpha(void *data, unsigned index, float mod)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (d3d)
      d3d->overlays[index].alpha_mod = mod;
}

static const video_overlay_interface_t d3d9_hlsl_overlay_interface = {
   d3d9_hlsl_overlay_enable,
   d3d9_hlsl_overlay_load,
   d3d9_hlsl_overlay_tex_geom,
   d3d9_hlsl_overlay_vertex_geom,
   d3d9_hlsl_overlay_full_screen,
   d3d9_hlsl_overlay_set_alpha,
};

static void d3d9_hlsl_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   *iface = &d3d9_hlsl_overlay_interface;
}
#endif

#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
static void d3d9_hlsl_overlay_render(d3d9_video_t *d3d,
      unsigned width,
      unsigned height,
      overlay_t *overlay, bool force_linear)
{
   D3DTEXTUREFILTERTYPE filter_type;
   LPDIRECT3DDEVICE9 dev;
   struct video_viewport vp;
   void *verts;
   unsigned i;
   Vertex vert[4];
   static const D3DVERTEXELEMENT9 vElems[4] = {
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
#ifdef _XBOX
      if (!SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
                  dev, sizeof(vert), D3DUSAGE_WRITEONLY,
                  0,
                  D3DPOOL_MANAGED,
                  (LPDIRECT3DVERTEXBUFFER9*)&overlay->vert_buf, NULL)))
         overlay->vert_buf = NULL;
#else
      if (!SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(
                  dev, sizeof(vert), D3DUSAGE_WRITEONLY,
                  0,
                  D3DPOOL_MANAGED,
                  (LPDIRECT3DVERTEXBUFFER9*)&overlay->vert_buf, NULL)))
         overlay->vert_buf = NULL;
#endif

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

   /* Use cached vertex declaration for overlay. */
   if (!d3d9_hlsl_overlay_decl)
      IDirect3DDevice9_CreateVertexDeclaration(dev,
            (const D3DVERTEXELEMENT9*)&vElems, (IDirect3DVertexDeclaration9**)&d3d9_hlsl_overlay_decl);
   IDirect3DDevice9_SetVertexDeclaration(dev, d3d9_hlsl_overlay_decl);

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

#endif

static void d3d9_hlsl_free(void *data)
{
   d3d9_video_t   *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_OVERLAY
   d3d9_hlsl_free_overlays(d3d);
#endif

   if (d3d->menu)
   {
      if ((LPDIRECT3DTEXTURE9)d3d->menu->tex)
         IDirect3DTexture9_Release((LPDIRECT3DTEXTURE9)d3d->menu->tex);
      d3d9_vertex_buffer_free(d3d->menu->vert_buf, NULL);
      free(d3d->menu);
   }
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
      d3d9_hlsl_renderchain_t *chain  = (d3d9_hlsl_renderchain_t*)&_chain->chain;

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
         d3d9_hlsl_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev, 0,
            (const float*)&d3d->mvp, 4);
      d3d9_hlsl_overlay_render(d3d, width, height, d3d->menu, false);

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
         d3d9_hlsl_overlay_render(d3d, width, height, &d3d->overlays[i], true);
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

      d3d->menu->tex = NULL;
      IDirect3DDevice9_CreateTexture(d3d->dev,
            width, height, 1,
            0, D3D9_ARGB8888_FORMAT,
            D3DPOOL_MANAGED,
            (struct IDirect3DTexture9**)&d3d->menu->tex, NULL);

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

#ifndef _XBOX
   if (want_mipmap)
      usage |= D3DUSAGE_AUTOGENMIPMAP;
#endif
   tex = NULL;
   if (!SUCCEEDED(IDirect3DDevice9_CreateTexture(d3d->dev,
               ti->width, ti->height, 1,
               usage, D3D9_ARGB8888_FORMAT,
               D3DPOOL_MANAGED,
               (struct IDirect3DTexture9**)&tex, NULL)))
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

static struct video_shader *d3d9_hlsl_get_current_shader(void *data)
{
   d3d9_video_t *d3d = (d3d9_video_t*)data;
   if (!d3d)
      return NULL;
   return &d3d->shader;
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
   d3d9_hlsl_get_current_shader,
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
            !(d3dr &&
               SUCCEEDED(IDirect3DDevice9_GetRenderTarget(d3dr,
                     0, (LPDIRECT3DSURFACE9*)&target)))
         || !SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(d3dr,
               width, height,
               (D3DFORMAT)D3D9_XRGB8888_FORMAT, D3DPOOL_SYSTEMMEM,
               (LPDIRECT3DSURFACE9*)&dest, NULL))
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
