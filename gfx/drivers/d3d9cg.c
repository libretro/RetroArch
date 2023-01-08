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

#include <d3d9.h>

#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_inline.h>

#include <defines/d3d_defines.h>
#include "../common/d3d_common.h"
#include "../common/d3d9_common.h"
#include "../include/Cg/cg.h"
#include "../include/Cg/cgD3D9.h"
#include "../video_coord_array.h"
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

#include "d3d_shaders/opaque.cg.d3d9.h"
#include "d3d9_renderchain.h"

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
   struct d3d9_renderchain chain;
   struct shader_pass stock_shader;
   CGcontext cgCtx;
} cg_renderchain_t;

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

   for (i = 0; i < sizeof(illegal) / sizeof(illegal[0]); i++)
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

      if (     cgGetParameterDirection(param)   != CG_IN
            || cgGetParameterVariability(param) != CG_VARYING)
         continue;

      semantic = cgGetParameterSemantic(param);
      if (!semantic)
         continue;

      if (string_is_equal(sem, semantic) &&
            d3d9_cg_validate_param_name(cgGetParameterName(param)))
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

   if (
         fragment_profile == CG_PROFILE_UNKNOWN ||
         vertex_profile   == CG_PROFILE_UNKNOWN)
   {
      RARCH_ERR("Invalid profile type\n");
      goto error;
   }

   RARCH_LOG("[D3D9 Cg]: Vertex profile: %s\n",
         cgGetProfileString(vertex_profile));
   RARCH_LOG("[D3D9 Cg]: Fragment profile: %s\n",
         cgGetProfileString(fragment_profile));

   if (path_is_file && !string_is_empty(prog))
      pass->fprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
            prog, fragment_profile, "main_fragment", fragment_opts);
   else
      pass->fprg = cgCreateProgram(cgCtx, CG_SOURCE, stock_cg_d3d9_program,
            fragment_profile, "main_fragment", fragment_opts);

   list = cgGetLastListing(cgCtx);
   if (list)
      listing_f = strdup(list);

   if (path_is_file && !string_is_empty(prog))
      pass->vprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
            prog, vertex_profile, "main_vertex", vertex_opts);
   else
      pass->vprg = cgCreateProgram(cgCtx, CG_SOURCE, stock_cg_d3d9_program,
            vertex_profile, "main_vertex", vertex_opts);

   list = cgGetLastListing(cgCtx);
   if (list)
      listing_v = strdup(list);

   if (!pass->fprg || !pass->vprg)
      goto error;

   cgD3D9LoadProgram(pass->fprg, true, 0);
   cgD3D9LoadProgram(pass->vprg, true, 0);

   free(listing_f);
   free(listing_v);

   return true;

error:
   RARCH_ERR("CG error: %s\n", cgGetErrorString(cgGetError()));
   if (listing_f)
      RARCH_ERR("Fragment:\n%s\n", listing_f);
   else if (listing_v)
      RARCH_ERR("Vertex:\n%s\n", listing_v);
   free(listing_f);
   free(listing_v);

   return false;
}

static void d3d9_cg_renderchain_set_shader_params(
      d3d9_renderchain_t *chain,
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass,
      unsigned video_w,
      unsigned video_h,
      unsigned tex_w,
      unsigned tex_h,
      unsigned viewport_w,
      unsigned viewport_h)
{
   CGparameter param;
   float frame_cnt;
   float video_size[2];
   float texture_size[2];
   float output_size[2];
   CGprogram fprg       = (CGprogram)pass->fprg;
   CGprogram vprg       = (CGprogram)pass->vprg;

   video_size[0]        = video_w;
   video_size[1]        = video_h;
   texture_size[0]      = tex_w;
   texture_size[1]      = tex_h;
   output_size[0]       = viewport_w;
   output_size[1]       = viewport_h;

   frame_cnt            = chain->frame_count;

   if (pass->info.pass->frame_count_mod)
      frame_cnt         = chain->frame_count
         % pass->info.pass->frame_count_mod;

   /* Vertex program */
   param                = cgGetNamedParameter(vprg, "IN.video_size");
   if (param)
      cgD3D9SetUniform(param, &video_size);
   param                = cgGetNamedParameter(vprg, "IN.texture_size");
   if (param)
      cgD3D9SetUniform(param, &texture_size);
   param                = cgGetNamedParameter(vprg, "IN.output_size");
   if (param)
      cgD3D9SetUniform(param, &output_size);
   param                = cgGetNamedParameter(vprg, "IN.frame_count");
   if (param)
      cgD3D9SetUniform(param, &frame_cnt);

   /* Fragment program */
   param                = cgGetNamedParameter(fprg, "IN.video_size");
   if (param)
      cgD3D9SetUniform(param, &video_size);
   param                = cgGetNamedParameter(fprg, "IN.texture_size");
   if (param)
      cgD3D9SetUniform(param, &texture_size);
   param                = cgGetNamedParameter(fprg, "IN.output_size");
   if (param)
      cgD3D9SetUniform(param, &output_size);
   param                = cgGetNamedParameter(fprg, "IN.frame_count");
   if (param)
      cgD3D9SetUniform(param, &frame_cnt);
}

static bool d3d9_cg_renderchain_init_shader_fvf(
      d3d9_renderchain_t *chain,
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
      if (string_is_equal_fast(&decl_end, &decl[count], sizeof(decl_end)))
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

      RARCH_LOG("[D3D9 Cg]: FVF POSITION semantic found.\n");
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

      RARCH_LOG("[D3D9 Cg]: FVF TEXCOORD0 semantic found.\n");
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

      RARCH_LOG("[D3D9 Cg]: FVF TEXCOORD1 semantic found.\n");
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

      RARCH_LOG("[D3D9 Cg]: FVF COLOR0 semantic found.\n");
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

   return d3d9_vertex_declaration_new(chain->dev,
         decl, (void**)&pass->vertex_decl);
}

static void d3d9_cg_renderchain_bind_orig(
      d3d9_renderchain_t *chain,
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
      IDirect3DDevice9_SetTexture(chain->dev, index, (IDirect3DBaseTexture9*)first_pass->tex);
      IDirect3DDevice9_SetSamplerState(chain->dev,
            index, D3DSAMP_MINFILTER, d3d_translate_filter(first_pass->info.pass->filter));
      IDirect3DDevice9_SetSamplerState(chain->dev,
            index, D3DSAMP_MAGFILTER, d3d_translate_filter(first_pass->info.pass->filter));
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

static void d3d9_cg_renderchain_bind_prev(d3d9_renderchain_t *chain,
      LPDIRECT3DDEVICE9 dev,
      struct shader_pass *pass)
{
   unsigned i;
   float texture_size[2];
   char attr_texture[64]    = {0};
   char attr_input_size[64] = {0};
   char attr_tex_size[64]   = {0};
   char attr_coord[64]      = {0};
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
      CGparameter param;
      float video_size[2];
      CGprogram fprg = (CGprogram)pass->fprg;
      CGprogram vprg = (CGprogram)pass->vprg;

      snprintf(attr_texture,    sizeof(attr_texture),    "%s.texture",      prev_names[i]);
      snprintf(attr_input_size, sizeof(attr_input_size), "%s.video_size",   prev_names[i]);
      snprintf(attr_tex_size,   sizeof(attr_tex_size),   "%s.texture_size", prev_names[i]);
      snprintf(attr_coord,      sizeof(attr_coord),      "%s.tex_coord",    prev_names[i]);

      video_size[0]  = chain->prev.last_width[
         (chain->prev.ptr - (i + 1)) & TEXTURESMASK];
      video_size[1]  = chain->prev.last_height[
         (chain->prev.ptr - (i + 1)) & TEXTURESMASK];

      /* Vertex program */
      param = cgGetNamedParameter(vprg, attr_input_size);
      if (param)
         cgD3D9SetUniform(param, &video_size);
      param = cgGetNamedParameter(vprg, attr_tex_size);
      if (param)
         cgD3D9SetUniform(param, &texture_size);

      /* Fragment program */
      param = cgGetNamedParameter(fprg, attr_input_size);
      if (param)
         cgD3D9SetUniform(param, &video_size);
      param = cgGetNamedParameter(fprg, attr_tex_size);
      if (param)
         cgD3D9SetUniform(param, &texture_size);

      param = cgGetNamedParameter(fprg, attr_texture);
      if (param)
      {
         unsigned         index = cgGetParameterResourceIndex(param);
         LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)
            chain->prev.tex[
            (chain->prev.ptr - (i + 1)) & TEXTURESMASK];

         IDirect3DDevice9_SetTexture(chain->dev, index, (IDirect3DBaseTexture9*)tex);
         unsigned_vector_list_append(chain->bound_tex, index);

         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_MINFILTER,
               d3d_translate_filter(chain->passes->data[0].info.pass->filter));
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_MAGFILTER,
               d3d_translate_filter(chain->passes->data[0].info.pass->filter));
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      }

      param = cgGetNamedParameter(vprg, attr_coord);
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
   }
}

static void d3d9_cg_renderchain_bind_pass(
      d3d9_renderchain_t *chain,
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
      char pass_base[64]        = {0};
      char attr_texture[64]     = {0};
      char attr_input_size[64]  = {0};
      char attr_tex_size[64]    = {0};
      char attr_coord[64]       = {0};
      struct shader_pass *curr_pass = (struct shader_pass*)&chain->passes->data[i];

      snprintf(pass_base,       sizeof(pass_base),       "PASS%u",          i);
      snprintf(attr_texture,    sizeof(attr_texture),    "%s.texture",      pass_base);
      snprintf(attr_input_size, sizeof(attr_input_size), "%s.video_size",   pass_base);
      snprintf(attr_tex_size,   sizeof(attr_tex_size),   "%s.texture_size", pass_base);
      snprintf(attr_coord,      sizeof(attr_coord),      "%s.tex_coord",    pass_base);

      video_size[0]   = curr_pass->last_width;
      video_size[1]   = curr_pass->last_height;
      texture_size[0] = curr_pass->info.tex_w;
      texture_size[1] = curr_pass->info.tex_h;

      /* Vertex program */
      param           = cgGetNamedParameter(vprg, attr_input_size);
      if (param)
         cgD3D9SetUniform(param, &video_size);
      param           = cgGetNamedParameter(vprg, attr_tex_size);
      if (param)
         cgD3D9SetUniform(param, &texture_size);

      /* Fragment program */
      param           = cgGetNamedParameter(fprg, attr_input_size);
      if (param)
         cgD3D9SetUniform(param, &video_size);
      param           = cgGetNamedParameter(fprg, attr_tex_size);
      if (param)
         cgD3D9SetUniform(param, &texture_size);

      param = cgGetNamedParameter(fprg, attr_texture);
      if (param)
      {
         unsigned index = cgGetParameterResourceIndex(param);
         unsigned_vector_list_append(chain->bound_tex, index);

         IDirect3DDevice9_SetTexture(chain->dev, index, (IDirect3DBaseTexture9*)curr_pass->tex);
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_MINFILTER,
               d3d_translate_filter(curr_pass->info.pass->filter));
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_MAGFILTER,
               d3d_translate_filter(curr_pass->info.pass->filter));
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
         IDirect3DDevice9_SetSamplerState(chain->dev, index, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      }

      param = cgGetNamedParameter(vprg, attr_coord);
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
   }
}

static void d3d9_cg_deinit_progs(cg_renderchain_t *chain)
{
   int i;

   if (chain->chain.passes->count >= 1)
   {
      d3d9_vertex_buffer_free(NULL, chain->chain.passes->data[0].vertex_decl);

      for (i = 1; i < chain->chain.passes->count; i++)
      {
         if (chain->chain.passes->data[i].tex)
            IDirect3DTexture9_Release(chain->chain.passes->data[i].tex);
         chain->chain.passes->data[i].tex = NULL;
         d3d9_vertex_buffer_free(
               chain->chain.passes->data[i].vertex_buf,
               chain->chain.passes->data[i].vertex_decl);

         if (chain->chain.passes->data[i].fprg)
            cgDestroyProgram(chain->chain.passes->data[i].fprg);
         if (chain->chain.passes->data[i].vprg)
            cgDestroyProgram(chain->chain.passes->data[i].vprg);
      }
   }

   if (chain->stock_shader.fprg)
      cgDestroyProgram(chain->stock_shader.fprg);
   if (chain->stock_shader.vprg)
      cgDestroyProgram(chain->stock_shader.vprg);
}

static void d3d9_cg_destroy_resources(cg_renderchain_t *chain)
{
   int i;

   for (i = 0; i < TEXTURES; i++)
   {
      if (chain->chain.prev.tex[i])
         IDirect3DTexture9_Release(chain->chain.prev.tex[i]);
      if (chain->chain.prev.vertex_buf[i])
         d3d9_vertex_buffer_free(chain->chain.prev.vertex_buf[i], NULL);
   }

   d3d9_cg_deinit_progs(chain);

   for (i = 0; i < chain->chain.luts->count; i++)
   {
      if (chain->chain.luts->data[i].tex)
         IDirect3DTexture9_Release(chain->chain.luts->data[i].tex);
   }

   cgD3D9UnloadAllPrograms();
   cgD3D9SetDevice(NULL);
}

static void d3d9_cg_deinit_context_state(cg_renderchain_t *chain)
{
   if (chain->cgCtx)
      cgDestroyContext(chain->cgCtx);

   chain->cgCtx = NULL;
}

void d3d9_cg_renderchain_free(void *data)
{
   cg_renderchain_t *chain = (cg_renderchain_t*)data;

   if (!chain)
      return;

   d3d9_cg_destroy_resources(chain);
   d3d9_renderchain_destroy_passes_and_luts(&chain->chain);
   d3d9_cg_deinit_context_state(chain);

   free(chain);
}

static void *d3d9_cg_renderchain_new(void)
{
   cg_renderchain_t *renderchain = (cg_renderchain_t*)calloc(1, sizeof(*renderchain));
   if (!renderchain)
      return NULL;

   d3d9_init_renderchain(&renderchain->chain);

   return renderchain;
}

static bool d3d9_cg_renderchain_init_shader(d3d9_video_t *d3d,
      cg_renderchain_t *renderchain)
{
   CGcontext cgCtx    = cgCreateContext();

   if (!cgCtx)
   {
      RARCH_ERR("Failed to create Cg context.\n");
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
      d3d9_renderchain_t *chain,
      const struct LinkInfo *info, unsigned _fmt)
{
   unsigned i;
   struct shader_pass pass;
   struct d3d_matrix ident;
   unsigned fmt = (_fmt == RETRO_PIXEL_FORMAT_RGB565) ?
      D3D9_RGB565_FORMAT : D3D9_XRGB8888_FORMAT;

   d3d_matrix_identity(&ident);

   IDirect3DDevice9_SetTransform(dev, D3DTS_WORLD, (D3DMATRIX*)&ident);
   IDirect3DDevice9_SetTransform(dev, D3DTS_VIEW,  (D3DMATRIX*)&ident);

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
            chain->dev, 4 * sizeof(struct D3D9CGVertex),
            D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, NULL);

      if (!chain->prev.vertex_buf[i])
         return false;

      chain->prev.tex[i] = (LPDIRECT3DTEXTURE9)
         d3d9_texture_new(chain->dev,
            info->tex_w, info->tex_h, 1, 0, fmt,
            D3DPOOL_MANAGED, 0, 0, 0, NULL, NULL, false);

      if (!chain->prev.tex[i])
         return false;

      IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)chain->prev.tex[i]);
      IDirect3DDevice9_SetSamplerState(dev,
            0, D3DSAMP_MINFILTER, d3d_translate_filter(info->pass->filter));
      IDirect3DDevice9_SetSamplerState(dev,
            0, D3DSAMP_MAGFILTER, d3d_translate_filter(info->pass->filter));
      IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      IDirect3DDevice9_SetTexture(chain->dev, 0, NULL);
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
      const D3DVIEWPORT9 *final_viewport,
      const struct LinkInfo *info,
      bool rgb32)
{
   cg_renderchain_t *chain        = (cg_renderchain_t*)d3d->renderchain_data;
   unsigned fmt                   = (rgb32) ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;

   if (!chain)
      return false;
   if (!d3d9_cg_renderchain_init_shader(d3d, chain))
   {
      RARCH_ERR("[D3D9 Cg]: Failed to initialize shader subsystem.\n");
      return false;
   }

   chain->chain.dev            = dev;
   chain->chain.final_viewport = (D3DVIEWPORT9*)final_viewport;
   chain->chain.frame_count    = 0;
   chain->chain.pixel_size     = (fmt == RETRO_PIXEL_FORMAT_RGB565) ? 2 : 4;

   if (!d3d9_cg_renderchain_create_first_pass(dev, chain, &chain->chain, info, fmt))
      return false;
   if (!d3d9_cg_load_program((cg_renderchain_t*)chain, &chain->stock_shader, NULL, false))
      return false;

   cgD3D9BindProgram((CGprogram)chain->stock_shader.fprg);
   cgD3D9BindProgram((CGprogram)chain->stock_shader.vprg);

   return true;
}

static bool d3d9_cg_renderchain_add_pass(void *data, const struct LinkInfo *info)
{
   struct shader_pass pass;
   cg_renderchain_t *chain     = (cg_renderchain_t*)data;

   pass.info                   = *info;
   pass.last_width             = 0;
   pass.last_height            = 0;
   pass.attrib_map             = (struct unsigned_vector_list*)
      unsigned_vector_list_new();
   pass.pool                   = D3DPOOL_DEFAULT;

   d3d9_cg_load_program((cg_renderchain_t*)chain, &pass, info->pass->source.path, true);

   if (!d3d9_cg_renderchain_init_shader_fvf(&chain->chain, &pass))
      return false;

   return d3d9_renderchain_add_pass(&chain->chain, &pass,
         info);
}

static void d3d9_cg_renderchain_calc_and_set_shader_mvp(
      CGprogram data, /* stock vertex program */
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   struct d3d_matrix proj, ortho, rot, matrix;
   CGparameter cgp = cgGetNamedParameter(data, "modelViewProj");

   d3d_matrix_identity(&ortho);
   d3d_matrix_ortho_off_center_lh(&ortho, 0, vp_width, 0, vp_height, 0, 1);
   d3d_matrix_identity(&rot);
   d3d_matrix_rotation_z(&rot, rotation * (D3D_PI / 2.0));
   d3d_matrix_multiply(&proj, &ortho, &rot);
   d3d_matrix_transpose(&matrix, &proj);

   if (cgp)
      cgD3D9SetUniformMatrix(cgp, (D3DMATRIX*)&matrix);
}

static INLINE void d3d9_cg_renderchain_set_vertices_on_change(
      d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation
      )
{
   struct D3D9CGVertex vert[4];
   unsigned i;
   void* verts       = NULL;
   const struct
      LinkInfo* info = (const struct LinkInfo*)&pass->info;
   float          _u = (float)(width) / info->tex_w;
   float          _v = (float)(height) / info->tex_h;

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
      vert[i].x -= 0.5f;
      vert[i].y += 0.5f;
   }

   IDirect3DVertexBuffer9_Lock(pass->vertex_buf, 0, 0, &verts, 0);
   memcpy(verts, vert, sizeof(vert));
   IDirect3DVertexBuffer9_Unlock(pass->vertex_buf);
}

static void d3d9_cg_renderchain_set_vertices(
      d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation)
{
   if (pass->last_width != width || pass->last_height != height)
      d3d9_cg_renderchain_set_vertices_on_change(chain,
            pass, width, height, out_width, out_height,
            vp_width, vp_height, rotation);

   d3d9_cg_renderchain_calc_and_set_shader_mvp(
         (CGprogram)pass->vprg, vp_width, vp_height, rotation);
   d3d9_cg_renderchain_set_shader_params(chain, chain->dev,
         pass,
         width, height,
         pass->info.tex_w, pass->info.tex_h,
         vp_width, vp_height);
}

static void d3d9_cg_renderchain_render_pass(
      d3d9_renderchain_t *chain,
      struct shader_pass *pass,
      unsigned pass_index)
{
   unsigned i;

   cgD3D9BindProgram((CGprogram)pass->fprg);
   cgD3D9BindProgram((CGprogram)pass->vprg);

   IDirect3DDevice9_SetTexture(chain->dev, 0, (IDirect3DBaseTexture9*)pass->tex);
   IDirect3DDevice9_SetSamplerState(chain->dev,
         0, D3DSAMP_MINFILTER, d3d_translate_filter(pass->info.pass->filter));
   IDirect3DDevice9_SetSamplerState(chain->dev,
         0, D3DSAMP_MAGFILTER, d3d_translate_filter(pass->info.pass->filter));

   IDirect3DDevice9_SetVertexDeclaration(chain->dev, pass->vertex_decl);
   for (i = 0; i < 4; i++)
      IDirect3DDevice9_SetStreamSource(chain->dev, i, pass->vertex_buf,
            0, sizeof(struct D3D9CGVertex));

   /* Set orig texture. */
   d3d9_cg_renderchain_bind_orig(chain, chain->dev, pass);

   /* Set prev textures. */
   d3d9_cg_renderchain_bind_prev(chain, chain->dev, pass);

   /* Set lookup textures */
   for (i = 0; i < chain->luts->count; i++)
   {
      CGparameter vparam;
      CGparameter fparam = cgGetNamedParameter(
            pass->fprg, chain->luts->data[i].id);
      int bound_index    = -1;

      if (fparam)
      {
         unsigned index  = cgGetParameterResourceIndex(fparam);
         bound_index     = index;

         d3d9_renderchain_add_lut_internal(chain, index, i);
      }

      vparam = cgGetNamedParameter(pass->vprg,
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
      d3d9_cg_renderchain_bind_pass(chain, chain->dev, pass, pass_index);

   IDirect3DDevice9_BeginScene(chain->dev);
   IDirect3DDevice9_DrawPrimitive(chain->dev, D3DPT_TRIANGLESTRIP, 0, 2);
   IDirect3DDevice9_EndScene(chain->dev);

   /* So we don't render with linear filter into render targets,
    * which apparently looked odd (too blurry). */
   IDirect3DDevice9_SetSamplerState(chain->dev,
         0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
   IDirect3DDevice9_SetSamplerState(chain->dev,
         0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

   d3d9_renderchain_unbind_all(chain);
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
   d3d9_renderchain_t *chain      = (d3d9_renderchain_t*)&_chain->chain;

   d3d9_renderchain_start_render(chain);

   current_width              = width;
   current_height             = height;

   first_pass                 = (struct shader_pass*)&chain->passes->data[0];

   d3d9_convert_geometry(
         &first_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->final_viewport);

   d3d9_blit_to_texture(first_pass->tex,
         frame_data,
         first_pass->info.tex_w,
         first_pass->info.tex_h,
         width,
         height,
         first_pass->last_width,
         first_pass->last_height,
         pitch, chain->pixel_size);

   /* Grab back buffer. */
   d3d9_device_get_render_target(chain->dev, 0, (void**)&back_buffer);

   /* In-between render target passes. */
   for (i = 0; i < chain->passes->count - 1; i++)
   {
      D3DVIEWPORT9   viewport = {0};
      struct shader_pass *from_pass  = (struct shader_pass*)&chain->passes->data[i];
      struct shader_pass *to_pass    = (struct shader_pass*)&chain->passes->data[i + 1];

      IDirect3DTexture9_GetSurfaceLevel(
		      (LPDIRECT3DTEXTURE9)to_pass->tex, 0, (IDirect3DSurface9**)&target);
      IDirect3DDevice9_SetRenderTarget(chain->dev, 0, target);

      d3d9_convert_geometry(&from_pass->info,
            &out_width, &out_height,
            current_width, current_height, chain->final_viewport);

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

      IDirect3DDevice9_SetViewport(chain->dev, (D3DVIEWPORT9*)&viewport);

      d3d9_cg_renderchain_set_vertices(
            chain, from_pass,
            current_width, current_height,
            out_width, out_height,
            out_width, out_height, 0);

      d3d9_cg_renderchain_render_pass(chain,
            from_pass,
            i + 1);

      current_width  = out_width;
      current_height = out_height;
      IDirect3DSurface9_Release(target);
   }

   /* Final pass */
   IDirect3DDevice9_SetRenderTarget(chain->dev, 0, back_buffer);

   last_pass = (struct shader_pass*)&chain->passes->
      data[chain->passes->count - 1];

   d3d9_convert_geometry(&last_pass->info,
         &out_width, &out_height,
         current_width, current_height, chain->final_viewport);

   IDirect3DDevice9_SetViewport(chain->dev, (D3DVIEWPORT9*)chain->final_viewport);

   d3d9_cg_renderchain_set_vertices(
         chain, last_pass,
         current_width, current_height,
         out_width, out_height,
         chain->final_viewport->Width,
         chain->final_viewport->Height,
         rotation);

   d3d9_cg_renderchain_render_pass(chain,
         last_pass,
         chain->passes->count);

   chain->frame_count++;

   if (back_buffer)
      IDirect3DSurface9_Release(back_buffer);

   d3d9_renderchain_end_render(chain);
   cgD3D9BindProgram((CGprogram)&_chain->stock_shader.fprg);
   cgD3D9BindProgram((CGprogram)&_chain->stock_shader.vprg);
   d3d9_cg_renderchain_calc_and_set_shader_mvp(
         (CGprogram)_chain->stock_shader.vprg,
         chain->final_viewport->Width,
         chain->final_viewport->Height, 0);
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
}

static bool d3d9_cg_init_base(d3d9_video_t *d3d, const video_info_t *info)
{
   D3DPRESENT_PARAMETERS d3dpp;
   HWND focus_window  = win32_get_window();

   memset(&d3dpp, 0, sizeof(d3dpp));

   g_pD3D9            = (LPDIRECT3D9)d3d9_create();

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

static bool renderchain_d3d_cg_init_first(
      enum gfx_ctx_api api,
      void **renderchain_handle)
{
   switch (api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
            void *data = d3d9_cg_renderchain_new();

            if (!data)
               return false;

            *renderchain_handle   = data;

            return true;
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static bool d3d9_cg_init_chain(d3d9_video_t *d3d,
      unsigned input_scale,
      bool rgb32)
{
   unsigned i = 0;
   struct LinkInfo link_info;
   unsigned current_width, current_height, out_width, out_height;

   /* Setup information for first pass. */
   link_info.pass  = NULL;
   link_info.tex_w = input_scale * RARCH_SCALE_BASE;
   link_info.tex_h = input_scale * RARCH_SCALE_BASE;
   link_info.pass  = &d3d->shader.pass[0];

   if (!renderchain_d3d_cg_init_first(GFX_CTX_DIRECT3D9_API,
            &d3d->renderchain_data))
   {
      RARCH_ERR("[D3D9]: Renderchain could not be initialized.\n");
      return false;
   }

   if (!d3d->renderchain_data)
      return false;

   if (
         !d3d9_cg_renderchain_init(
            d3d,
            d3d->dev, &d3d->final_viewport, &link_info,
            rgb32)
      )
   {
      RARCH_ERR("[D3D9]: Failed to init render chain.\n");
      return false;
   }

   d3d9_log_info(&link_info);

   current_width  = link_info.tex_w;
   current_height = link_info.tex_h;
   out_width      = 0;
   out_height     = 0;

   for (i = 1; i < d3d->shader.passes; i++)
   {
      d3d9_convert_geometry(
            &link_info,
            &out_width, &out_height,
            current_width, current_height, &d3d->final_viewport);

      link_info.pass  = &d3d->shader.pass[i];
      link_info.tex_w = next_pow2(out_width);
      link_info.tex_h = next_pow2(out_height);

      current_width   = out_width;
      current_height  = out_height;

      if (!d3d9_cg_renderchain_add_pass(
               d3d->renderchain_data, &link_info))
      {
         RARCH_ERR("[D3D9]: Failed to add pass.\n");
         return false;
      }
      d3d9_log_info(&link_info);
   }

   {
      unsigned i;
      settings_t *settings = config_get_ptr();
      bool video_smooth    = settings->bools.video_smooth;

      for (i = 0; i < d3d->shader.luts; i++)
      {
         if (!d3d9_renderchain_add_lut(
                  d3d->renderchain_data,
                  d3d->shader.lut[i].id, d3d->shader.lut[i].path,
                  d3d->shader.lut[i].filter == RARCH_FILTER_UNSPEC 
                  ? video_smooth 
                  : (d3d->shader.lut[i].filter == RARCH_FILTER_LINEAR)))
         {
            RARCH_ERR("[D3D9]: Failed to init LUTs.\n");
            return false;
         }
      }
   }

   return true;
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

      /* the D3DX font driver uses POOL_DEFAULT resources
       * and will prevent a clean reset here
       * another approach would be to keep track of all created D3D
       * font objects and free/realloc them around the d3d_reset call  */
#ifdef HAVE_MENU
      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
#endif

      if (!d3d9_reset(d3d->dev, &d3dpp))
      {
         d3d9_cg_deinitialize(d3d);
         IDirect3D9_Release(g_pD3D9);
         g_pD3D9 = NULL;

         ret = d3d9_cg_init_base(d3d, info);
         if (ret)
            RARCH_LOG("[D3D9]: Recovered from dead state.\n");
      }

#ifdef HAVE_MENU
      menu_driver_init(info->is_threaded);
#endif
   }

   if (!ret)
      return ret;

   if (!d3d9_cg_init_chain(d3d, info->input_scale, info->rgb32))
   {
      RARCH_ERR("[D3D9]: Failed to initialize render chain.\n");
      return false;
   }

   video_driver_get_size(&width, &height);
   d3d9_set_viewport(d3d,
      width, height, false, true);

   font_driver_init_osd(d3d, info,
         false,
         info->is_threaded,
         FONT_DRIVER_RENDER_D3D9_API);

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
      if (!d3d9_vertex_declaration_new(d3d->dev,
               (void*)VertexElements, (void**)&d3d->menu_display.decl))
         return false;
   }

   d3d->menu_display.offset = 0;
   d3d->menu_display.size   = 1024;
   d3d->menu_display.buffer = d3d9_vertex_buffer_new(
         d3d->dev, d3d->menu_display.size * sizeof(Vertex),
         D3DUSAGE_WRITEONLY,
         D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1,
         D3DPOOL_DEFAULT,
         NULL);

   if (!d3d->menu_display.buffer)
      return false;

   d3d_matrix_identity(&d3d->mvp_transposed);
   d3d_matrix_ortho_off_center_lh(&d3d->mvp_transposed, 0, 1, 0, 1, 0, 1);
   d3d_matrix_transpose(&d3d->mvp, &d3d->mvp_transposed);

   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_CULLMODE, D3DCULL_NONE);
   IDirect3DDevice9_SetRenderState(d3d->dev, D3DRS_SCISSORTESTENABLE, TRUE);

   return true;
}

static bool d3d9_cg_restore(d3d9_video_t *d3d)
{
   d3d9_cg_deinitialize(d3d);

   if (!d3d9_cg_initialize(d3d, &d3d->video_info))
   {
      RARCH_ERR("[D3D9]: Restore error.\n");
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

   if (!string_is_empty(d3d->shader_path))
      free(d3d->shader_path);
   d3d->shader_path = NULL;

   switch (type)
   {
      case RARCH_SHADER_CG:
      case RARCH_SHADER_HLSL:
         if (!string_is_empty(path))
            d3d->shader_path = strdup(path);
         break;
      case RARCH_SHADER_NONE:
         break;
      default:
         RARCH_WARN("[D3D9]: Only Cg shaders are supported. Falling back to stock.\n");
   }

   if (!d3d9_process_shader(d3d) || !d3d9_cg_restore(d3d))
   {
      RARCH_ERR("[D3D9]: Failed to set shader.\n");
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

   full_x                = (windowed_full || info->width  == 0) ?
      (mon_rect.right  - mon_rect.left) : info->width;
   full_y                = (windowed_full || info->height == 0) ?
      (mon_rect.bottom - mon_rect.top)  : info->height;
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

      RARCH_LOG("[D3D9]: Using GPU: \"%s\".\n", ident.Description);
      RARCH_LOG("[D3D9]: GPU API Version: %s\n", version_str);

      video_driver_set_gpu_device_string(ident.Description);
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
      RARCH_ERR("[D3D9]: Failed to init D3D.\n");
      free(d3d);
      return NULL;
   }

   d3d->keep_aspect       = info->force_aspect;

   return d3d;
}

static void d3d9_cg_free(void *data)
{
   d3d9_video_t   *d3d = (d3d9_video_t*)data;

   if (!d3d)
      return;

#ifdef HAVE_OVERLAY
   d3d9_free_overlays(d3d);
   if (d3d->overlays)
      free(d3d->overlays);
   d3d->overlays      = NULL;
   d3d->overlays_size = 0;
#endif

   d3d9_free_overlay(d3d, d3d->menu);
   if (d3d->menu)
      free(d3d->menu);
   d3d->menu          = NULL;

   d3d9_cg_deinitialize(d3d);

   if (!string_is_empty(d3d->shader_path))
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
   bool menu_is_alive                  = video_info->menu_is_alive;
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
         RARCH_ERR("[D3D9]: Failed to restore.\n");
         return false;
      }
   }

   if (d3d->should_resize)
   {
      cg_renderchain_t   *_chain = (cg_renderchain_t*)
         d3d->renderchain_data;
      d3d9_renderchain_t *chain  = (d3d9_renderchain_t*)&_chain->chain;
      d3d9_set_viewport(d3d, width, height, false, true);

      if (chain)
         chain->final_viewport = (D3DVIEWPORT9*)&d3d->final_viewport;

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

   IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev,
         0, (const float*)&d3d->mvp, 4);
   d3d9_cg_renderchain_render(
            d3d, frame, frame_width, frame_height,
            pitch, d3d->dev_rotation);
   
   if (black_frame_insertion && !d3d->menu->enabled)
   {
      int n;
      for (n = 0; n < video_info->black_frame_insertion; ++n) 
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
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev,
         0, (const float*)&d3d->mvp, 4);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_MENU
   if (d3d->menu && d3d->menu->enabled)
   {
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev,
         0, (const float*)&d3d->mvp, 4);
      d3d9_overlay_render(d3d, width, height, d3d->menu, false);

      d3d->menu_display.offset = 0;
      IDirect3DDevice9_SetVertexDeclaration(d3d->dev, (LPDIRECT3DVERTEXDECLARATION9)d3d->menu_display.decl);
      IDirect3DDevice9_SetStreamSource(d3d->dev, 0,
            (LPDIRECT3DVERTEXBUFFER9)d3d->menu_display.buffer,
            0, sizeof(Vertex));

      IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
      menu_driver_frame(menu_is_alive, video_info);
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
      IDirect3DDevice9_SetVertexShaderConstantF(d3d->dev,
         0, (const float*)&d3d->mvp, 4);
      for (i = 0; i < d3d->overlays_size; i++)
         d3d9_overlay_render(d3d, width, height, &d3d->overlays[i], true);
   }
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (msg && *msg)
   {
      IDirect3DDevice9_SetViewport(d3d->dev, (D3DVIEWPORT9*)&screen_vp);
      IDirect3DDevice9_BeginScene(d3d->dev);
      font_driver_render_msg(d3d, msg, NULL, NULL);
      IDirect3DDevice9_EndScene(d3d->dev);
   }

   win32_update_title();
   IDirect3DDevice9_Present(d3d->dev, NULL, NULL, NULL, NULL);

   return true;
}

static const video_poke_interface_t d3d9_cg_poke_interface = {
   d3d9_cg_get_flags,
   d3d9_load_texture,
   d3d9_unload_texture,
   d3d9_set_video_mode,
#if defined(__WINRT__)
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
   d3d9_set_aspect_ratio,
   d3d9_apply_state_changes,
   d3d9_set_menu_texture_frame,
   d3d9_set_menu_texture_enable,
   d3d9_set_osd_msg,

   win32_show_cursor,
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL,                         /* get_hw_render_interface */
   NULL,                         /* set_hdr_max_nits */
   NULL,                         /* set_hdr_paper_white_nits */
   NULL,                         /* set_hdr_contrast */
   NULL                          /* set_hdr_expand_gamut */
};

static void d3d9_cg_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &d3d9_cg_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool d3d9_cg_gfx_widgets_enabled(void *data)
{
   return false; /* currently disabled due to memory issues */
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

video_driver_t video_d3d9_cg = {
   d3d9_cg_init,
   d3d9_cg_frame,
   d3d9_cg_set_nonblock_state,
   d3d9_cg_alive,
   NULL,                      /* focus */
   win32_suppress_screensaver,
   d3d9_has_windowed,
   d3d9_cg_set_shader,
   d3d9_cg_free,
   "d3d9_cg",
   d3d9_set_viewport,
   d3d9_set_rotation,
   d3d9_viewport_info,
   d3d9_read_viewport,
   NULL,                      /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d9_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
   d3d9_cg_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   d3d9_cg_gfx_widgets_enabled
#endif
};
